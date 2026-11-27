// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/runtime_main.h"

#include <windows.h>

#include <mcpelauncher/linker.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#include "abi/sysv_abi.h"
#include "bionic/raw_syscalls.h"
#include "runtime/mod_paths.h"
#include "elf/guest_image_check.h"
#include "elf/stack_guard_rewrite.h"
#include "elixir_runtime/guest_environment.h"
#include "elixir_runtime/guest_symbolizer.h"
#include "elixir_runtime/host_log.h"
#include "elixir_runtime/import_check.h"
#include "elixir_runtime/link_joins.h"
#include "elixir_runtime/payload.h"
#include "elixir_runtime/process_tuning.h"
#include "elixir_runtime/roblox_session_store.h"
#include "elixir_runtime/runtime_flags.h"
#include "elixir_runtime/text.h"
#include "elixir_runtime/winhttp_client.h"
#include "fflags/fflags.h"
#include "platform/windows/crash_reporter.h"
#include "platform/windows/session_marker.h"
#include "platform/windows/guest_tls.h"
#include "platform/windows/raw_syscall_trap.h"
#include "runtime/crash_report_policy.h"
#include "runtime/device_profile.h"
#include "runtime/display_size.h"
#include "runtime/environment.h"
#include "runtime/memory_limit.h"
#include "runtime/process_diagnostics.h"
#include "runtime/session_log.h"
#include "runtime/texture_memory_policy.h"
#include "services/client_settings_service.h"

#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
#include "elixir_runtime/gpu_memory.h"
#endif

#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
#include "android/android.h"
#endif

#if defined(ELIXIR_RUNTIME_HAS_WINDOW)
#include "elixir_runtime/game_session.h"
#include "window/window.h"
#endif
#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
#include "elixir_runtime/discord_presence.h"
#include "elixir_runtime/experience_session.h"
#endif
#if defined(ELIXIR_RUNTIME_HAS_AUDIO_JNI)
#include "audio/wasapi_audio.h"
#include "mocktail/audio/fmod_jni_audio_bridge.h"
#include "mocktail/audio/webrtc_jni_audio_bridge.h"
#endif

#if defined(ELIXIR_RUNTIME_HAS_JNI)
#include <jni.h>

#include <functional>

#include "elixir_runtime/account_book.h"
#include "elixir_runtime/account_use.h"
#include "elixir_runtime/app_restart.h"
#include "elixir_runtime/kept_cookies.h"
#include "elixir_runtime/roblox_startup.h"
#include "elixir_runtime/sign_in_identity.h"
#include "jnivm/jnivm.h"

extern "C" {
extern void* mocktail_gameactivity_on_start_native;
extern void* mocktail_gameactivity_on_resume_native;
extern void* mocktail_gameactivity_on_surface_created_native;
extern void* mocktail_gameactivity_on_surface_changed_native;
extern void* mocktail_gameactivity_on_surface_redraw_needed_native;
extern void* mocktail_gameactivity_on_trim_memory_native;
}
#endif

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "unknown"
#endif

namespace elixir::runtime {
namespace {

namespace mr = mocktail::runtime;

class CommandLineEnvironment final : public mr::Environment {
 public:
  explicit CommandLineEnvironment(const LaunchOptions& options) {
    if (!options.renderer.empty()) values_["MOCKTAIL_GRAPHICS_BACKEND"] = "direct-vulkan";
    if (!options.present_mode.empty()) values_["MOCKTAIL_PRESENT_MODE"] = options.present_mode;
    if (!options.audio_device.empty()) values_["MOCKTAIL_AUDIO_OUTPUT_DEVICE"] = options.audio_device;
    const std::string host_input = process_.GetOr("ELIXIR_AUDIO_INPUT_DEVICE", "");
    const std::string legacy_input = process_.GetOr("MOCKTAIL_AUDIO_INPUT_DEVICE", "");
    if (host_input == "disabled" || legacy_input == "disabled") {
      values_["MOCKTAIL_AUDIO_INPUT_DEVICE"] = "disabled";
    } else if (!options.audio_input_device.empty()) {
      values_["MOCKTAIL_AUDIO_INPUT_DEVICE"] = options.audio_input_device;
    } else if (!host_input.empty()) {
      values_["MOCKTAIL_AUDIO_INPUT_DEVICE"] = host_input;
    }
  }

  std::optional<std::string> Get(std::string_view name) const override {
    const auto found = values_.find(std::string(name));
    if (found != values_.end()) return found->second;
    return process_.Get(name);
  }

