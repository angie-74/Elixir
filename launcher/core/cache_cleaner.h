// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_CACHE_CLEANER_H_
#define ELIXIR_LAUNCHER_CORE_CACHE_CLEANER_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/paths.h"

namespace elixir::launcher {

struct CacheLocation {
  std::wstring id;
  std::wstring title;
  std::wstring description;
  std::wstring path;
  bool exists = false;
  bool redirected = false;
  std::uint64_t bytes = 0;
  std::uint64_t files = 0;
  std::uint64_t protected_files = 0;
};

std::vector<CacheLocation> ClientCacheLocations(const LauncherPaths& paths, std::string_view package, bool measure);

bool IsProtectedFromCleaning(std::wstring_view file_name);

struct CacheClearResult {
  bool ok = false;
  std::uint64_t files_deleted = 0;
  std::uint64_t bytes_freed = 0;
  std::uint64_t kept = 0;
  std::vector<std::wstring> failures;
  std::wstring error;
};

CacheClearResult ClearClientCaches(const LauncherPaths& paths, std::string_view package, bool runtime_running);

}

#endif
