// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <shobjidl.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "elixir/core_api.h"
#include "elixir_runtime/host_main.h"
#include "elixir_runtime/relaunch.h"
#include "platform/windows/app_user_model_id.h"

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "0.0.0"
#endif

namespace {

std::string ToUtf8(const wchar_t* text) {
  if (text == nullptr || *text == L'\0') return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) return {};
  std::string out(static_cast<std::size_t>(size - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
  return out;
}

std::string JsonString(const std::string& text) {
  std::string out = "\"";
  for (const char c : text) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
      out.push_back(c);
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char escaped[8];
      std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
      out += escaped;
    } else {
      out.push_back(c);
    }
  }
  out.push_back('"');
  return out;
}

std::string LastErrorText(DWORD code) {
  wchar_t* text = nullptr;
  const DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
      reinterpret_cast<wchar_t*>(&text), 0, nullptr);
  std::string out = length > 0 ? ToUtf8(text) : std::string();
  if (text != nullptr) LocalFree(text);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ' || out.back() == '.')) {
    out.pop_back();
  }
  return out + " (" + std::to_string(code) + ")";
}

void WriteStderr(const std::string& line) {
  const HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return;
  const std::string text = "elixir-runtime: " + line + "\n";
  DWORD written = 0;
  WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
}

constexpr std::string_view kStopCommand = R"({"command":"stop"})";
constexpr std::string_view kJoinCommandPrefix = R"({"command":"join","request":)";
constexpr std::size_t kMaxCommandBytes = kJoinCommandPrefix.size() + ELIXIR_LAUNCH_MAX_BYTES + 1;

class LauncherPipe {
 public:
  explicit LauncherPipe(const std::wstring& name) {
    if (name.empty()) return;
    if (name.rfind(LR"(\\.\pipe\)", 0) != 0) return;
    pipe_ = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    if (pipe_ == INVALID_HANDLE_VALUE) pipe_ = nullptr;
    write_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  }
  ~LauncherPipe() { Close(); }
  LauncherPipe(const LauncherPipe&) = delete;
  LauncherPipe& operator=(const LauncherPipe&) = delete;

  bool connected() const { return pipe_ != nullptr; }

  bool alive() {
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (pipe_ == nullptr || broken_) return false;
    DWORD available = 0;
    return PeekNamedPipe(pipe_, nullptr, 0, nullptr, &available, nullptr) != FALSE;
  }

  void Send(const std::string& json) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (pipe_ == nullptr || broken_) return;
    const std::string line = json + "\n";
    OVERLAPPED overlapped{};
    overlapped.hEvent = write_event_;
    ResetEvent(write_event_);
    DWORD written = 0;
    BOOL ok = WriteFile(pipe_, line.data(), static_cast<DWORD>(line.size()), &written, &overlapped);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
      if (WaitForSingleObject(write_event_, 2000) != WAIT_OBJECT_0) {
        CancelIoEx(pipe_, &overlapped);
        GetOverlappedResult(pipe_, &overlapped, &written, TRUE);
        broken_ = true;
        return;
      }
      ok = GetOverlappedResult(pipe_, &overlapped, &written, FALSE);
    }
    if (!ok || written != line.size()) broken_ = true;
  }

  void WatchCommands(void (*on_stop)(), void (*on_join)(LauncherPipe* pipe, const char* request, std::size_t size)) {
    if (pipe_ == nullptr) return;
    on_stop_ = on_stop;
    on_join_ = on_join;
    read_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD available = 0;
    while (PeekNamedPipe(pipe_, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
      char buffer[4096];
      DWORD read = 0;
      if (!ReadSome(buffer, available < sizeof(buffer) ? available : sizeof(buffer), &read) || read == 0) break;
      Consume(buffer, read);
      SecureZeroMemory(buffer, read);
    }
    reader_ = std::thread([this] {
      char buffer[4096];
      while (!closing_.load()) {
        DWORD read = 0;
        if (!ReadSome(buffer, sizeof(buffer), &read) || read == 0) break;
        Consume(buffer, read);
        SecureZeroMemory(buffer, read);
      }
    });
  }

  void Close() {
    closing_.store(true);
    if (pipe_ != nullptr) CancelIoEx(pipe_, nullptr);
    if (reader_.joinable()) reader_.join();
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (pipe_ != nullptr) {
      FlushFileBuffers(pipe_);
      CloseHandle(pipe_);
      pipe_ = nullptr;
    }
    if (write_event_ != nullptr) {
      CloseHandle(write_event_);
      write_event_ = nullptr;
    }
    if (read_event_ != nullptr) {
      CloseHandle(read_event_);
      read_event_ = nullptr;
    }
  }

 private:
  bool ReadSome(char* buffer, DWORD size, DWORD* read) {
    OVERLAPPED overlapped{};
    overlapped.hEvent = read_event_;
    ResetEvent(read_event_);
    BOOL ok = ReadFile(pipe_, buffer, size, read, &overlapped);
    if (!ok && GetLastError() == ERROR_IO_PENDING) ok = GetOverlappedResult(pipe_, &overlapped, read, TRUE);
    return ok != FALSE;
  }

  void Consume(const char* data, DWORD size) {
    for (DWORD i = 0; i < size; ++i) {
      if (data[i] != '\n') {
        if (discarding_) continue;
        if (pending_.size() >= kMaxCommandBytes) {
          WipePending();
          discarding_ = true;
          continue;
        }
        pending_.push_back(data[i]);
        continue;
      }
      if (!discarding_) HandleLine(pending_);
      WipePending();
      discarding_ = false;
    }
  }

  void HandleLine(const std::string& line) {
    if (line == kStopCommand) {
      if (!stopped_) {
        stopped_ = true;
        on_stop_();
      }
      return;
    }
    if (line.size() > kJoinCommandPrefix.size() + 1 && line.compare(0, kJoinCommandPrefix.size(), kJoinCommandPrefix) == 0 &&
        line.back() == '}' && on_join_ != nullptr) {
      on_join_(this, line.data() + kJoinCommandPrefix.size(), line.size() - kJoinCommandPrefix.size() - 1);
    }
  }

  void WipePending() {
    SecureZeroMemory(pending_.data(), pending_.capacity());
    pending_.clear();
  }

  HANDLE pipe_ = nullptr;
  HANDLE write_event_ = nullptr;
  HANDLE read_event_ = nullptr;
  void (*on_stop_)() = nullptr;
  void (*on_join_)(LauncherPipe* pipe, const char* request, std::size_t size) = nullptr;
  std::string pending_;
  bool discarding_ = false;
  bool stopped_ = false;
  std::mutex write_mutex_;
  bool broken_ = false;
  std::atomic<bool> closing_{false};
  std::thread reader_;
};

