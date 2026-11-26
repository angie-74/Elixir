// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_CRASH_LOGS_H_
#define ELIXIR_LAUNCHER_CORE_CRASH_LOGS_H_

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace elixir::launcher {

struct CrashLogEntry {
  std::wstring path;
  std::wstring name;
  std::uint64_t size = 0;
  FILETIME modified{};
  bool is_dump = false;
};

std::vector<CrashLogEntry> ListCrashLogs(const std::wstring& directory);

bool ReadCrashLogText(const std::wstring& path, std::wstring* text, std::wstring* error,
                      std::uint64_t max_bytes = 4u << 20);

std::vector<std::wstring> CrashLogGroup(const std::wstring& path);

bool RecycleFiles(const std::vector<std::wstring>& paths, std::wstring* error);

}

#endif
