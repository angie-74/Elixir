// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The main loop is Mocktail's (src/legacy/legacy_runtime.cc, Apache-2.0).
// Modified by Elixir.
#include "elixir_runtime/game_session.h"

#include <windows.h>

#include <jni.h>
#include <mcpelauncher/linker.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "bionic/runtime.h"
#include "elixir_runtime/text.h"
#include "elixir_runtime/host_log.h"
#include "elixir_runtime/roblox_startup.h"
#include "elixir_runtime/stall_profiler.h"
#include "jnivm/jnivm.h"
#include "runtime/display_size.h"
#include "runtime/roblox_capability_resolver.h"
#include "runtime/roblox_game_session_native_adapter.h"
#include "runtime/roblox_text_input_jni_bridge.h"
#include "runtime/roblox_window_input_runtime.h"
#include "window/win32_game_window.h"
#include "window/window.h"

#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
#include "android/android.h"
#endif
#if defined(ELIXIR_RUNTIME_HAS_AUDIO_JNI)
#include "audio/wasapi_audio.h"
#include "mocktail/audio/webrtc_jni_audio_bridge.h"
#endif
#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
#include "graphics/vulkan/guest_vulkan.h"
#endif
#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
#include "elixir_runtime/experience_session.h"
#endif

namespace elixir::runtime {
namespace {

std::int64_t NowMicroseconds() {
  LARGE_INTEGER frequency{}, counter{};
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&counter);
  return counter.QuadPart / frequency.QuadPart * 1000000 + counter.QuadPart % frequency.QuadPart * 1000000 /
                                                                 frequency.QuadPart;
}

struct PresentPacing {
  std::atomic<std::int64_t> last_us{0};
  std::atomic<std::int64_t> average_us{0};
  std::atomic<std::int64_t> max_gap_us{0};
  std::atomic<std::uint32_t> stalls{0};
  std::array<std::atomic<std::int64_t>, 8> stall_at_us{};
};
PresentPacing g_pacing;
std::atomic<DWORD> g_present_thread{0};

void NotePresentPacing() {
  g_present_thread.store(GetCurrentThreadId(), std::memory_order_relaxed);
  const std::int64_t now = NowMicroseconds();
  const std::int64_t last = g_pacing.last_us.exchange(now, std::memory_order_relaxed);
  if (last == 0) return;
  const std::int64_t gap = now - last;
  const std::int64_t average = g_pacing.average_us.load(std::memory_order_relaxed);
  if (gap > g_pacing.max_gap_us.load(std::memory_order_relaxed)) g_pacing.max_gap_us.store(gap, std::memory_order_relaxed);
  if (average > 0 && average < 20000 && gap >= 40000 && gap > 3 * average) {
    const std::uint32_t index = g_pacing.stalls.fetch_add(1, std::memory_order_relaxed);
    g_pacing.stall_at_us[index % g_pacing.stall_at_us.size()].store(now, std::memory_order_relaxed);
  }
  const std::int64_t sample = std::min<std::int64_t>(gap, 100000);
  g_pacing.average_us.store(average == 0 ? sample : (average * 15 + sample) / 16, std::memory_order_relaxed);
}

std::string PacingReport(std::int64_t session_start_us) {
  const std::int64_t max_gap = g_pacing.max_gap_us.exchange(0, std::memory_order_relaxed);
  const std::uint32_t stalls = g_pacing.stalls.exchange(0, std::memory_order_relaxed);
  if (max_gap == 0) return {};
  std::string report = "; frame gaps: max " + std::to_string(max_gap / 1000) + " ms";
  if (stalls > 0) {
    report += ", " + std::to_string(stalls) + (stalls == 1 ? " stall" : " stalls") + " over 40 ms (at";
    const std::uint32_t shown = std::min<std::uint32_t>(stalls, static_cast<std::uint32_t>(g_pacing.stall_at_us.size()));
    for (std::uint32_t i = 0; i < shown; ++i) {
      const std::int64_t at = g_pacing.stall_at_us[i].load(std::memory_order_relaxed);
      char text[32];
      std::snprintf(text, sizeof(text), " %.1f", static_cast<double>(at - session_start_us) / 1e6);
      report += text;
    }
    report += std::string(stalls > shown ? " ..." : "") + " s)";
  }
  return report;
}


#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
void OnVulkanPresent(VkResult result) {
  if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
    mocktail::window::NoteVulkanPresent();
    NotePresentPacing();
  } else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    mocktail::window::NoteVulkanSurfaceOutOfDate();
  }
}
#endif

