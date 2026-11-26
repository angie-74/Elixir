// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_PROCESS_H_
#define ELIXIR_LAUNCHER_CORE_PROCESS_H_

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <functional>
#include <string>
#include <cstdint>
#include <string_view>
#include <vector>

#include "core/config.h"
#include "core/paths.h"

namespace elixir::launcher {

std::wstring QuoteArgument(std::wstring_view argument);
std::wstring BuildCommandLine(const std::wstring& executable, const std::vector<std::wstring>& arguments);

struct CaptureResult {
  bool started = false;
  bool timed_out = false;
  bool cancelled = false;
  DWORD exit_code = 0;
  std::string stdout_text;
  std::string stderr_text;
  std::wstring error;
};

using LineCallback = std::function<void(std::string_view line)>;

CaptureResult RunCaptured(const std::wstring& executable, const std::vector<std::wstring>& arguments,
                          const std::wstring& working_directory, const LineCallback& on_stdout_line,
                          const std::atomic<bool>* cancel, DWORD timeout_ms);

bool IsAnsiSafe(std::wstring_view text);

CaptureResult ExtractWithSystemTar(const std::wstring& archive, const std::wstring& destination,
                                   const std::atomic<bool>* cancel, DWORD timeout_ms);

std::vector<std::wstring> BuildRuntimeArguments(const LauncherConfig& config, const LauncherPaths& paths);
std::vector<std::wstring> RuntimeCommand(const LauncherPaths& paths, std::vector<std::wstring> arguments);

struct RuntimeLaunch {
  bool ok = false;
  HANDLE process = nullptr;
  HANDLE ipc_pipe = nullptr;
  DWORD pid = 0;
  bool high_priority_applied = false;
  bool power_throttling_opt_out_applied = false;
  std::wstring command_line;
  std::wstring error;
};

RuntimeLaunch LaunchRuntime(const LauncherConfig& config, const LauncherPaths& paths);

RuntimeLaunch LaunchInstance(const LauncherConfig& config, const LauncherPaths& paths, int instance);

struct RuntimeEvent {
  std::string event;
  std::string detail;
  bool has_code = false;
  std::int64_t code = 0;
};
bool ParseRuntimeEvent(std::string_view line, RuntimeEvent* event);

void ReadRuntimeEvents(HANDLE pipe, HANDLE process, const std::function<void(const RuntimeEvent&)>& on_event);

bool SendRuntimeStop(HANDLE pipe);

inline constexpr std::size_t kMaxRuntimeJoinBytes = 64 * 1024;

bool SendRuntimeJoin(HANDLE pipe, const std::string& request);

std::wstring RuntimeExitMeaning(DWORD code);

bool IsAnyRuntimeProcessRunning();

}

#endif
