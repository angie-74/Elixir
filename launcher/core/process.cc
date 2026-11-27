// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/process.h"

#include "core/global_settings.h"
#include "platform/windows/session_marker.h"

#include "core/instances.h"

#include <sddl.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <thread>

#include "core/json.h"
#include "core/mods.h"
#include "core/util.h"
#include "core/i18n.h"

namespace elixir::launcher {

bool IsAnyRuntimeProcessRunning() {
  if (const HANDLE marker = OpenEventW(SYNCHRONIZE, FALSE, elixir::kRobloxSessionMarkerName)) {
    CloseHandle(marker);
    return true;
  }
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  bool found = false;
  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (_wcsicmp(entry.szExeFile, kRuntimeExecutable) == 0) {
        found = true;
        break;
      }
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return found;
}

std::wstring QuoteArgument(std::wstring_view argument) {
  if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
    return std::wstring(argument);
  }
  std::wstring quoted = L"\"";
  for (std::size_t i = 0;; ++i) {
    std::size_t backslashes = 0;
    while (i < argument.size() && argument[i] == L'\\') {
      ++i;
      ++backslashes;
    }
    if (i == argument.size()) {
      quoted.append(backslashes * 2, L'\\');
      break;
    }
    if (argument[i] == L'"') {
      quoted.append(backslashes * 2 + 1, L'\\');
      quoted.push_back(L'"');
    } else {
      quoted.append(backslashes, L'\\');
      quoted.push_back(argument[i]);
    }
  }
  quoted.push_back(L'"');
  return quoted;
}

std::wstring BuildCommandLine(const std::wstring& executable, const std::vector<std::wstring>& arguments) {
  std::wstring command = QuoteArgument(executable);
  for (const std::wstring& argument : arguments) {
    command.push_back(L' ');
    command += QuoteArgument(argument);
  }
  return command;
}

namespace {

constexpr std::size_t kMaxCapture = 16u << 20;

void AppendCapped(std::string* out, const char* data, std::size_t size) {
  if (out->size() >= kMaxCapture) return;
  out->append(data, size < kMaxCapture - out->size() ? size : kMaxCapture - out->size());
}

}

CaptureResult RunCaptured(const std::wstring& executable, const std::vector<std::wstring>& arguments,
                          const std::wstring& working_directory, const LineCallback& on_stdout_line,
                          const std::atomic<bool>* cancel, DWORD timeout_ms) {
  CaptureResult result;
  SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE out_read = nullptr, out_write = nullptr, err_read = nullptr, err_write = nullptr;
  if (!CreatePipe(&out_read, &out_write, &inheritable, 0)) {
    result.error = L"CreatePipe failed: " + Win32ErrorText(GetLastError());
    return result;
  }
  if (!CreatePipe(&err_read, &err_write, &inheritable, 0)) {
    result.error = L"CreatePipe failed: " + Win32ErrorText(GetLastError());
    CloseHandle(out_read);
    CloseHandle(out_write);
    return result;
  }
  SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0);
  HANDLE null_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable,
                                  OPEN_EXISTING, 0, nullptr);

  SIZE_T attribute_size = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
  std::vector<std::byte> attribute_buffer(attribute_size);
  auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_buffer.data());
  InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size);
  HANDLE inherited[3] = {out_write, err_write, null_input};
  const DWORD inherited_count = null_input != INVALID_HANDLE_VALUE ? 3 : 2;
  UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                            inherited_count * sizeof(HANDLE), nullptr, nullptr);

  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = null_input != INVALID_HANDLE_VALUE ? null_input : nullptr;
  startup.StartupInfo.hStdOutput = out_write;
  startup.StartupInfo.hStdError = err_write;
  startup.lpAttributeList = attributes;

  std::wstring command_line = BuildCommandLine(executable, arguments);
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(
      executable.c_str(), command_line.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, nullptr,
      working_directory.empty() ? nullptr : working_directory.c_str(), &startup.StartupInfo, &process);
  const DWORD create_error = GetLastError();
  DeleteProcThreadAttributeList(attributes);
  CloseHandle(out_write);
  CloseHandle(err_write);
  if (null_input != INVALID_HANDLE_VALUE) CloseHandle(null_input);
  if (!created) {
    CloseHandle(out_read);
    CloseHandle(err_read);
    result.error = L"Cannot start " + executable + L": " + Win32ErrorText(create_error);
    return result;
  }
  CloseHandle(process.hThread);
  result.started = true;

  std::thread stderr_reader([&] {
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(err_read, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
      AppendCapped(&result.stderr_text, buffer, read);
    }
  });
  std::thread stdout_reader([&] {
    char buffer[4096];
    DWORD read = 0;
    std::string pending;
    while (ReadFile(out_read, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
      AppendCapped(&result.stdout_text, buffer, read);
      if (!on_stdout_line) continue;
      pending.append(buffer, read);
      std::size_t newline;
      while ((newline = pending.find('\n')) != std::string::npos) {
        std::string_view line(pending.data(), newline);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        on_stdout_line(line);
        pending.erase(0, newline + 1);
      }
    }
    if (on_stdout_line && !pending.empty()) on_stdout_line(pending);
  });

  const ULONGLONG start = GetTickCount64();
  for (;;) {
    const DWORD wait = WaitForSingleObject(process.hProcess, 100);
    if (wait == WAIT_OBJECT_0) break;
    if (cancel != nullptr && cancel->load()) {
      result.cancelled = true;
      TerminateProcess(process.hProcess, 1);
    } else if (timeout_ms != INFINITE && GetTickCount64() - start > timeout_ms) {
      result.timed_out = true;
      TerminateProcess(process.hProcess, 1);
    }
    if (wait == WAIT_FAILED) break;
  }
  GetExitCodeProcess(process.hProcess, &result.exit_code);
  CloseHandle(process.hProcess);

  for (std::thread* reader : {&stdout_reader, &stderr_reader}) {
    HANDLE thread = reinterpret_cast<HANDLE>(reader->native_handle());
    if (WaitForSingleObject(thread, 3000) == WAIT_TIMEOUT) CancelSynchronousIo(thread);
    reader->join();
  }
  CloseHandle(out_read);
  CloseHandle(err_read);
  return result;
}