const ElixirCoreApi* g_api = nullptr;

void ReportToLauncher(void* context, uint32_t kind, const char* json) {
  (void)kind;
  if (context != nullptr && json != nullptr) static_cast<LauncherPipe*>(context)->Send(json);
}

void StopFromLauncher() {
  if (g_api != nullptr) g_api->request_stop();
}

bool CoreTakesJoins() {
  return g_api != nullptr &&
         g_api->struct_size >= offsetof(ElixirCoreApi, submit_launch) + sizeof(g_api->submit_launch) &&
         g_api->submit_launch != nullptr;
}

void JoinFromLauncher(LauncherPipe* launcher, const char* request, std::size_t size) {
  int result = ELIXIR_LAUNCH_UNAVAILABLE;
  if (size > 0 && size <= ELIXIR_LAUNCH_MAX_BYTES && CoreTakesJoins()) {
    result = g_api->submit_launch(request, static_cast<uint32_t>(size));
  }
  if (result == ELIXIR_LAUNCH_ACCEPTED) return;
  const char* state = result == ELIXIR_LAUNCH_BUSY || result == ELIXIR_LAUNCH_STOPPING ? "busy" : "rejected";
  launcher->Send(std::string("{\"event\":\"join\",\"detail\":\"") + state + "\"}");
}

int Finish(LauncherPipe& launcher, int code) {
  launcher.Send("{\"event\":\"exit\",\"code\":" + std::to_string(code) + "}");
  launcher.Close();
  return code;
}

int HostError(LauncherPipe& launcher, int code, const std::string& message) {
  WriteStderr(message);
  launcher.Send("{\"event\":\"error\",\"detail\":" + JsonString(message) + "}");
  return Finish(launcher, code);
}

}

int ElixirRuntimeHostMain(int argc, wchar_t** argv) {
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
  SetCurrentProcessExplicitAppUserModelID(elixir::kAppUserModelId);

  std::wstring pipe_name;
  unsigned long after_pid = 0;
  std::vector<std::string> arguments;
  for (int i = 1; i < argc; ++i) {
    const std::wstring_view argument = argv[i];
    if (argument == elixir::runtime::kIpcPipeFlag && i + 1 < argc) {
      pipe_name = argv[++i];
      continue;
    }
    if (argument == elixir::runtime::kAfterPidFlag && i + 1 < argc) {
      after_pid = wcstoul(argv[++i], nullptr, 10);
      continue;
    }
    arguments.push_back(ToUtf8(argv[i]));
  }
  elixir::runtime::WaitForProcessToExit(after_pid, elixir::runtime::kAfterPidWaitMs);
  LauncherPipe launcher(pipe_name);
  launcher.Send("{\"event\":\"host\",\"pid\":" + std::to_string(GetCurrentProcessId()) + ",\"version\":" +
                JsonString(ELIXIR_VERSION) + ",\"abi\":" + std::to_string(ELIXIR_CORE_ABI_VERSION) + "}");

  g_api = elixir_core_get_api(ELIXIR_CORE_ABI_VERSION);
  if (g_api == nullptr) return HostError(launcher, ELIXIR_EXIT_CORE_ABI, "Elixir's runtime could not start.");

  ElixirCoreHostInfo host{};
  host.struct_size = sizeof(host);
  host.abi_version = ELIXIR_CORE_ABI_VERSION;
  host.host_version = ELIXIR_VERSION;
  host.report = launcher.connected() ? &ReportToLauncher : nullptr;
  host.report_context = &launcher;
  int code = g_api->initialize(&host);
  if (code == 0) {
    launcher.WatchCommands(&StopFromLauncher, &JoinFromLauncher);
    std::vector<const char*> argv8;
    argv8.reserve(arguments.size());
    for (const std::string& argument : arguments) argv8.push_back(argument.c_str());
    code = g_api->run(static_cast<int>(argv8.size()), argv8.data());
  }
  g_api->shutdown();
  if (code == static_cast<int>(ELIXIR_EXIT_RESTART) && !launcher.alive()) {
    std::wstring error;
    if (!elixir::runtime::RelaunchThisProcess(&error)) {
      WriteStderr("Roblox could not be started again: " + ToUtf8(error.c_str()));
    }
  }
  return Finish(launcher, code);
}