std::uint64_t PresentedFrames() {
#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
  return vulkan::PresentedFrameCount();
#else
  return 0;
#endif
}

std::string ReferenceReport() {
  const jnivm::ReferenceStatistics references = jnivm::GetReferenceStatistics();
  return "JNI: " + std::to_string(references.objects) + " objects, " + std::to_string(references.arrays) +
         " arrays, " + std::to_string(references.handle_slots) + " of " + std::to_string(references.handle_capacity) +
         " handle slots, " + std::to_string(references.thread_local_refs) + " main-thread local references";
}

void PublishWindowSize(std::uint32_t width, std::uint32_t height) {
  const std::string text = std::to_string(width) + "x" + std::to_string(height);
  _putenv_s(mocktail::runtime::kWindowSizeEnvironment, text.c_str());
}

void* ResolveRobloxSymbol(void* context, const char* name) { return linker::dlsym(context, name); }

void RestoreJniFunctions(void* context) { static_cast<jnivm::VM*>(context)->RestoreFunctions(); }

}

bool GameSession::StartInput(void* roblox, jnivm::VM* vm, bool controllers, std::string* error) {
  namespace mr = mocktail::runtime;
  const mr::RobloxSymbolLookup lookup(&ResolveRobloxSymbol, roblox);
  const mr::RobloxCapabilityResolution resolution =
      mr::ResolveRobloxCapabilities(lookup, mr::RobloxCapabilityRequirements{false});
  if (!resolution.ok()) {
    std::string names;
    for (const std::string& name : resolution.missing_required_symbols()) names += " " + name;
    if (error != nullptr) *error = "libroblox.so lacks natives Roblox's capability check requires:" + names;
    return false;
  }
  const mr::RobloxCapabilities& capabilities = *resolution.capabilities();
  const mr::RobloxInputCapabilityFlags& flags = capabilities.input_flags;
  if (!flags.has_mouse && !flags.has_hardware_keyboard) {
    if (error != nullptr) *error = "libroblox.so exports no mouse or keyboard natives";
    return false;
  }
  const mr::JniEnvironmentProvider environment{vm->GetJavaVM(), vm, &RestoreJniFunctions};
  auto input = std::make_shared<mr::RobloxWindowInputRuntime>(environment, capabilities.input);
  input->ConfigureControllers(controllers);
  mocktail::Status status = input->Initialize();
  if (!status.ok()) {
    if (error != nullptr) *error = "the window input runtime did not start: " + status.message();
    return false;
  }
  std::unique_ptr<mr::RobloxTextInputJniBridge> text;
  status = mr::RobloxTextInputJniBridge::Create(vm, input, &text);
  if (!status.ok()) {
    static_cast<void>(input->Shutdown());
    if (error != nullptr) *error = "the text input bridge did not start: " + status.message();
    return false;
  }
  input_ = std::move(input);
  text_input_ = std::move(text);
  Log(std::string("input: mouse ") + (flags.has_mouse ? "yes" : "no") + ", keyboard " +
      (flags.has_hardware_keyboard ? "yes" : "no") + ", text " + (flags.has_text_input ? "yes" : "no") +
      ", controllers " + (controllers ? (flags.has_gamepad_events ? "yes" : "no natives") : "off"));
  return true;
}

void GameSession::StopInput() {
  if (text_input_ != nullptr) {
    static_cast<void>(text_input_->Shutdown());
    text_input_.reset();
  }
  if (input_ != nullptr) {
    static_cast<void>(input_->Shutdown());
    input_.reset();
  }
}