namespace {

UINT SystemAnsiCodePage() {
  wchar_t value[16] = {};
  DWORD size = sizeof(value);
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Nls\\CodePage", L"ACP", RRF_RT_REG_SZ,
                   nullptr, value, &size) == ERROR_SUCCESS) {
    const unsigned long code_page = std::wcstoul(value, nullptr, 10);
    if (code_page != 0) return static_cast<UINT>(code_page);
  }
  return GetACP();
}

}

bool IsAnsiSafe(std::wstring_view text) {
  bool ascii = true;
  for (const wchar_t c : text) ascii &= c < 0x80;
  if (ascii) return true;
  static const UINT code_page = SystemAnsiCodePage();
  if (code_page == CP_UTF8) return true;
  BOOL used_default = FALSE;
  const int length = WideCharToMultiByte(code_page, WC_NO_BEST_FIT_CHARS, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, &used_default);
  return length > 0 && !used_default;
}

CaptureResult ExtractWithSystemTar(const std::wstring& archive, const std::wstring& destination,
                                   const std::atomic<bool>* cancel, DWORD timeout_ms) {
  wchar_t system_dir[MAX_PATH] = {};
  GetSystemDirectoryW(system_dir, MAX_PATH);
  const std::wstring tar = JoinPath(system_dir, L"tar.exe");
  std::wstring archive_argument = archive;
  std::wstring link;
  if (!IsAnsiSafe(archive)) {
    static std::atomic<unsigned> counter{0};
    wchar_t name[96];
    std::swprintf(name, 96, L"elixir-archive-%lu-%u%ls", static_cast<unsigned long>(GetCurrentProcessId()),
                  counter.fetch_add(1), ExtensionOf(archive).c_str());
    const std::wstring parent = ParentDirectory(destination);
    link = JoinPath(parent.empty() ? destination : parent, name);
    DeleteFileW(link.c_str());
    if (!CreateHardLinkW(link.c_str(), archive.c_str(), nullptr) && !CopyFileW(archive.c_str(), link.c_str(), TRUE)) {
      CaptureResult result;
      result.error = L"Cannot prepare " + archive + L" for tar.exe: " + Win32ErrorText(GetLastError());
      return result;
    }
    archive_argument = parent.empty() ? std::wstring(name) : std::wstring(L"..\\") + name;
  }
  CaptureResult result = RunCaptured(tar, {L"-x", L"-f", archive_argument}, destination, {}, cancel, timeout_ms);
  if (!link.empty()) DeleteFileW(link.c_str());
  return result;
}

std::vector<std::wstring> RuntimeCommand(const LauncherPaths& paths, std::vector<std::wstring> arguments) {
  if (!paths.runtime_flag.empty()) arguments.insert(arguments.begin(), paths.runtime_flag);
  return arguments;
}

