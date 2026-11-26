// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_ROBLOX_UPDATE_H_
#define ELIXIR_LAUNCHER_CORE_ROBLOX_UPDATE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/apk_component.h"
#include "core/roblox_download.h"

namespace elixir::launcher {

bool NewerRobloxRelease(const RobloxReleaseList& list, std::int64_t installed_code, RobloxRelease* out);

enum class RobloxUpdatePhase { kChecking, kDownloading, kVerifying, kInstalling };

struct RobloxUpdateProgress {
  RobloxUpdatePhase phase = RobloxUpdatePhase::kChecking;
  std::string version_name;
  int percent = -1;
};
using RobloxUpdateProgressCallback = std::function<void(const RobloxUpdateProgress&)>;

struct RobloxUpdateRequest {
  ApkComponent component;
  std::wstring exe_dir;
  std::wstring downloads_dir;
  std::wstring apps_dir;
  std::wstring work_dir;
  std::int64_t installed_code = 0;
  std::optional<RobloxRelease> target;
};

struct RobloxUpdateSteps {
  std::function<RobloxReleaseList(const std::atomic<bool>* cancel)> list;
  std::function<RobloxDownloadResult(const RobloxRelease& release, const RobloxProgressCallback& progress,
                                     const std::atomic<bool>* cancel)>
      download;
  std::function<ApkResult(const std::vector<std::wstring>& files, const ApkProgressCallback& progress,
                          const std::atomic<bool>* cancel)>
      verify;
  std::function<ApkResult(const ApkMetadata& verified, const ApkProgressCallback& progress,
                          const std::atomic<bool>* cancel)>
      install;
  std::function<void(const std::wstring& directory)> discard;
};

RobloxUpdateSteps RealRobloxUpdateSteps(const RobloxUpdateRequest& request);

struct RobloxUpdateOutcome {
  enum class Result { kUpToDate, kInstalled, kCancelled, kFailed };
  Result result = Result::kFailed;
  RobloxRelease release;
  ApkMetadata metadata;
  std::wstring error;
  std::string log;
};

RobloxUpdateOutcome UpdateRoblox(const RobloxUpdateRequest& request, const RobloxUpdateSteps& steps,
                                 const RobloxUpdateProgressCallback& progress, const std::atomic<bool>* cancel);

bool IsCacheFolderName(std::wstring_view name);

struct AppDataSnapshot {
  bool ok = false;
  std::wstring path;
  std::uint64_t files = 0;
  std::uint64_t bytes = 0;
  std::uint64_t skipped_links = 0;
  std::wstring error;
};

std::wstring AppBackupsDirectory(const std::wstring& backups_root, std::string_view package);

AppDataSnapshot SnapshotAppData(const std::wstring& data_dir, const std::wstring& backups_dir,
                                const std::wstring& name, std::int64_t version_code);

AppDataSnapshot RestoreAppData(const std::wstring& snapshot, const std::wstring& data_dir,
                               const std::wstring& backups_dir, std::wstring* error);

void RecoverInterruptedDataWork(const std::wstring& backups_dir, const std::wstring& data_dir);

void PruneSnapshots(const std::wstring& backups_dir, std::size_t keep, const std::vector<std::wstring>& protect);

void PruneVersions(const std::wstring& package_dir, const std::vector<std::wstring>& keep, std::int64_t below_code);

}

#endif