#if defined(ELIXIR_RUNTIME_HAS_AUDIO_JNI)
void GameSession::SetMicrophonePicker(mocktail::audio::WebRtcMicrophonePicker picker) {
  if (window_ == nullptr) return;
  struct MenuState {
    mocktail::audio::WebRtcMicrophonePicker picker;
    std::wstring feedback;
  };
  const auto menu = std::make_shared<MenuState>(MenuState{std::move(picker), {}});
  if (!window_->SetMicrophoneMenu(
          [menu]() -> std::vector<window::MicrophoneMenuItem> {
            namespace audio = mocktail::audio;
            if (!audio::MicrophoneCapturePermitted()) return {{L"Microphone disabled in Elixir Settings", {}}};
            if (!menu->picker.select) return {{L"Microphone unavailable", {}}};
            std::vector<audio::WasapiAudioDevice> devices;
            if (!audio::ListWasapiRecordingDevices(&devices).ok()) return {{L"Could not list Windows microphones", {}}};
            if (devices.empty()) return {{L"No Windows microphones connected", {}}};

            std::wstring selected;
            std::string selected_name;
            if (!audio::GetConfiguredWasapiRecordingDevice(&selected, &selected_name).ok()) {
              return {{L"Microphone unavailable", {}}};
            }
            bool default_available = false;
            bool selected_available = false;
            for (const audio::WasapiAudioDevice& device : devices) {
              default_available |= device.is_default;
              selected_available |= device.id == selected;
            }
            if (selected.empty()) selected_available = default_available;
            const bool has_selection = menu->picker.route_configured && selected_available;
            std::vector<window::MicrophoneMenuItem> items;
            if (!menu->feedback.empty()) items.push_back({menu->feedback, {}});
            if (!has_selection) items.push_back({L"Configured microphone unavailable", {}});
            items.push_back({L"System Default", {}, default_available, has_selection && selected.empty()});
            for (audio::WasapiAudioDevice& device : devices) {
              const bool checked = has_selection && device.id == selected;
              items.push_back({WideFromUtf8(device.name), std::move(device.id), true, checked});
            }
            return items;
          },
          [menu](std::wstring_view endpoint) {
            std::string resolved_name;
            const mocktail::Status status = menu->picker.select(endpoint, &resolved_name);
            if (!status.ok()) {
              menu->feedback = L"Could not switch: " + WideFromUtf8(status.message());
              Log("microphone menu: " + status.message());
              return;
            }
            menu->picker.route_configured = true;
            menu->feedback.clear();
            Log("microphone menu: selected a Windows input for this session");
          })) {
    Log("microphone menu: could not add the window menu");
  }
}
#endif

GameSession::GameSession() = default;

GameSession::~GameSession() {
  if (window_ != nullptr) Close(nullptr);
}

bool GameSession::Open(const GameSessionOptions& options, std::string* error) {
  window::GameWindowOptions window_options;
  window_options.width = options.width;
  window_options.height = options.height;
  window_options.mode = window::WindowMode::kWindowed;
  window_options.title = options.title;
  window_options.start_hidden = true;
  window_options.offscreen = options.offscreen;
  window_options.state_path = options.state_path;
  auto created = std::make_unique<window::GameWindow>();
  const mocktail::Status status = created->Create(window_options);
  if (!status.ok()) {
    if (error != nullptr) *error = "cannot create the game window: " + status.message();
    return false;
  }
  window::SetCurrentGameWindow(created.get());
  window_ = std::move(created);
  const window::GameWindowSnapshot snapshot = window_->Snapshot();
  PublishWindowSize(snapshot.pixel_width, snapshot.pixel_height);
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
  native_window_ = android::CreateNativeWindow(window_->hwnd(), static_cast<int>(snapshot.pixel_width),
                                               static_cast<int>(snapshot.pixel_height));
  if (native_window_ == nullptr) {
    if (error != nullptr) *error = "cannot create the ANativeWindow for the game window";
    Close(nullptr);
    return false;
  }
#endif
#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
  vulkan::SetPresentHook(&OnVulkanPresent);
#endif
  opened_ms_ = GetTickCount64();
  Log("game window " + std::to_string(snapshot.pixel_width) + "x" + std::to_string(snapshot.pixel_height) +
      " px, DPI scale " + std::to_string(snapshot.dpi_scale) + ", hidden until Roblox presents a frame");
  return true;
}

