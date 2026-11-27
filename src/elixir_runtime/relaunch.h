// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_RELAUNCH_H_
#define ELIXIR_RUNTIME_RELAUNCH_H_

#include <string>
#include <string_view>
#include <vector>

namespace elixir::runtime {

inline constexpr wchar_t kAfterPidFlag[] = L"--after-pid";
inline constexpr wchar_t kIpcPipeFlag[] = L"--ipc-pipe";
inline constexpr unsigned long kAfterPidWaitMs = 20000;

std::wstring QuoteCommandLineArgument(std::wstring_view argument);

std::wstring RelaunchCommandLine(const std::vector<std::wstring>& arguments, unsigned long after_pid);

void WaitForProcessToExit(unsigned long pid, unsigned long timeout_ms);

bool RelaunchThisProcess(std::wstring* error);

}

#endif
