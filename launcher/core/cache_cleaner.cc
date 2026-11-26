// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/cache_cleaner.h"

#include <cwctype>

#include "core/util.h"

namespace elixir::launcher {

namespace {

std::wstring Lower(std::wstring_view text) {
  std::wstring lower(text);
  for (wchar_t& c : lower) c = static_cast<wchar_t>(std::towlower(c));
  return lower;
}

struct Walk {
  std::uint64_t bytes = 0;
  std::uint64_t files = 0;
  std::uint64_t protected_files = 0;
};

void Measure(const std::wstring& directory, Walk* walk, int depth) {
  if (depth > 64) return;
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileExW(JoinPath(directory, L"*").c_str(), FindExInfoBasic, &data, FindExSearchNameMatch,
                                 nullptr, FIND_FIRST_EX_LARGE_FETCH);
  if (find == INVALID_HANDLE_VALUE) return;
  do {
    const std::wstring name = data.cFileName;
    if (name == L"." || name == L"..") continue;
    if (IsProtectedFromCleaning(name)) {
      ++walk->protected_files;
      continue;
    }
    if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      Measure(JoinPath(directory, name), walk, depth + 1);
      continue;
    }
    walk->bytes += (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    ++walk->files;
  } while (FindNextFileW(find, &data));
  FindClose(find);
}

void Clear(const std::wstring& directory, CacheClearResult* result, int depth) {
  if (depth > 64) return;
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileExW(JoinPath(directory, L"*").c_str(), FindExInfoBasic, &data, FindExSearchNameMatch,
                                 nullptr, FIND_FIRST_EX_LARGE_FETCH);
  if (find == INVALID_HANDLE_VALUE) return;
  do {
    const std::wstring name = data.cFileName;
    if (name == L"." || name == L"..") continue;
    const std::wstring child = JoinPath(directory, name);
    if (IsProtectedFromCleaning(name)) {
      ++result->kept;
      continue;
    }
    if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      const bool removed = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                               ? RemoveDirectoryW(child.c_str()) != FALSE
                               : DeleteFileEntry(child, data.dwFileAttributes);
      if (!removed) result->failures.push_back(child);
      continue;
    }
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      Clear(child, result, depth + 1);
      RemoveDirectoryW(child.c_str());
      continue;
    }
    const std::uint64_t size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    if (DeleteFileEntry(child, data.dwFileAttributes)) {
      ++result->files_deleted;
      result->bytes_freed += size;
    } else {
      result->failures.push_back(child);
    }
  } while (FindNextFileW(find, &data));
  FindClose(find);
}

}

bool IsProtectedFromCleaning(std::wstring_view file_name) {
  const std::wstring name = Lower(file_name);
  if (name.find(L"cookie") != std::wstring::npos) return true;
  if (name.rfind(L"globalbasicsettings", 0) == 0) return true;
  if (name.rfind(L"fflags.json", 0) == 0) return true;
  if (name == L"appstorage.json" || name == L"clientappsettings.json") return true;
  if (name.rfind(L"config.json", 0) == 0) return true;
  return false;
}

std::vector<CacheLocation> ClientCacheLocations(const LauncherPaths& paths, std::string_view package, bool measure) {
  const std::wstring app = AndroidAppDataDirectory(paths, package);
  std::vector<CacheLocation> locations = {
      {L"android-cache", L"Roblox cache", L"Files Roblox keeps to load faster.", JoinPath(app, L"cache")},
      {L"code-cache", L"Code cache", L"Files Roblox can rebuild.", JoinPath(app, L"code_cache")},
      {L"roblox-logs", L"Roblox logs", L"Roblox’s own logs of earlier sessions.",
       JoinPath(JoinPath(JoinPath(app, L"files"), L"appData"), L"logs")},
      {L"temp", L"Temporary files", L"Leftovers from earlier installs and sessions.", paths.guest_tmp_dir},
      {L"downloads", L"Interrupted downloads", L"Partial Roblox downloads kept so they can resume.",
       paths.downloads_dir},
  };
  for (CacheLocation& location : locations) {
    location.exists = IsRealDirectoryBelow(paths.data_root, location.path);
    location.redirected = !location.exists && PathIsDirectory(location.path);
    if (measure && location.exists) {
      Walk walk;
      Measure(location.path, &walk, 0);
      location.bytes = walk.bytes;
      location.files = walk.files;
      location.protected_files = walk.protected_files;
    }
  }
  return locations;
}

CacheClearResult ClearClientCaches(const LauncherPaths& paths, std::string_view package, bool runtime_running) {
  CacheClearResult result;
  if (runtime_running) {
    result.error = L"Roblox is running. Close Roblox, then clear the caches.";
    return result;
  }
  for (const CacheLocation& location : ClientCacheLocations(paths, package, false)) {
    if (!location.exists) continue;
    Clear(location.path, &result, 0);
  }
  result.ok = result.failures.empty();
  if (!result.ok) {
    result.error = result.failures.size() == 1
                       ? std::wstring(L"1 file couldn’t be deleted. It may be in use.")
                       : std::to_wstring(result.failures.size()) + L" files couldn’t be deleted. They may be in use.";
  }
  return result;
}

}