bool ApplyFrameRateCap(unsigned cap, float* current_hz, std::vector<float>* supported_hz) {
  const float wanted = static_cast<float>(cap);
  if (cap == 0 || *current_hz <= 0.0f || wanted <= *current_hz + 0.5f) return false;
  *current_hz = wanted;
  const auto place = std::lower_bound(supported_hz->begin(), supported_hz->end(), wanted);
  if (place == supported_hz->end() || *place != wanted) supported_hz->insert(place, wanted);
  return true;
}

namespace {

bool MonitorRefresh(HWND window, HMONITOR* monitor, unsigned* hz) {
  *monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
  MONITORINFOEXW info{};
  info.cbSize = sizeof(info);
  if (*monitor == nullptr || !GetMonitorInfoW(*monitor, &info)) return false;
  DEVMODEW mode{};
  mode.dmSize = sizeof(mode);
  if (!EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) || mode.dmDisplayFrequency <= 1) return false;
  *hz = mode.dmDisplayFrequency;
  return true;
}

}

GameSessionGeometry GameSession::Geometry() const {
  GameSessionGeometry geometry;
  if (window_ == nullptr) return geometry;
  const window::GameWindowSnapshot snapshot = window_->Snapshot();
  geometry.width = static_cast<int>(snapshot.pixel_width);
  geometry.height = static_cast<int>(snapshot.pixel_height);
  geometry.dpi_scale = snapshot.dpi_scale;
  const mocktail::platform::DisplayRefreshCapabilities refresh = mocktail::window::GetDisplayRefreshCapabilities();
  if (refresh.valid()) {
    geometry.refresh_hz = refresh.current_hz;
    geometry.supported_refresh_hz = refresh.supported_hz;
    geometry.monitor_hz = refresh.current_hz;
    ApplyFrameRateCap(frame_rate_cap_, &geometry.refresh_hz, &geometry.supported_refresh_hz);
  }
  return geometry;
}

void GameSession::FollowDisplayRefresh(void* roblox, jnivm::VM* vm) {
  refresh_roblox_ = roblox;
  refresh_vm_ = vm;
  HMONITOR monitor = nullptr;
  unsigned hz = 0;
  if (window_ != nullptr && MonitorRefresh(window_->hwnd(), &monitor, &hz)) {
    refresh_monitor_ = monitor;
    refresh_hz_ = hz;
  }
}

void GameSession::CheckDisplayRefresh() {
  if (refresh_roblox_ == nullptr || refresh_vm_ == nullptr || window_ == nullptr) return;
  HMONITOR monitor = nullptr;
  unsigned hz = 0;
  if (!MonitorRefresh(window_->hwnd(), &monitor, &hz)) return;
  if (monitor == refresh_monitor_ && hz == refresh_hz_) return;
  const bool rate_changed = hz != refresh_hz_;
  refresh_monitor_ = monitor;
  refresh_hz_ = hz;
  if (!rate_changed) return;
  mocktail::platform::DisplayRefreshCapabilities refresh = mocktail::window::GetDisplayRefreshCapabilities();
  if (!refresh.valid()) return;
  const float monitor_hz = refresh.current_hz;
  const bool capped = ApplyFrameRateCap(frame_rate_cap_, &refresh.current_hz, &refresh.supported_hz);
  Log("display: the game window's monitor now refreshes at " + std::to_string(static_cast<int>(monitor_hz + 0.5f)) +
      " Hz; telling Roblox " + DescribeRefreshRates(refresh.current_hz, refresh.supported_hz) +
      (capped ? " (its frame-rate cap is above the monitor's rate)" : ""));
  if (!PassDisplayRefreshRates(refresh_roblox_, refresh_vm_, refresh.current_hz, refresh.supported_hz)) {
    Log("display: libroblox.so does not take refresh rates; Roblox keeps the old one");
  }
}

void GameSession::PumpOnce() {
  if (window_ == nullptr) return;
  window_->Pump();
  HandleEvents();
}