 private:
  std::map<std::string, std::string> values_;
  mr::ProcessEnvironment process_;
};

std::filesystem::path Absolute(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::path absolute = std::filesystem::absolute(path, error);
  return error ? path : absolute.lexically_normal();
}

void PrintUsageError(const std::string& error) {
  std::fprintf(stderr, "elixir-runtime: %s\nRun 'elixir-runtime --help' for the supported options.\n",
               error.c_str());
  std::fflush(stderr);
}

using SessionFile = std::unique_ptr<void, decltype(&CloseHandle)>;

DWORD HoldReadOnlyGlobalSettings(const std::filesystem::path& android_data, SessionFile& held) {
  const auto directory = android_data / L"com.roblox.client" / L"files" / L"appData";
  const auto file = directory / L"GlobalBasicSettings_13.xml";
  const DWORD attributes = GetFileAttributesW(file.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_READONLY) ||
      (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) return ERROR_SUCCESS;
  for (auto dir = directory; dir != android_data.parent_path(); dir = dir.parent_path()) {
    const DWORD part = GetFileAttributesW(dir.c_str());
    if (part == INVALID_FILE_ATTRIBUTES || (part & FILE_ATTRIBUTE_REPARSE_POINT)) return ERROR_SUCCESS;
  }
  const HANDLE file_handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                        FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (file_handle == INVALID_HANDLE_VALUE) {
    const DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? ERROR_SUCCESS : error;
  }
  held.reset(file_handle);
  BY_HANDLE_FILE_INFORMATION info{};
  if (!GetFileInformationByHandle(file_handle, &info)) return GetLastError();
  if (!(info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ||
      (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || info.nNumberOfLinks != 1) {
    held.reset();
  }
  return ERROR_SUCCESS;
}

int Fail(int code, const std::string& message) {
  LogError(message);
  EmitEvent(2, "error", message);
  return code;
}

std::atomic<bool> g_stop_requested{false};

struct LoadedModule {
  std::string name;
  std::uintptr_t base;
};

int ELIXIR_SYSV CollectModule(struct dl_phdr_info* info, std::size_t, void* data) noexcept {
  struct Prefix {
    std::uintptr_t addr;
    const char* name;
  };
  const auto* prefix = reinterpret_cast<const Prefix*>(info);
  auto* modules = static_cast<std::vector<LoadedModule>*>(data);
  try {
    modules->push_back({prefix->name != nullptr ? prefix->name : "", prefix->addr});
  } catch (...) {
  }
  return 0;
}

void LogGuestCodeCheck() {
  const ULONGLONG started = GetTickCount64();
  const std::vector<::elixir::elf::GuestCodeCheck> checks = ::elixir::elf::CheckGuestCodeAgainstFiles();
  std::uint64_t compared = 0;
  std::size_t wrong = 0;
  std::ostringstream details;
  for (const ::elixir::elf::GuestCodeCheck& check : checks) {
    compared += check.compared_bytes;
    if (!check.unreadable && check.differing_bytes == 0) continue;
    ++wrong;
    details << "\n  " << check.path << ": ";
    if (check.differing_bytes != 0) {
      details << check.differing_bytes << " byte(s) on " << check.differing_pages
              << " page(s) differ from the file, the first at 0x" << std::hex << check.first_difference << std::dec;
    }
    if (check.unreadable) details << (check.differing_bytes != 0 ? "; " : "") << "could not be read in full";
  }
  const std::string took = std::to_string(GetTickCount64() - started) + " ms";
  if (checks.empty()) {
    Log("guest code: no image to check");
  } else if (wrong == 0) {
    Log("guest code: " + std::to_string(checks.size()) + " image(s) run exactly as they are in their files (" +
        std::to_string(compared) + " bytes of executable segments compared in " + took + ")");
  } else {
    Log("guest code: " + std::to_string(wrong) + " of " + std::to_string(checks.size()) +
        " image(s) do NOT match their files (checked in " + took + "):" + details.str());
  }
}

#if defined(ELIXIR_RUNTIME_HAS_JNI)
bool Disabled(const mr::Environment& environment, std::string_view name) {
  const std::string value = environment.GetOr(name, "");
  return value == "0" || value == "false" || value == "off" || value == "no";
}

struct ClientSettings {
  std::string json;
  std::string source;
};

ClientSettings ResolveClientSettings(const mr::Environment& environment, const mr::RuntimePaths& paths) {
  mocktail::services::ClientSettingsOptions options;
  options.explicit_json = environment.GetOr("MOCKTAIL_CLIENT_SETTINGS_JSON", "");
  if (const std::string file = environment.GetOr("MOCKTAIL_CLIENT_SETTINGS_JSON_FILE", ""); !file.empty()) {
    options.explicit_file = PathFromUtf8(file);
  }
  options.sober_mode = true;
  options.fetch = !Disabled(environment, "MOCKTAIL_FETCH_CLIENT_SETTINGS");
  options.auto_update = !Disabled(environment, "MOCKTAIL_CLIENT_SETTINGS_AUTO_UPDATE");
  options.application = environment.GetOr("MOCKTAIL_CLIENT_SETTINGS_APP", "GoogleAndroidApp");
  options.url = environment.GetOr("MOCKTAIL_CLIENT_SETTINGS_URL", "");
  const std::string cache = environment.GetOr("MOCKTAIL_CLIENT_SETTINGS_CACHE_FILE", "");
  options.cache_file = !cache.empty() ? PathFromUtf8(cache)
                                      : paths.cache_root() / L"clientsettings" / PathFromUtf8(options.application + ".json");
  WinHttpClient http;
  mocktail::services::ClientSettingsService service(http);
  const mocktail::services::ClientSettingsResult result = service.Resolve(options);
  using mocktail::services::ClientSettingsSource;
  ClientSettings settings;
  settings.json = result.json;
  switch (result.source) {
    case ClientSettingsSource::kExplicitJson: settings.source = "MOCKTAIL_CLIENT_SETTINGS_JSON"; break;
    case ClientSettingsSource::kExplicitFile: settings.source = NativeUtf8(options.explicit_file); break;
    case ClientSettingsSource::kBundledFile: settings.source = NativeUtf8(options.bundled_file); break;
    case ClientSettingsSource::kSafeDefaults: settings.source = "Mocktail's safe defaults"; break;
    case ClientSettingsSource::kDownloaded:
      settings.source = std::string("Roblox's CDN (") + (result.cache_updated ? "cache updated" : "cache unchanged") + ")";
      break;
    case ClientSettingsSource::kCache: settings.source = "the cache " + NativeUtf8(options.cache_file); break;
    case ClientSettingsSource::kEmptyDefaults: settings.source = "empty defaults"; break;
  }
  if (!result.error.empty()) settings.source += "; " + result.error;
  return settings;
}
#endif

#if defined(ELIXIR_RUNTIME_HAS_WINDOW)
bool QueueAndroidWindowFlags(void*, int flags, int mask) {
  return mocktail::window::RequestFullscreenFromAndroidWindowFlags(flags, mask);
}

std::string WindowsAppTheme() {
  DWORD value = 0;
  DWORD size = sizeof(value);
  if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS &&
      value != 0) {
    return "Light";
  }
  return "Dark";
}
#endif

mr::DisplaySize PrimaryDisplaySize() {
  DEVMODEW mode{};
  mode.dmSize = sizeof(mode);
  if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmPelsWidth > 0 && mode.dmPelsHeight > 0) {
    return {static_cast<std::int32_t>(mode.dmPelsWidth), static_cast<std::int32_t>(mode.dmPelsHeight)};
  }
  return {};
}

}

static bool HoldDataFolder(const std::filesystem::path& data_root) {
  std::error_code error;
  std::wstring key = std::filesystem::weakly_canonical(data_root, error).wstring();
  if (error || key.empty()) key = data_root.wstring();
  std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
  std::uint64_t hash = 1469598103934665603ull;
  for (const wchar_t c : key) {
    hash ^= static_cast<std::uint16_t>(c);
    hash *= 1099511628211ull;
  }
  static std::mutex held_mutex;
  static std::vector<std::uint64_t> held;
  std::lock_guard<std::mutex> lock(held_mutex);
  if (std::find(held.begin(), held.end(), hash) != held.end()) return true;
  wchar_t name[64];
  std::swprintf(name, 64, L"Local\\Elixir.Roblox.%016llx", static_cast<unsigned long long>(hash));
  const HANDLE mutex = CreateMutexW(nullptr, FALSE, name);
  if (mutex == nullptr) return true;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(mutex);
    return false;
  }
  held.push_back(hash);
  static const HANDLE marker = CreateEventW(nullptr, TRUE, FALSE, ::elixir::kRobloxSessionMarkerName);
  static_cast<void>(marker);
  return true;
}

