// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/shared_settings.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "core/global_settings.h"
#include "core/instances.h"
#include "core/util.h"

namespace elixir::launcher {
namespace {

constexpr wchar_t kSharedName[] = L"roblox-settings.xml";
constexpr wchar_t kBaseName[] = L"roblox-settings.base.xml";
constexpr wchar_t kBackupName[] = L"roblox-settings.before-sharing.xml";
constexpr DWORD kMaxBytes = 4u << 20;

std::mutex& Lock() {
  static std::mutex lock;
  return lock;
}

struct Settings {
  std::string bytes;
  DWORD attributes = 0;
  std::uint64_t written = 0;

  bool read_only() const { return (attributes & FILE_ATTRIBUTE_READONLY) != 0; }
};

std::optional<Settings> ReadSettings(const LauncherPaths& paths) {
  const std::wstring file = GlobalSettingsFile(paths);
  if (!IsRealDirectoryBelow(paths.data_root, ParentDirectory(file))) return std::nullopt;
  const HANDLE handle =
      CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                  OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return std::nullopt;
  Settings settings;
  BY_HANDLE_FILE_INFORMATION info{};
  bool ok = GetFileInformationByHandle(handle, &info) &&
            (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) == 0 &&
            info.nFileSizeHigh == 0 && info.nFileSizeLow > 0 && info.nFileSizeLow <= kMaxBytes;
  if (ok) {
    settings.attributes = info.dwFileAttributes;
    settings.written = (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) |
                       info.ftLastWriteTime.dwLowDateTime;
    settings.bytes.resize(info.nFileSizeLow);
    DWORD got = 0;
    ok = ReadFile(handle, settings.bytes.data(), info.nFileSizeLow, &got, nullptr) && got == info.nFileSizeLow;
  }
  CloseHandle(handle);
  if (!ok) return std::nullopt;
  return settings;
}

bool WriteSettings(const LauncherPaths& paths, const std::string& bytes, bool read_only) {
  const std::wstring file = GlobalSettingsFile(paths);
  if (!IsRealDirectoryBelow(paths.data_root, ParentDirectory(file))) return false;
  const DWORD found = GetFileAttributesW(file.c_str());
  const bool was_locked = found != INVALID_FILE_ATTRIBUTES && (found & FILE_ATTRIBUTE_READONLY) != 0;
  if (found != INVALID_FILE_ATTRIBUTES && (found & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
    return false;
  }
  if (was_locked) SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL);
  if (!AtomicWriteFile(file, bytes, AtomicWriteBackup::kNone, nullptr)) {
    if (was_locked) SetFileAttributesW(file.c_str(), found);
    return false;
  }
  if (read_only) SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY);
  return true;
}

std::optional<std::string> ReadOwnFile(const std::wstring& file) {
  std::string bytes;
  if (!ReadFileBytes(file, &bytes, nullptr, kMaxBytes) || bytes.empty()) return std::nullopt;
  return bytes;
}

std::optional<std::string> ReadShared(const LauncherPaths& first) {
  std::optional<std::string> shared = ReadOwnFile(SharedSettingsFile(first));
  if (shared && !IsGlobalSettingsText(*shared)) shared.reset();
  return shared;
}

bool WriteOwnFile(const std::wstring& file, const std::string& bytes) {
  return AtomicWriteFile(file, bytes, AtomicWriteBackup::kNone, nullptr);
}

std::vector<LauncherPaths> WindowFolders(const LauncherPaths& first) {
  std::vector<LauncherPaths> folders{first};
  for (int number = 2; number <= kMaxRobloxWindows; ++number) {
    LauncherPaths paths = InstancePaths(first, number);
    if (PathIsDirectory(paths.data_root)) folders.push_back(std::move(paths));
  }
  return folders;
}

bool Collect(const LauncherPaths& first) {
  std::optional<std::string> shared = ReadShared(first);
  const std::optional<Settings> leader = ReadSettings(first);
  const bool locked = leader && leader->read_only();
  struct Change {
    std::wstring base_file;
    std::string base;
    Settings now;
  };
  std::vector<Change> changes;
  for (const LauncherPaths& folder : WindowFolders(first)) {
    const std::wstring base_file = SharedSettingsBaseFile(folder);
    std::optional<std::string> base = ReadOwnFile(base_file);
    if (!base) continue;
    std::optional<Settings> now = ReadSettings(folder);
    if (!now || now->bytes == *base || !IsGlobalSettingsText(now->bytes)) continue;
    changes.push_back(Change{base_file, std::move(*base), std::move(*now)});
  }
  std::stable_sort(changes.begin(), changes.end(),
                   [](const Change& a, const Change& b) { return a.now.written < b.now.written; });
  bool changed = false;
  for (const Change& change : changes) {
    if (!locked) {
      const std::optional<std::string> merged =
          shared ? MergeGlobalSettings(change.base, change.now.bytes, *shared)
                 : std::optional<std::string>(change.now.bytes);
      if (!merged) continue;
      if (!shared || *merged != *shared) {
        if (!WriteOwnFile(SharedSettingsFile(first), *merged)) continue;
        shared = *merged;
        changed = true;
      }
    }
    WriteOwnFile(change.base_file, change.now.bytes);
  }
  return changed;
}

}

std::wstring SharedSettingsFile(const LauncherPaths& first) { return JoinPath(first.data_root, kSharedName); }

std::wstring SharedSettingsBaseFile(const LauncherPaths& window) { return JoinPath(window.data_root, kBaseName); }

std::wstring SharedSettingsBackupFile(const LauncherPaths& window) { return JoinPath(window.data_root, kBackupName); }

bool CollectSharedSettings(const LauncherPaths& first) {
  const std::lock_guard<std::mutex> guard(Lock());
  return Collect(first);
}

bool ShareSettingsWith(const LauncherPaths& first, const LauncherPaths& window) {
  const std::lock_guard<std::mutex> guard(Lock());
  Collect(first);
  if (DataFolderInUse(window.data_root)) return false;
  const bool is_first = PathsEqual(first.data_root, window.data_root);
  std::optional<std::string> shared = ReadShared(first);
  std::optional<Settings> leader = ReadSettings(first);
  if (leader && !IsGlobalSettingsText(leader->bytes)) leader.reset();
  const bool locked = leader && leader->read_only();
  if (leader && (locked || !shared) && (!shared || *shared != leader->bytes)) {
    if (!WriteOwnFile(SharedSettingsFile(first), leader->bytes)) return false;
    shared = leader->bytes;
  }
  const std::optional<Settings> own = is_first ? leader : ReadSettings(window);
  if (!shared) {
    if (!own || !IsGlobalSettingsText(own->bytes)) return true;
    if (!WriteOwnFile(SharedSettingsFile(first), own->bytes)) return false;
    shared = own->bytes;
  }
  if (!(is_first && locked)) {
    if (!own || own->bytes != *shared) {
      const std::wstring kept = SharedSettingsBackupFile(window);
      if (own && !PathIsFile(SharedSettingsBaseFile(window)) && !PathIsFile(kept)) WriteOwnFile(kept, own->bytes);
      if (!WriteSettings(window, *shared, locked)) return false;
    } else if (own->read_only() != locked) {
      SetFileAttributesW(GlobalSettingsFile(window).c_str(),
                         locked ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL);
    }
  }
  return WriteOwnFile(SharedSettingsBaseFile(window), *shared);
}

}