std::vector<std::wstring> BuildRuntimeArguments(const LauncherConfig& config, const LauncherPaths& paths) {
  const auto on_off = [](bool value) { return std::wstring(value ? L"on" : L"off"); };
  wchar_t scale[32];
  std::swprintf(scale, 32, L"%.2f", ResolutionScaleOrDefault(config.graphics.resolution_scale));
  wchar_t stretch[32];
  std::swprintf(stretch, 32, L"%.2f", StretchOrDefault(config.graphics.stretch));
  std::vector<std::wstring> args;
  if (config.installed.present()) {
    args.insert(args.end(), {L"--package", Utf8ToWide(config.installed.package), L"--version-code",
                             std::to_wstring(config.installed.version_code), L"--app-dir",
                             Utf8ToWide(config.installed.install_dir)});
  }
  const std::wstring fflags =
      config.advanced.fflags_path.empty() ? paths.fflags_file : Utf8ToWide(config.advanced.fflags_path);
  args.insert(args.end(),
              {L"--data-dir", paths.data_root, L"--config", paths.config_file, L"--crash-dir", paths.crashes_dir,
               L"--fflags", fflags, L"--guest-runtime", paths.guest_runtime_dir,
               L"--renderer", Utf8ToWide(config.graphics.renderer),
               L"--resolution-scale", scale,
               L"--stretch", stretch,
               L"--audio-device",
               config.audio.output_device_id.empty() ? L"default" : Utf8ToWide(config.audio.output_device_id),
               L"--timer-resolution-1ms", on_off(config.performance.timer_resolution_1ms),
               L"--mmcss", on_off(config.performance.mmcss)});
  if (!paths.accounts_dir.empty()) args.insert(args.end(), {L"--accounts-dir", paths.accounts_dir});
  if (!config.audio.input_device_id.empty()) {
    const std::string& id = config.audio.input_device_id;
    const bool valid = id.size() <= 512 && std::none_of(id.begin(), id.end(), [](unsigned char c) {
      return c < 0x20 || c == 0x7f;
    });
    args.insert(args.end(), {L"--audio-input-device", valid ? Utf8ToWide(id) : L"disabled"});
  }
  GlobalSettings roblox_settings;
  std::wstring roblox_settings_error;
  if (LoadGlobalSettings(paths, &roblox_settings, &roblox_settings_error) && roblox_settings.framerate_cap &&
      *roblox_settings.framerate_cap > 0 && *roblox_settings.framerate_cap <= 100000) {
    args.insert(args.end(), {L"--frame-rate-cap", std::to_wstring(*roblox_settings.framerate_cap)});
  }
  if (!config.advanced.manage_fast_flags) args.insert(args.end(), {L"--user-fflags", L"off"});
  if (config.mods.enabled) {
    const std::wstring mods = ModsDirectory(paths);
    if (PathIsDirectory(mods)) args.insert(args.end(), {L"--mods-dir", mods});
  }
  const IntegrationSettings& integrations = config.integrations;
  if (integrations.discord_rich_presence && IsDiscordApplicationId(DiscordApplicationId())) {
    args.insert(args.end(), {L"--discord", Utf8ToWide(std::string(DiscordApplicationId())), L"--discord-game",
                             on_off(integrations.discord_show_game), L"--discord-join",
                             on_off(integrations.discord_show_game && integrations.discord_join_button),
                             L"--discord-rpc",
                             on_off(integrations.discord_show_game && integrations.discord_allow_game_presence)});
  }
  return args;
}

namespace {

HANDLE CreateRuntimePipe(std::wstring* name) {
  static std::atomic<unsigned> counter{0};
  *name = L"\\\\.\\pipe\\elixir-runtime-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
          std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(counter.fetch_add(1));
  std::wstring sddl = L"D:P(A;;GA;;;SY)";
  HANDLE token = nullptr;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> buffer(size);
    wchar_t* sid = nullptr;
    if (size > 0 && GetTokenInformation(token, TokenUser, buffer.data(), size, &size) &&
        ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid)) {
      sddl += L"(A;;GA;;;" + std::wstring(sid) + L")";
      LocalFree(sid);
    }
    CloseHandle(token);
  }
  SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, FALSE};
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                            &attributes.lpSecurityDescriptor, nullptr)) {
    return nullptr;
  }
  HANDLE pipe = CreateNamedPipeW(name->c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                                 64 * 1024, 64 * 1024, 0, &attributes);
  LocalFree(attributes.lpSecurityDescriptor);
  return pipe == INVALID_HANDLE_VALUE ? nullptr : pipe;
}