RuntimeSettings ResolveRuntimeSettings(const LaunchOptions& options) {
  RuntimeSettings settings;
  const CommandLineEnvironment environment(options);
  const std::filesystem::path executable_directory = mr::RuntimePaths::CurrentExecutableDirectory();
  bool portable = false;
  std::filesystem::path data_root;
  if (!options.data_dir.empty()) {
    data_root = Absolute(options.data_dir);
  } else {
    data_root = mr::RuntimePaths::ResolveElixirDataRoot(environment, executable_directory, &portable);
    if (!data_root.empty()) data_root = Absolute(data_root);
  }
  if (data_root.empty()) {
    settings.exit_code = kExitConfiguration;
    settings.error = "cannot find the Elixir data folder (%LOCALAPPDATA% is not available); pass --data-dir <folder>";
    return settings;
  }
  settings.paths = mr::RuntimePaths::FromElixirDataRoot(data_root, executable_directory, portable);
  if (!options.config_file.empty()) settings.paths.set_config_file(Absolute(options.config_file));
  if (!options.crash_dir.empty()) settings.paths.set_crashes_root(Absolute(options.crash_dir));
  if (!options.guest_runtime_dir.empty()) settings.paths.set_guest_runtime_root(Absolute(options.guest_runtime_dir));

  settings.config = mr::LoadRuntimeConfig(environment, settings.paths.config_file());
  if (!settings.config) {
    settings.exit_code = kExitConfiguration;
    settings.error = "configuration (" + NativeUtf8(settings.paths.config_file()) + "): " + settings.config.error;
    return settings;
  }
  if (!options.fflags_file.empty()) {
    settings.fflags_file = Absolute(options.fflags_file);
  } else if (!settings.config.launcher.fflags_path.empty()) {
    settings.fflags_file = Absolute(PathFromUtf8(settings.config.launcher.fflags_path));
  } else {
    settings.fflags_file = settings.paths.fflags_file();
  }
  settings.paths.set_fflags_file(settings.fflags_file);
  settings.user_fflags = options.user_fflags.value_or(true);
  return settings;
}

int BuildRuntimeFlagsDocument(const RuntimeSettings& settings, std::string* json, std::string* error,
                              UserFlags* user_out, elixir::fflags::Resolution* resolution_out,
                              std::string* gpu_report) {
  const mr::RuntimeConfig& config = settings.config.config;
  RuntimeFlagInputs inputs;
  inputs.frame_rate = mr::FrameRatePolicy{};
  inputs.performance = config.performance();
  inputs.microphone_enabled = config.microphone_enabled();
  inputs.host_memory_bytes = mr::DetectHostMemoryBytes();
#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
  if (!config.headless()) {
    if (const auto gpu = DetectVulkanGpuMemory()) {
      inputs.gpu_adapter_name = gpu->adapter_name;
      inputs.gpu_memory_bytes = gpu->bytes;
      inputs.gpu_memory_source = gpu->source == VulkanGpuMemorySource::kDedicatedVideoMemory
                                     ? "dedicated video memory" : "UMA local memory budget";
    }
  }
#endif
  inputs.desktop_playability = config.desktop_playability();
  const RuntimeFlagSets sets = BuildRuntimeFlagSets(inputs);
  if (!sets.ok()) {
    *error = sets.error;
    return kExitConfiguration;
  }
  const UserFlags user = settings.user_fflags ? LoadUserFlags(settings.fflags_file) : UserFlags{};
  const elixir::fflags::Allowlist allowlist = elixir::fflags::Allowlist::Builtin();
  const elixir::fflags::Resolution resolution =
      elixir::fflags::ResolveOverrides(sets.mandatory, sets.defaults, user.flags, allowlist);
  *json = RuntimeFlagsJson(inputs, sets, user, allowlist, resolution, config.device_profile().name);
  if (gpu_report != nullptr) {
    const std::string* applied = resolution.effective.Find("FIntRenderForceVideoMemorySize");
    if (applied != nullptr && inputs.gpu_memory_bytes != 0) {
      *gpu_report = "graphics memory: " + inputs.gpu_adapter_name + ", " + inputs.gpu_memory_source + " = " +
                    std::to_string(inputs.gpu_memory_bytes) + " bytes; Roblox budget = " + *applied + " bytes";
    } else {
      *gpu_report = "graphics memory: using Roblox's default budget";
    }
  }
  if (user_out != nullptr) *user_out = user;
  if (resolution_out != nullptr) *resolution_out = resolution;
  return kExitOk;
}