void GameSession::HandleEvents() {
  window::HostWindowEvent event;
  while (window_->PollHostEvent(&event)) {
    if (event.type == window::HostWindowEventType::kResized) {
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
      android::ResizeNativeWindow(static_cast<ANativeWindow*>(native_window_), static_cast<int>(event.pixel_width),
                                  static_cast<int>(event.pixel_height));
#endif
      PublishWindowSize(event.pixel_width, event.pixel_height);
    }
    if (event.type != window::HostWindowEventType::kResized) {
      Log(std::string("game window: ") + window::HostWindowEventTypeName(event.type) + " (" +
          std::to_string(event.pixel_width) + "x" + std::to_string(event.pixel_height) + ")");
    }
  }
  if (experience_ == nullptr) {
    mocktail::window::WindowSurfaceEvent surface_event;
    while (window_->PollSurfaceEvent(&surface_event)) {
    }
  }
  if (!first_frame_logged_ && window_->HasPresentedFrame()) NoteFirstFrame();
}

void GameSession::NoteFirstFrame() {
  first_frame_logged_ = true;
  const std::string detail = "first frame presented " + std::to_string(GetTickCount64() - opened_ms_) +
                             " ms after the window was created (" + std::to_string(PresentedFrames()) +
                             " presents so far)";
  Log(detail);
  EmitEvent(1, "first-frame", detail);
}

GameSessionEnd GameSession::Run(const std::atomic<bool>& stop, std::uint64_t time_limit_ms,
                                const std::atomic<bool>* restart) {
  const ULONGLONG start = GetTickCount64();
  const std::int64_t start_us = NowMicroseconds();
  static_cast<void>(PacingReport(start_us));
  StallProfiler profiler(&g_pacing.last_us, &g_pacing.average_us, &g_present_thread, &NowMicroseconds);
  profiler.Start();
  ULONGLONG last_report = start;
  ULONGLONG last_display_check = start;
  std::uint64_t frames_at_report = PresentedFrames();
  GameSessionEnd end = GameSessionEnd::kStopped;
  for (;;) {
    if (stop.load()) {
      end = GameSessionEnd::kStopped;
      break;
    }
    if (restart != nullptr && restart->load()) {
      end = GameSessionEnd::kRestart;
      break;
    }
    if (!window_->Pump()) {
      end = GameSessionEnd::kClosed;
      break;
    }
    HandleEvents();
#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
    if (experience_ != nullptr && !experience_->DrainSurfaceEvents()) {
      end = GameSessionEnd::kFailed;
      break;
    }
#endif
    RunPendingMainThreadWork();
#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
    if (experience_ != nullptr && (!experience_->DrainPlatformEvents() || !experience_->DrainLaunchRequests())) {
      end = GameSessionEnd::kFailed;
      break;
    }
#endif
    const ULONGLONG now = GetTickCount64();
    if (time_limit_ms != 0 && now - start >= time_limit_ms) {
      end = GameSessionEnd::kTimeLimit;
      break;
    }
    if (now - last_display_check >= 1000) {
      last_display_check = now;
      CheckDisplayRefresh();
    }
    if (now - last_report >= 10000) {
      const std::uint64_t frames = PresentedFrames();
      Log("session: " + std::to_string(frames - frames_at_report) + " frames in the last " +
          std::to_string((now - last_report) / 1000) + " s (" + std::to_string(frames) + " in total)" +
          PacingReport(start_us) + "; " + ReferenceReport());
      for (const std::string& line : profiler.Report()) Log(line);
      const std::string futex = bionic::FutexStatisticsReport();
      if (!futex.empty()) Log("session: " + futex);
      const std::string mapping = bionic::MmStatisticsReport();
      if (!mapping.empty()) Log("session: " + mapping);
      frames_at_report = frames;
      last_report = now;
    }
    window_->PaceInputPump();
  }
  profiler.Stop();
  Log("session: " + ReferenceReport());
  return end;
}

bool GameSession::presented() const { return window_ != nullptr && window_->HasPresentedFrame(); }

std::uint64_t GameSession::frames() const { return PresentedFrames(); }

void GameSession::Close(void* surface) {
  StopInput();
#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
  vulkan::SetPresentHook(nullptr);
#endif
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
  if (surface != nullptr) android::AssociateSurface(surface, nullptr);
  if (native_window_ != nullptr) android::DestroyNativeWindow(static_cast<ANativeWindow*>(native_window_));
#endif
  native_window_ = nullptr;
  if (window_ != nullptr) {
    window_->Destroy();
    window_.reset();
  }
}

}