bool WaitOverlapped(HANDLE handle, OVERLAPPED* overlapped, HANDLE also, DWORD* transferred) {
  HANDLE waits[2] = {overlapped->hEvent, also};
  const DWORD count = also != nullptr ? 2 : 1;
  const DWORD result = WaitForMultipleObjects(count, waits, FALSE, INFINITE);
  if (result != WAIT_OBJECT_0) {
    CancelIoEx(handle, overlapped);
    GetOverlappedResult(handle, overlapped, transferred, TRUE);
    return false;
  }
  return GetOverlappedResult(handle, overlapped, transferred, FALSE) != FALSE;
}

}

bool ParseRuntimeEvent(std::string_view line, RuntimeEvent* event) {
  std::string error;
  const auto json = Json::Parse(line, &error);
  if (!json || !json->is_object()) return false;
  RuntimeEvent out;
  out.event = json->GetString("event");
  if (out.event.empty()) return false;
  out.detail = json->GetString("detail");
  if (const Json* code = json->Find("code"); code != nullptr && code->is_number()) {
    out.has_code = true;
    out.code = code->AsInt(0);
  }
  *event = std::move(out);
  return true;
}

void ReadRuntimeEvents(HANDLE pipe, HANDLE process, const std::function<void(const RuntimeEvent&)>& on_event) {
  if (pipe == nullptr) return;
  HANDLE own = nullptr;
  if (!DuplicateHandle(GetCurrentProcess(), pipe, GetCurrentProcess(), &own, 0, FALSE, DUPLICATE_SAME_ACCESS)) return;
  HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  OVERLAPPED overlapped{};
  overlapped.hEvent = event;
  DWORD transferred = 0;
  bool connected = ConnectNamedPipe(own, &overlapped) != FALSE || GetLastError() == ERROR_PIPE_CONNECTED;
  if (!connected && GetLastError() == ERROR_IO_PENDING) connected = WaitOverlapped(own, &overlapped, process, &transferred);
  std::string pending;
  char buffer[4096];
  while (connected) {
    overlapped = OVERLAPPED{};
    overlapped.hEvent = event;
    ResetEvent(event);
    DWORD read = 0;
    BOOL ok = ReadFile(own, buffer, sizeof(buffer), &read, &overlapped);
    if (!ok && GetLastError() == ERROR_IO_PENDING) ok = WaitOverlapped(own, &overlapped, nullptr, &read);
    if (!ok || read == 0) break;
    pending.append(buffer, read);
    for (std::size_t end; (end = pending.find('\n')) != std::string::npos;) {
      RuntimeEvent parsed;
      if (ParseRuntimeEvent(std::string_view(pending).substr(0, end), &parsed)) on_event(parsed);
      pending.erase(0, end + 1);
    }
    if (pending.size() > 64 * 1024) pending.clear();
  }
  CloseHandle(event);
  CloseHandle(own);
}

namespace {

bool WriteRuntimeCommand(HANDLE pipe, const char* line, DWORD size, DWORD timeout_ms) {
  if (pipe == nullptr) return false;
  HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (event == nullptr) return false;
  OVERLAPPED overlapped{};
  overlapped.hEvent = event;
  DWORD written = 0;
  BOOL ok = WriteFile(pipe, line, size, &written, &overlapped);
  if (!ok && GetLastError() == ERROR_IO_PENDING) {
    ok = WaitForSingleObject(event, timeout_ms) == WAIT_OBJECT_0 &&
         GetOverlappedResult(pipe, &overlapped, &written, FALSE);
    if (!ok) {
      CancelIoEx(pipe, &overlapped);
      GetOverlappedResult(pipe, &overlapped, &written, TRUE);
    }
  }
  CloseHandle(event);
  return ok && written == size;
}

}

bool SendRuntimeStop(HANDLE pipe) {
  static constexpr char kStop[] = "{\"command\":\"stop\"}\n";
  return WriteRuntimeCommand(pipe, kStop, sizeof(kStop) - 1, 1000);
}

bool SendRuntimeJoin(HANDLE pipe, const std::string& request) {
  if (request.size() < 2 || request.size() > kMaxRuntimeJoinBytes || request.front() != '{' ||
      request.back() != '}' || request.find_first_of("\r\n") != std::string::npos) {
    return false;
  }
  std::string line = "{\"command\":\"join\",\"request\":" + request + "}\n";
  const bool ok = WriteRuntimeCommand(pipe, line.data(), static_cast<DWORD>(line.size()), 5000);
  SecureZeroMemory(line.data(), line.size());
  return ok;
}