int RunElixirRuntime(const std::vector<std::string>& arguments) {
  const CommandLineResult parsed = ParseRuntimeCommandLine(arguments);
  if (!parsed.ok()) {
    PrintUsageError(parsed.error);
    return kExitUsage;
  }
  const LaunchOptions& options = parsed.options;
  if (options.mode == RuntimeMode::kHelp) {
    std::fputs(RuntimeUsage().c_str(), stdout);
    std::fflush(stdout);
    return kExitOk;
  }
  if (options.mode == RuntimeMode::kVersion) {
    std::fputs(RuntimeVersionText().c_str(), stdout);
    std::fflush(stdout);
    return kExitOk;
  }

  const RuntimeSettings settings = ResolveRuntimeSettings(options);
  if (settings.exit_code != kExitOk) {
    std::fprintf(stderr, "elixir-runtime: %s\n", settings.error.c_str());
    std::fflush(stderr);
    return settings.exit_code;
  }
  const mr::RuntimePaths& paths = settings.paths;
  const mr::RuntimeConfigLoadResult& config = settings.config;

  if (options.mode == RuntimeMode::kRun && !HoldDataFolder(paths.data_root())) {
    const std::string message = "another Roblox window is already using " + NativeUtf8(paths.data_root());
    std::fprintf(stderr, "elixir-runtime: %s\n", message.c_str());
    std::fflush(stderr);
    EmitEvent(2, "error", message);
    return kExitConfiguration;
  }

  if (options.mode == RuntimeMode::kPrintRuntimeFlags) {
    std::string json;
    std::string error;
    const int code = BuildRuntimeFlagsDocument(settings, &json, &error);
    if (code != kExitOk) {
      std::fprintf(stderr, "elixir-runtime: runtime Fast Flags: %s\n", error.c_str());
      return code;
    }
    std::fwrite(json.data(), 1, json.size(), stdout);
    std::fflush(stdout);
    return kExitOk;
  }

  const bool run_mode = options.mode == RuntimeMode::kRun;
  ResetConnectionDiagnostics();
  mr::SessionLog session;
  if (run_mode && options.session_log) {
    const CommandLineEnvironment environment(options);
    session = mr::SessionLog::Start(environment, paths);
    if (session.active()) {
      std::fputs(session.Header(environment, paths, config.config.graphics_backend_name()).c_str(), stderr);
    } else if (session.attempted()) {
      LogError("session log unavailable: " + session.error());
    }
  }
  Log(std::string("elixir-runtime ") + ELIXIR_VERSION + " mode=" + (run_mode ? "run" : "check-imports") +
      " data=" + NativeUtf8(paths.data_root()) + (paths.portable() ? " (portable)" : ""));
  Log("config " + NativeUtf8(paths.config_file()) + (config.file_loaded ? " loaded" : " absent; using defaults") +
      ", device profile " + config.config.device_profile().name + " (" +
      std::string(mr::DeviceClassName(config.config.device_profile().device_class)) + ")");
  for (const std::string& warning : config.warnings) Log("config warning: " + warning);
  if (run_mode) mr::LogProcessDiagnostics(mr::ProcessDiagnosticStage::kStartup);

  const bool timer_1ms = options.timer_resolution_1ms.value_or(config.launcher.timer_resolution_1ms);
  SetMmcssEnabled(options.mmcss.value_or(config.launcher.mmcss));
  std::optional<TimerResolutionScope> timer;
  if (run_mode) {
    Log(ApplyPriorityClass(options.priority).detail);
    if (options.power_throttling) Log(SetPowerThrottlingOptOut(!*options.power_throttling).detail);
    Log("power throttling: " + CurrentPowerThrottlingState());
    Log("main thread " + EnsureThreadStackGuarantee(64 * 1024).detail);
    timer.emplace(timer_1ms);
    Log(std::string("timer resolution: ") +
        (timer->active() ? "1 ms while running" : timer_1ms ? "timeBeginPeriod(1) failed" : "system default"));
    Log(std::string("MMCSS for render/audio threads: ") + (MmcssEnabled() ? "on" : "off"));
  }

  StartGuestSymbolizer();
  {
    platform::CrashReporterOptions crash;
    crash.directory = paths.crashes_root().native();
    crash.write_minidump = true;
    crash.product_version = std::string("elixir-runtime ") + ELIXIR_VERSION;
    crash.guest_symbolizer = [](std::uintptr_t address, platform::SymbolInfo* info) {
      return SymbolizeGuestAddress(address, info);
    };
    platform::InstallCrashReporter(crash);
  }
  mr::MemoryLimitWatchdog memory_watchdog;
  struct JoinedThread {
    std::thread thread;
    ~JoinedThread() {
      if (thread.joinable()) thread.join();
    }
  } guest_code_check;
  const auto shutdown = [&](int code) {
    if (guest_code_check.thread.joinable()) guest_code_check.thread.join();
    memory_watchdog.Stop();
    StopGuestSymbolizer();
    if (timer) timer->Release();
    if (run_mode) mr::LogProcessDiagnostics(mr::ProcessDiagnosticStage::kShutdown);
    if (run_mode) {
      Log("guest thread pointer: " + std::to_string(platform::FsRestoreCount()) + " lazy FS restore(s) this session");
      const ::elixir::platform::RawSyscallStats raw = ::elixir::platform::GetRawSyscallStats();
      std::string direct = ::elixir::bionic::RawSyscallSummary();
      if (raw.known_functions != 0 || !direct.empty()) {
        if (direct.empty()) direct = "direct system calls: none made";
        Log(direct + "; " + std::to_string(raw.watched_functions) + " of " + std::to_string(raw.known_functions) +
            " function(s) watched, " + std::to_string(raw.stops) + " breakpoint(s), " + std::to_string(raw.traced) +
            " instruction(s) run one at a time, " + std::to_string(raw.unwatched) + " stop(s) without a register");
      }
    }
    Log("exit code " + std::to_string(code));
    session.Finish();
    return code;
  };
  if (run_mode && config.config.performance().memory_limit_enabled()) {
    std::string error;
    if (!memory_watchdog.Start(config.config.performance().memory_limit_bytes(), &error)) {
      return shutdown(Fail(kExitGuestRuntime, "memory limit: " + error));
    }
    Log("memory limit: commit-charge watchdog at " + std::to_string(config.config.performance().memory_limit_mb) +
        " MiB (MOCKTAIL_MEMORY_LIMIT_MB); past it the runtime ends with exit code 137");
  }

  PayloadRequest request;
  request.roblox_library = options.roblox_library;
  request.app_dir = !options.app_dir.empty() ? options.app_dir : PathFromUtf8(config.launcher.installed_install_dir);
  request.expected_package = !options.package.empty() ? options.package : config.launcher.installed_package;
  if (options.version_code) {
    request.expected_version_code = *options.version_code;
  } else if (options.app_dir.empty() && !config.launcher.installed_install_dir.empty()) {
    request.expected_version_code = config.launcher.installed_version_code;
  }
  const PayloadResolution payload = ResolveInstalledPayload(request);
  if (!payload.ok()) return shutdown(Fail(kExitPayloadMissing, payload.error));
  const PayloadLocation& location = payload.location;
  EmitEvent(1, "payload",
            (location.package.empty() ? std::string("?") : location.package) + " versionCode " +
                (location.version_code >= 0 ? std::to_string(location.version_code) : std::string("?")));
  Log("payload " + NativeUtf8(location.roblox_library) + (location.development_override ? " (--roblox-lib)" : "") +
      " package=" + (location.package.empty() ? std::string("?") : location.package) +
      " versionCode=" + (location.version_code >= 0 ? std::to_string(location.version_code) : std::string("?")) +
      " build-id=" + (location.build_id.empty() ? std::string("none") : location.build_id));

#if defined(ELIXIR_RUNTIME_HAS_JNI)
  struct Fetch {
    std::thread thread;
    ~Fetch() {
      if (thread.joinable()) thread.detach();
    }
  } fetch;
  auto client_settings_promise = std::make_shared<std::promise<ClientSettings>>();
  std::future<ClientSettings> client_settings = client_settings_promise->get_future();
  if (run_mode) {
    fetch.thread = std::thread([client_settings_promise, options, paths] {
      const CommandLineEnvironment environment(options);
      client_settings_promise->set_value(ResolveClientSettings(environment, paths));
    });
  }
#endif

  GuestEnvironmentOptions guest;
  guest.package_name = !location.package.empty() ? location.package
                       : !options.package.empty() ? options.package
                                                  : std::string("com.roblox.client");
  guest.data_root = paths.data_root();
  guest.app_dir = location.app_dir;
  {
    std::error_code mods_dir_error;
    if (!options.mods_dir.empty() && std::filesystem::is_directory(options.mods_dir, mods_dir_error)) {
      guest.mods_dir = options.mods_dir;
    }
  }
  guest.guest_runtime_dir = paths.guest_runtime_root();
  guest.library_search_path = {location.library_dir, paths.guest_runtime_root()};
  SessionFile locked_settings(nullptr, &CloseHandle);
  if (run_mode && guest.package_name == "com.roblox.client") {
    const DWORD error = HoldReadOnlyGlobalSettings(paths.android_data_root(), locked_settings);
    if (error != ERROR_SUCCESS) {
      return shutdown(Fail(kExitConfiguration, "cannot protect the read-only global settings (Windows error " +
                                                   std::to_string(error) + ")"));
    }
  }
  const GuestEnvironmentStatus& environment = EnsureGuestEnvironment(guest);
  if (!environment.ok) return shutdown(Fail(environment.exit_code, environment.error));
  Log("guest libc: " + std::to_string(environment.export_count) + " host exports in " +
      std::to_string(environment.synthetic_libraries) + " synthetic libraries" +
      (environment.vulkan_installed ? ", libvulkan.so with " + std::to_string(environment.vulkan_exports) + " exports"
                                    : std::string(", no libvulkan.so (built without Vulkan)")));
  Log(environment.libc_extension_loaded
          ? "libc extension: " + std::to_string(environment.libc_extension_exports) + " functions merged into libc.so"
          : "libc extension: " + environment.libc_extension_note);
  for (const std::string& mount : environment.mounts) Log("mount " + mount);
  Log("library search path " + environment.library_search_path);
  EmitEvent(1, "android-environment",
            std::to_string(environment.synthetic_libraries) + " synthetic libraries, " +
                std::to_string(environment.export_count) + " host exports");

  if (options.mode == RuntimeMode::kCheckImports) {
    const ImportCheckReport report = CheckImports(location.roblox_library, guest.library_search_path);
    const std::string text = FormatImportCheckReport(report, location.roblox_library);
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
    if (!report.error.empty()) return shutdown(kExitPayloadMissing);
    return shutdown(report.ok() ? kExitOk : kExitUnresolvedImports);
  }

  std::string effective_flags_json = "{}";
  {
    std::string json;
    std::string error;
    UserFlags user;
    elixir::fflags::Resolution resolution;
    std::string gpu_report;
    if (BuildRuntimeFlagsDocument(settings, &json, &error, &user, &resolution, &gpu_report) != kExitOk) {
      return shutdown(Fail(kExitConfiguration, "runtime Fast Flags: " + error));
    }
    if (!user.error.empty()) {
      return shutdown(Fail(kExitConfiguration, "cannot load Fast Flags from " + NativeUtf8(settings.fflags_file) +
                                                   ": " + user.error));
    }
    Log("fast flags: " + std::to_string(user.flags.size()) + " user flag(s) from " +
        NativeUtf8(settings.fflags_file) + (user.loaded ? "" : " (no file)") + ", " +
        std::to_string(resolution.applied_count) + " applied; " + std::to_string(resolution.effective.size()) +
        " flags in the effective set");
    Log(gpu_report);
    for (const elixir::fflags::FlagReport& report : resolution.user) {
      if (report.status == elixir::fflags::FlagStatus::kApplied) continue;
      Log("fast flag " + report.name + " not applied: " + elixir::fflags::FlagStatusName(report.status) +
          (report.detail.empty() ? std::string() : " (" + report.detail + ")"));
    }
    effective_flags_json = elixir::fflags::SerializeFlagsJson(resolution.effective);
  }
  std::string fast_flags_json;
  {
    std::string error;
    if (!mr::MergeCrashReportFastFlagsOverrides("{}", &fast_flags_json, &error)) {
      return shutdown(Fail(kExitConfiguration, "crash-report fast-flags policy: " + error));
    }
  }

  [[maybe_unused]] const std::string asset_path = "/data/app/" + guest.package_name + "/assets/content";
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
  if (!location.app_dir.empty()) {
    android::AssetSources sources;
    std::error_code error;
    const bool extracted = std::filesystem::is_directory(location.app_dir / L"assets" / L"content", error);
    if (extracted) sources.directories.push_back(GenericUtf8(location.app_dir / L"assets"));
    std::error_code mods_error;
    const bool mods = !options.mods_dir.empty() && std::filesystem::is_directory(options.mods_dir, mods_error);
    if (mods) {
      sources.overlay_directory = GenericUtf8(options.mods_dir);
      sources.overlay_filter = &::elixir::mods::IsReplaceable;
    }
    const std::filesystem::path base_apk = location.app_dir / L"base.apk";
    if (std::filesystem::is_regular_file(base_apk, error)) sources.archives.push_back(GenericUtf8(base_apk));
    std::string asset_error;
    if ((!sources.directories.empty() || !sources.archives.empty()) &&
        !android::SetAssetSources(sources, &asset_error)) {
      return shutdown(Fail(kExitPayloadLoad, "cannot read the app's assets: " + asset_error));
    }
    Log("assets: " + std::string(extracted ? "extracted folder " + NativeUtf8(location.app_dir / L"assets") + ", "
                                           : "no extracted folder, ") +
        (sources.archives.empty() ? std::string("no base.apk") : NativeUtf8(base_apk)) + "; asset path " + asset_path +
        (extracted ? std::string() : " (not extracted)"));
    if (mods) {
      Log("mods: files in " + NativeUtf8(options.mods_dir) +
          " replace Roblox's textures, sounds, fonts and sky images of the same path");
    }
    const char* configured_assets = std::getenv("MOCKTAIL_ASSET_PATH");
    if (extracted && (configured_assets == nullptr || configured_assets[0] == '\0')) {
      _putenv_s("MOCKTAIL_ASSET_PATH", NativeUtf8(location.app_dir / L"assets" / L"content").c_str());
    }
  }
#endif

  const mr::DisplaySize display = PrimaryDisplaySize();
  const std::string display_text = std::to_string(display.width) + "x" + std::to_string(display.height);
  _putenv_s(mr::kDisplaySizeEnvironment, display_text.c_str());
  _putenv_s(mr::kWindowSizeEnvironment, display_text.c_str());
  if (!location.version_name.empty()) _putenv_s("MOCKTAIL_ROBLOX_VERSION", location.version_name.c_str());
  _wputenv_s(L"ELIXIR_WEBVIEW_DATA_DIR", (paths.data_root() / L"webview").c_str());
  if (location.version_code >= 0) {
    _putenv_s("MOCKTAIL_ROBLOX_VERSION_CODE", std::to_string(location.version_code).c_str());
  }


  if (g_stop_requested.load()) {
    Log("stop requested: ending before libroblox.so is loaded");
    EmitEvent(3, "stopped", "before libroblox.so was loaded");
    return shutdown(kExitOk);
  }
  EmitEvent(1, "loading", "libroblox.so and its libraries");
  if (const char* proc_trace = std::getenv("ELIXIR_PROC_TRACE"); proc_trace != nullptr && proc_trace[0] == '1') {
    Log("proc tracing on (ELIXIR_PROC_TRACE=1): every /proc open and read is logged");
  }
  platform::EnsureGuestThread();
  const ULONGLONG load_start = GetTickCount64();
  void* handle = linker::dlopen(GenericUtf8(location.roblox_library).c_str(), RTLD_NOW);
  if (handle == nullptr) {
    const char* error = linker::dlerror();
    return shutdown(Fail(kExitPayloadLoad, std::string("cannot load ") + NativeUtf8(location.roblox_library) + ": " +
                                               (error != nullptr ? error : "dlopen failed") +
                                               "\nRun elixir-runtime --check-imports to list what is missing."));
  }
  std::vector<LoadedModule> modules;
  linker::dl_iterate_phdr(
      reinterpret_cast<int(ELIXIR_GUEST_CC*)(struct dl_phdr_info*, std::size_t, void*)>(&CollectModule), &modules);
  std::ostringstream loaded;
  loaded << "loaded " << kRobloxLibraryName << " in " << (GetTickCount64() - load_start) << " ms; "
         << modules.size() << " modules mapped:";
  for (const LoadedModule& module : modules) {
    if (!module.name.empty()) loaded << "\n  " << module.name;
  }
  Log(loaded.str());
  EmitEvent(1, "loaded", "libroblox.so in " + std::to_string(GetTickCount64() - load_start) + " ms");
  if (::elixir::elf::TotalStackGuardRewrites() != 0) {
    Log("guest code: ELIXIR_FS_REWRITE=1 rewrote " + std::to_string(::elixir::elf::TotalStackGuardRewrites()) +
        " stack-guard read(s) in memory; this run does not execute the code that is in the files");
  }
  guest_code_check.thread = std::thread([] {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    LogGuestCodeCheck();
  });
#if defined(ELIXIR_RUNTIME_HAS_JNI)
  const auto on_load = reinterpret_cast<abi::SysvFn<jint(JavaVM*, void*)>>(linker::dlsym(handle, "JNI_OnLoad"));
  if (on_load == nullptr) return shutdown(Fail(kExitPayloadLoad, "libroblox.so does not export JNI_OnLoad"));
  if (g_stop_requested.load()) {
    Log("stop requested: ending before JNI_OnLoad");
    EmitEvent(3, "stopped", "before JNI_OnLoad");
    return shutdown(kExitOk);
  }
  bool present_policy_is_default = false;
  {
    const auto unset = [](const char* name) {
      const char* value = std::getenv(name);
      return value == nullptr || value[0] == '\0';
    };
    if (!options.present_mode.empty()) {
      _putenv_s("ELIXIR_PRESENT_MODE", options.present_mode.c_str());
    } else if (unset("ELIXIR_PRESENT_MODE") && unset("ELIXIR_VSYNC")) {
      _putenv_s("ELIXIR_VSYNC", "on");
      present_policy_is_default = true;
    }
  }
  for (const std::string& area : options.trace) {
    if (area == "jni") {
      _putenv_s("MOCKTAIL_JNI_TRACE", "1");
      _putenv_s("MOCKTAIL_JNI_VM_TRACE", "1");
    } else if (area == "jni-strings") {
      _putenv_s("MOCKTAIL_JNI_STRING_TRACE", "1");
    } else if (area == "window") {
      _putenv_s("MOCKTAIL_WINDOW_TRACE", "1");
    }
    Log("trace: " + area);
  }
  static jnivm::VM* const java_vm = new jnivm::VM();
  std::vector<std::string> guest_split_paths;
  for (const std::string& filename : location.split_apks) {
    guest_split_paths.push_back("/data/app/" + guest.package_name + "/" + filename);
  }
  java_vm->SetApplicationSplitSourceDirs(guest_split_paths);
  java_vm->SetEnsureDirectoryQuery(&EnsureAndroidDirectory);
  RegisterRobloxJavaClasses(java_vm);
  const mr::DeviceProfile& device = config.config.device_profile();
  const mr::InputCapabilityConfig& input = config.config.input_capabilities();
  java_vm->SetPlatformIdentity(jnivm::PlatformIdentity{
      input.touch_enabled, input.mouse_enabled, input.keyboard_enabled, device.pc_hardware, device.platform_name,
      device.display_name, device.manufacturer, device.model, device.brand, device.device_code, device.device_sku,
      device.soc_model});
  const auto session_store = std::make_shared<RobloxSessionStore>(paths.auth_root() / L"roblox-session.dpapi");
  SignInIdentity::Options sign_in_options;
  sign_in_options.http_client = std::make_shared<WinHttpClient>();
  sign_in_options.log = [](std::string_view line) { Log(line); };
  sign_in_options.store = session_store;
  sign_in_options.accounts = std::make_shared<AccountBook>(
      options.accounts_dir.empty() ? paths.auth_root() / L"accounts" : Absolute(options.accounts_dir));
  const auto account_use = std::make_shared<AccountUse>();
  sign_in_options.account_changed = [account_use](long long user_id) { account_use->SetAccount(user_id); };
  const std::unique_ptr<SignInIdentity> sign_in = SignInIdentity::Install(*java_vm, std::move(sign_in_options));
  KeptCookies::Options kept_cookie_options;
  kept_cookie_options.store = std::make_shared<RobloxSessionStore>(paths.auth_root() / L"roblox-cookies.dpapi",
                                                                   RobloxSessionStore::Content::kCookies);
  kept_cookie_options.log = [](std::string_view line) { Log(line); };
  const std::shared_ptr<KeptCookies> kept_cookies = KeptCookies::Install(*java_vm, std::move(kept_cookie_options));
  AppRestart::Options restart_options;
  restart_options.log = [](std::string_view line) { Log(line); };
  const std::shared_ptr<AppRestart> restart = AppRestart::Install(*java_vm, std::move(restart_options));
  if (const std::string after = mr::ProcessEnvironment().GetOr("ELIXIR_RESTART_TEST_MS", ""); !after.empty()) {
    SetEnvironmentVariableW(L"ELIXIR_RESTART_TEST_MS", nullptr);
    const unsigned long delay = std::strtoul(after.c_str(), nullptr, 10);
    std::thread([restart, delay] {
      Sleep(delay);
      static_cast<void>(restart->Request(RestartCause::kAccountSwitched));
    }).detach();
  }
  if (sign_in == nullptr) {
    Log("sign-in: the account lookup is unavailable (its thread could not be started)");
  } else {
    std::string restored;
    switch (session_store->Load(&restored)) {
      case RobloxSessionStore::LoadResult::kFound:
        sign_in->AcceptRestored(restored);
        if (java_vm->DispatchRobloxCredential(restored.data(), restored.size())) {
          Log("sign-in: a saved Roblox session was restored");
        } else {
          Log("sign-in: the saved Roblox session could not be restored");
        }
        break;
      case RobloxSessionStore::LoadResult::kMissing:
        Log("sign-in: no saved Roblox session");
        break;
      case RobloxSessionStore::LoadResult::kUnreadable:
        Log("sign-in: the saved Roblox session could not be read (another Windows account or PC); sign in again");
        break;
    }
    if (!restored.empty()) SecureZeroMemory(restored.data(), restored.size());
    restored.clear();
  }
#if defined(ELIXIR_RUNTIME_HAS_AUDIO_JNI)
  java_vm->SetMicrophoneAvailabilityQuery(
      config.config.headless() ? nullptr : &mocktail::audio::HasWasapiCaptureEndpoint);
  mocktail::audio::SetMicrophoneCapturePermitted(config.config.microphone_enabled());
  if (const mocktail::Status audio = mocktail::audio::InstallFmodJniAudioBridge(
          java_vm, config.config.audio_output_device(), true);
      !audio.ok()) {
    Log("audio output unavailable: " + audio.message());
  }
  const std::string& input_device = config.config.audio_input_device();
  mocktail::audio::WebRtcMicrophonePicker microphone_picker;
  const bool allow_input_device_name = options.audio_input_device.empty() &&
                                       input_device != config.launcher.audio_input_device_id;
  if (const mocktail::Status voice = mocktail::audio::InstallWebRtcJniAudioBridge(
          java_vm, input_device, allow_input_device_name, &microphone_picker);
      !voice.ok()) {
    Log("voice audio unavailable: " + voice.message());
  }
#endif
#if defined(ELIXIR_RUNTIME_HAS_WINDOW)
  java_vm->SetAndroidWindowCallbacks(std::make_shared<int>(0), jnivm::AndroidWindowCallbacks{&QueueAndroidWindowFlags});
#endif
  java_vm->GetJNIEnv();
  EmitEvent(1, "jni", "JNI_OnLoad");
  const ULONGLONG jni_start = GetTickCount64();
  const jint jni_version = on_load(java_vm->GetJavaVM(), nullptr);
  const ULONGLONG jni_ms = GetTickCount64() - jni_start;
  java_vm->RestoreFunctions();
  if (jni_version != JNI_VERSION_1_2 && jni_version != JNI_VERSION_1_4 && jni_version != JNI_VERSION_1_6) {
    char text[64];
    std::snprintf(text, sizeof(text), "%d (0x%08x)", static_cast<int>(jni_version),
                  static_cast<unsigned>(jni_version));
    return shutdown(Fail(kExitPayloadLoad, std::string("JNI_OnLoad of libroblox.so returned ") + text +
                                               ", which is not a JNI version Android accepts"));
  }
  const std::string jni_text = "JNI " + std::to_string(jni_version >> 16) + "." + std::to_string(jni_version & 0xffff);
  const std::pair<const char*, void*> activity_natives[] = {
      {"onStartNative", mocktail_gameactivity_on_start_native},
      {"onResumeNative", mocktail_gameactivity_on_resume_native},
      {"onSurfaceCreatedNative", mocktail_gameactivity_on_surface_created_native},
      {"onSurfaceChangedNative", mocktail_gameactivity_on_surface_changed_native},
      {"onSurfaceRedrawNeededNative", mocktail_gameactivity_on_surface_redraw_needed_native},
      {"onTrimMemoryNative", mocktail_gameactivity_on_trim_memory_native}};
  std::string registered, missing;
  for (const auto& [name, address] : activity_natives) (address != nullptr ? registered : missing) += std::string(" ") + name;
  Log("JNI_OnLoad returned " + jni_text + " in " + std::to_string(jni_ms) + " ms; GameActivity natives registered:" +
      (registered.empty() ? std::string(" none") : registered) +
      (missing.empty() ? std::string() : "; not registered:" + missing));
  EmitEvent(1, "jni-loaded", "JNI_OnLoad returned " + jni_text + " in " + std::to_string(jni_ms) + " ms");

  {
    std::error_code error;
    if (location.app_dir.empty() ||
        !std::filesystem::is_directory(location.app_dir / L"assets" / L"content", error)) {
      return shutdown(Fail(kExitPayloadMissing,
                           "the installed app has no extracted assets (" +
                               NativeUtf8(location.app_dir / L"assets" / L"content") +
                               "): Roblox reads its content from that folder. Elixir.exe extracts it from the app's "
                               "base.apk when it installs the app, and before Play for an installation that does not "
                               "have it yet."));
    }
  }

  RobloxStartupOptions startup;
  startup.package_name = guest.package_name;
  startup.version_name = location.version_name;
  startup.asset_path = asset_path;
  {
    const ULONGLONG wait_start = GetTickCount64();
    const ClientSettings fetched = client_settings.get();
    fetch.thread.join();
    const ULONGLONG waited = GetTickCount64() - wait_start;
    startup.client_settings_json = fetched.json;
    Log("client settings: " + std::to_string(fetched.json.size()) + " bytes from " + fetched.source +
        (waited > 0 ? "; start-up waited " + std::to_string(waited) + " ms for them" : std::string()));
  }
  startup.client_settings_overrides_json = effective_flags_json;
  startup.fast_flags_json = fast_flags_json;
  startup.user_agent = config.config.roblox_http_user_agent().value_or(device.roblox_http_user_agent);
  startup.device = device;
  startup.input = input;
  startup.host_memory_bytes = mr::DetectHostMemoryBytes();
  startup.display = display;
  startup.window = display;
  startup.headless = config.config.headless();
  std::function<void()> pump;
#if defined(ELIXIR_RUNTIME_HAS_WINDOW)
  GameSession game;
#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
  std::unique_ptr<DiscordPresence> discord;
  if (!options.discord_app.empty() && !startup.headless) {
    DiscordPresenceOptions discord_options;
    discord_options.application_id = options.discord_app;
    discord_options.show_game = options.discord_game.value_or(true);
    discord_options.join_button = options.discord_join.value_or(false);
    discord_options.game_presence = options.discord_rpc.value_or(true);
    discord_options.http = std::make_shared<WinHttpClient>();
    discord = std::make_unique<DiscordPresence>(std::move(discord_options));
    discord->Start();
    Log("discord: Rich Presence is on; it shows once you're in an experience");
  }
  std::unique_ptr<ExperienceSession> experience;
#endif
  const bool windowed = !startup.headless;
  if (windowed) {
    GameSessionOptions window_options;
    window_options.width = config.config.window().width;
    window_options.height = config.config.window().height;
    window_options.title = PathFromUtf8(config.config.window().title).wstring();
    if (options.instance) window_options.title += L" (" + std::to_wstring(*options.instance) + L")";
    window_options.state_path = paths.state_root() / L"window_state.json";
    window_options.offscreen = mr::ProcessEnvironment().GetOr("ELIXIR_WINDOW_OFFSCREEN", "") == "1";
    std::string window_error;
    if (!game.Open(window_options, &window_error)) return shutdown(Fail(kExitGuestRuntime, window_error));
#if defined(ELIXIR_RUNTIME_HAS_AUDIO_JNI)
    game.SetMicrophonePicker(std::move(microphone_picker));
#endif
    game.SetFrameRateCap(options.frame_rate_cap > 0 ? static_cast<unsigned>(options.frame_rate_cap) : 0);
    const GameSessionGeometry geometry = game.Geometry();
    startup.native_window = game.native_window();
    startup.window = {geometry.width, geometry.height};
    startup.dpi_scale = geometry.dpi_scale;
    startup.refresh_hz = geometry.refresh_hz;
    startup.supported_refresh_hz = geometry.supported_refresh_hz;
    if (geometry.refresh_hz > geometry.monitor_hz && geometry.monitor_hz > 0.0f) {
      if (present_policy_is_default) _putenv_s("ELIXIR_VSYNC", "off");
      Log("display: the game window's monitor refreshes at " +
          std::to_string(static_cast<int>(geometry.monitor_hz + 0.5f)) + " Hz and Roblox's frame-rate cap is " +
          std::to_string(options.frame_rate_cap) + "; Roblox is told " +
          DescribeRefreshRates(geometry.refresh_hz, geometry.supported_refresh_hz) +
          (present_policy_is_default ? ", and frames are not held for the display" : ""));
    } else if (geometry.refresh_hz > 0.0f) {
      Log("display: the game window's monitor refreshes at " +
          DescribeRefreshRates(geometry.refresh_hz, geometry.supported_refresh_hz));
    }
    startup.theme = WindowsAppTheme();
    _putenv_s("MOCKTAIL_RESOLVED_THEME_INTERNAL", startup.theme.c_str());
    pump = [&game] { game.PumpOnce(); };
#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
    ExperienceSessionOptions experience_options;
    experience_options.asset_folder_path = startup.asset_path;
    experience_options.dpi_scale = geometry.dpi_scale;
    experience_options.touch_enabled = input.touch_enabled;
    experience_options.mouse_enabled = input.mouse_enabled;
    experience_options.keyboard_enabled = input.keyboard_enabled;
    experience_options.microphone_enabled = config.config.microphone_enabled();
    if (discord != nullptr) experience_options.presence = discord->observer();
    experience_options.in_game_changed = [account_use](bool in_game) { account_use->SetInGame(in_game); };
    std::string experience_unavailable;
    experience = ExperienceSession::Create(handle, java_vm, experience_options, &experience_unavailable);
    if (experience == nullptr) {
      Log("experience launch unavailable: " + experience_unavailable);
      if (RefuseLinkJoins() > 0) EmitEvent(3, "join", "rejected");
    }
    game.AttachExperience(experience.get());
#endif
  }
#endif
  EmitEvent(1, "roblox-startup", "Roblox's start-up natives");
  constexpr unsigned kStartupTimeoutMs = 120000;
  const RobloxStartupResult started =
      RunRobloxStartup(handle, java_vm, startup, g_stop_requested, kStartupTimeoutMs, pump);
  if (!started.missing.empty()) {
    std::string names;
    for (const std::string& name : started.missing) names += "\n  " + name;
    return shutdown(Fail(kExitPayloadLoad, started.error + " (is it a Roblox build?):" + names));
  }
  if (started.stopped) {
    Log("stop requested: Roblox's start-up ended before " + started.step);
    EmitEvent(3, "stopped", "during Roblox's start-up, before " + started.step);
    return shutdown(kExitOk);
  }
  if (started.timed_out) {
    const int code = shutdown(Fail(kExitGuestRuntime, started.error));
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
    return code;
  }
  if (!started.ok) return shutdown(Fail(kExitGuestRuntime, started.error));
  const char* last_step = startup.native_window != nullptr ? "nativeAppBridgeV2StartAppWithParams"
                                                           : "nativeAppBridgeV2InitWithParams";
  Log("Roblox's start-up natives returned: " + std::to_string(started.steps_run) + " steps in " +
      std::to_string(started.elapsed_ms) + " ms, through " + last_step);
  EmitEvent(1, "roblox-started", std::to_string(started.steps_run) + " start-up steps in " +
                                     std::to_string(started.elapsed_ms) + " ms");
#if defined(ELIXIR_RUNTIME_HAS_WINDOW)
  if (windowed) {
    bool experience_ready = true;
#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
    if (experience != nullptr) experience_ready = experience->OnRobloxStarted();
#endif
    game.FollowDisplayRefresh(handle, java_vm);
    std::string input_error;
    if (experience_ready &&
        !game.StartInput(handle, java_vm, options.controller.value_or(true), &input_error)) {
      Log("input unavailable: " + input_error);
    }
    const GameSessionEnd end =
        experience_ready
            ? game.Run(g_stop_requested, options.stop_after_ms ? static_cast<std::uint64_t>(*options.stop_after_ms) : 0,
                       &restart->requested())
            : GameSessionEnd::kFailed;
    const char* reason = end == GameSessionEnd::kClosed    ? "the game window was closed"
                         : end == GameSessionEnd::kStopped ? "stop requested"
                         : end == GameSessionEnd::kFailed  ? "the experience session failed"
                         : end == GameSessionEnd::kRestart ? RestartReason(restart->cause())
                                                           : "--stop-after reached";
    restart->Detach();
    Log(std::string("session ends: ") + reason);
    game.StopInput();
    static_cast<void>(java_vm->RefreshRobloxCredentialFromEngine(java_vm->GetJNIEnv()));
    if (kept_cookies != nullptr) kept_cookies->Detach();
    bool app_shut_down = false;
#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
    if (experience != nullptr) app_shut_down = experience->Shutdown();
#endif
    std::string shutdown_error;
    if (!app_shut_down && !ShutdownRobloxApp(handle, java_vm, &shutdown_error)) {
      Log("Roblox's lifecycle shutdown was skipped: " + shutdown_error);
    }
    java_vm->ClearAndroidWindowCallbacks();
#if defined(ELIXIR_RUNTIME_HAS_AUDIO_JNI)
    static_cast<void>(mocktail::audio::ShutdownWebRtcJniAudioBridge(java_vm));
    static_cast<void>(mocktail::audio::ShutdownFmodJniAudioBridge(java_vm));
#endif
#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
    game.AttachExperience(nullptr);
    experience.reset();
    if (discord != nullptr) discord->Stop();
#endif
    if (sign_in != nullptr) sign_in->Shutdown();
    const std::string frames = game.presented()
                                   ? "Roblox presented " + std::to_string(game.frames()) + " frames"
                                   : std::string("Roblox presented no frame");
    Log("session: " + frames);
    game.Close(started.surface);
    if (end == GameSessionEnd::kFailed) {
      EmitEvent(2, "error", "the game session ended unexpectedly");
      return shutdown(kExitGuestRuntime);
    }
    if (end == GameSessionEnd::kRestart) {
      EmitEvent(3, "restart", std::string(reason) + "; " + frames);
      return shutdown(kExitRestart);
    }
    EmitEvent(3, end == GameSessionEnd::kStopped ? "stopped" : "ended", std::string(reason) + "; " + frames);
    return shutdown(kExitOk);
  }
#endif
  Log(std::string("Roblox has run its start-up natives through ") + last_step +
      ". Without a window (headless) the session ends here. Stopping.");
  EmitEvent(3, "not-implemented",
            "Roblox's start-up natives have run; a headless session has no window, so Roblox stops here");
#else
  Log(std::string("JNI_OnLoad ") + (linker::dlsym(handle, "JNI_OnLoad") != nullptr ? "found" : "not exported"));
  Log("libroblox.so is loaded. JNI, the Android activity lifecycle, the window, rendering, input and audio "
      "are not implemented in elixir-runtime yet; Roblox does not run. Stopping.");
  EmitEvent(3, "not-implemented",
            "libroblox.so is loaded; the Java side and the activity lifecycle are not ported yet, so Roblox stops "
            "here");
#endif
  return shutdown(kExitNotImplemented);
}

void RequestStop() noexcept { g_stop_requested.store(true); }

}
