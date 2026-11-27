// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/relaunch.h"

#include <windows.h>
#include <shellapi.h>

namespace elixir::runtime {

std::wstring QuoteCommandLineArgument(std::wstring_view argument) {
  if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
    return std::wstring(argument);
  }
  std::wstring quoted = L"\"";
  std::size_t backslashes = 0;
  for (const wchar_t c : argument) {
    if (c == L'\\') {
      ++backslashes;
      continue;
    }
    if (c == L'"') {
      quoted.append(backslashes * 2 + 1, L'\\');
    } else {
      quoted.append(backslashes, L'\\');
    }
    backslashes = 0;
    quoted.push_back(c);
  }
  quoted.append(backslashes * 2, L'\\');
  quoted.push_back(L'"');
  return quoted;
}

std::wstring RelaunchCommandLine(const std::vector<std::wstring>& arguments, unsigned long after_pid) {
  std::wstring line;
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    if (i > 0 && (arguments[i] == kAfterPidFlag || arguments[i] == kIpcPipeFlag)) {
      ++i;
      continue;
    }
    if (!line.empty()) line.push_back(L' ');
    line += QuoteCommandLineArgument(arguments[i]);
  }
  if (line.empty()) return line;
  line += L" ";
  line += kAfterPidFlag;
  line += L" " + std::to_wstring(after_pid);
  return line;
}

void WaitForProcessToExit(unsigned long pid, unsigned long timeout_ms) {
  if (pid == 0 || pid == GetCurrentProcessId()) return;
  const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (process == nullptr) return;
  WaitForSingleObject(process, timeout_ms);
  CloseHandle(process);
}

bool RelaunchThisProcess(std::wstring* error) {
  const auto fail = [error](const wchar_t* what) {
    if (error != nullptr) *error = std::wstring(what) + L" (Windows error " + std::to_wstring(GetLastError()) + L")";
    return false;
  };
  std::wstring program(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, program.data(), static_cast<DWORD>(program.size()));
  if (length == 0 || length >= program.size()) return fail(L"the program's path could not be read");
  program.resize(length);
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) return fail(L"the command line could not be read");
  std::vector<std::wstring> arguments;
  arguments.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) arguments.emplace_back(argv[i]);
  LocalFree(argv);
  if (arguments.empty()) arguments.push_back(program);
  std::wstring command = RelaunchCommandLine(arguments, GetCurrentProcessId());
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  const DWORD priority = GetPriorityClass(GetCurrentProcess());
  if (!CreateProcessW(program.c_str(), command.data(), nullptr, nullptr, FALSE,
                      CREATE_UNICODE_ENVIRONMENT | priority, nullptr, nullptr, &startup, &process)) {
    return fail(L"the new process could not be started");
  }
  AllowSetForegroundWindow(process.dwProcessId);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}

}