std::wstring RuntimeExitMeaning(DWORD code) {
  switch (code) {
    case 0: return T(L"Roblox closed.");
    case 2: return T(L"Elixir couldn’t start Roblox. Reinstall Elixir and try again.");
    case 3: return T(L"A setting stopped Roblox from starting. Check your Fast Flags and try again.");
    case 4: return T(L"Roblox couldn’t start on this PC. Try again, or reinstall Elixir.");
    case 5: return T(L"Roblox isn’t installed correctly. Reinstall Roblox and try again.");
    case 6: return T(L"Roblox couldn’t load. Reinstall Roblox and try again.");
    case 7: return T(L"This version of Roblox isn’t supported yet.");
    case 10: return T(L"This version of Elixir can’t open the game window. Reinstall Elixir.");
    case 11: return T(L"Part of Elixir is missing. Reinstall Elixir.");
    case 12: return T(L"Part of Elixir is out of date. Reinstall Elixir.");
    case 137: return T(L"Roblox ran out of memory. Close other apps and try again.");
    default: break;
  }
  if (code >= 0xC0000000u || code == 0x80000003u) return T(L"Roblox crashed.");
  return {};
}

bool ShouldRestartRuntime(DWORD exit_code, bool stopped_by_launcher, ULONGLONG now_ms, ULONGLONG last_restart_ms) {
  if (exit_code != kRuntimeRestartExitCode || stopped_by_launcher) return false;
  return last_restart_ms == 0 || now_ms - last_restart_ms >= kRuntimeRestartMinGapMs;
}

namespace {

RuntimeLaunch StartRuntime(const LauncherConfig& config, const LauncherPaths& paths,
                           std::vector<std::wstring> arguments, bool events) {
  RuntimeLaunch launch;
  if (!PathIsFile(paths.runtime_exe)) {
    launch.error = T(L"Part of Elixir is missing. Reinstall Elixir.");
    return launch;
  }
  std::wstring pipe_name;
  if (events) launch.ipc_pipe = CreateRuntimePipe(&pipe_name);
  if (launch.ipc_pipe != nullptr) arguments.insert(arguments.begin(), {L"--ipc-pipe", pipe_name});
  arguments = RuntimeCommand(paths, std::move(arguments));
  launch.command_line = BuildCommandLine(paths.runtime_exe, arguments);
  std::wstring mutable_command = launch.command_line;
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  DWORD flags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;
  if (config.performance.high_priority) flags |= HIGH_PRIORITY_CLASS;
  if (!CreateProcessW(paths.runtime_exe.c_str(), mutable_command.data(), nullptr, nullptr, FALSE, flags, nullptr,
                      paths.exe_dir.c_str(), &startup, &process)) {
    launch.error = ErrorCause(GetLastError());
    if (launch.ipc_pipe != nullptr) CloseHandle(launch.ipc_pipe);
    launch.ipc_pipe = nullptr;
    return launch;
  }
  launch.high_priority_applied =
      config.performance.high_priority && GetPriorityClass(process.hProcess) == HIGH_PRIORITY_CLASS;
  if (config.performance.disable_power_throttling) {
    PROCESS_POWER_THROTTLING_STATE state{};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    if (config.performance.timer_resolution_1ms) state.ControlMask |= 0x4;
    state.StateMask = 0;
    launch.power_throttling_opt_out_applied =
        SetProcessInformation(process.hProcess, ProcessPowerThrottling, &state, sizeof(state)) != FALSE;
    if (!launch.power_throttling_opt_out_applied) {
      state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
      launch.power_throttling_opt_out_applied =
          SetProcessInformation(process.hProcess, ProcessPowerThrottling, &state, sizeof(state)) != FALSE;
    }
  }
  ResumeThread(process.hThread);
  CloseHandle(process.hThread);
  launch.process = process.hProcess;
  launch.pid = process.dwProcessId;
  launch.ok = true;
  return launch;
}

}

RuntimeLaunch LaunchRuntime(const LauncherConfig& config, const LauncherPaths& paths) {
  return StartRuntime(config, paths, BuildRuntimeArguments(config, paths), true);
}

RuntimeLaunch LaunchInstance(const LauncherConfig& config, const LauncherPaths& paths, int instance) {
  LauncherConfig extra = config;
  extra.integrations.discord_rich_presence = false;
  const LauncherPaths instance_paths = InstancePaths(paths, instance);
  std::vector<std::wstring> arguments = BuildRuntimeArguments(extra, instance_paths);
  arguments.insert(arguments.end(), {L"--instance", std::to_wstring(instance)});
  return StartRuntime(extra, instance_paths, std::move(arguments), false);
}

}
