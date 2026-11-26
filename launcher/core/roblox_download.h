// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_ROBLOX_DOWNLOAD_H_
#define ELIXIR_LAUNCHER_CORE_ROBLOX_DOWNLOAD_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/apk_component.h"
#include "core/paths.h"

namespace elixir::launcher {

const std::vector<std::string>& RobloxAssetHosts();

bool RobloxDownloadAvailable();

struct RobloxPackageFile {
  std::string name;
  std::string url;
  std::string sha256;
  std::uint64_t size = 0;
  bool split = false;
};

struct RobloxRelease {
  std::string version_name;
  std::int64_t version_code = 0;
  std::string published_at;
  bool pinned = false;
  std::vector<RobloxPackageFile> files;
  std::uint64_t TotalSize() const;
};

std::string ParseGitHubAssetDigest(std::string_view digest);

bool ParseGitHubReleases(std::string_view json, std::vector<RobloxRelease>* releases, std::size_t* skipped,
                         std::string* error);

bool ParsePinnedRobloxReleases(std::string_view json, std::vector<RobloxRelease>* releases, std::string* error);

std::wstring LocateDownloadSources(const std::wstring& exe_dir);

struct GitHubApiResponse {
  long status = 0;
  std::string body;
  std::string error;
  std::string ratelimit_limit;
  std::string ratelimit_remaining;
  std::string ratelimit_reset;
  std::string retry_after;
  std::int64_t date = 0;
};

struct GitHubApiFailure {
  std::wstring reason;
  bool rate_limited = false;
  std::int64_t retry_at = 0;
};

GitHubApiFailure DescribeGitHubApiFailure(const GitHubApiResponse& response, std::int64_t received_at);

struct RobloxReleaseList {
  bool ok = false;
  bool pinned = false;
  std::vector<RobloxRelease> releases;
  std::size_t skipped = 0;
  std::wstring error;
  bool rate_limited = false;
  std::int64_t retry_at = 0;
};

RobloxReleaseList LatestRobloxVersionFromGitHubResponse(const GitHubApiResponse& response, std::int64_t received_at);
RobloxReleaseList FetchRobloxVersions(const std::atomic<bool>* cancel);

RobloxReleaseList ReleaseListFromGitHubResponse(const GitHubApiResponse& response, std::int64_t received_at);

RobloxReleaseList FetchRobloxReleases(const std::atomic<bool>* cancel);

RobloxReleaseList FetchAllRobloxReleases(const std::atomic<bool>* cancel);

RobloxReleaseList WithPinnedFallback(RobloxReleaseList list, const std::wstring& pinned_sources);

std::int64_t UnixNow();
std::wstring FormatLocalClockTime(std::int64_t unix_time, std::int64_t now);

struct SigningPins {
  std::wstring path;
  std::string package;
  std::vector<std::string> sha256;
};

std::wstring LocateSigningPins(const std::wstring& exe_dir);
bool LoadSigningPins(const std::wstring& path, SigningPins* pins, std::wstring* error);

struct RobloxDownloadProgress {
  std::string message;
  int file = 0;
  int files = 0;
  std::uint64_t done = 0;
  std::uint64_t total = 0;
};
using RobloxProgressCallback = std::function<void(const RobloxDownloadProgress&)>;

struct DownloadedFile {
  std::wstring path;
  std::string name;
  std::string sha256;
  std::string published_sha256;
  std::uint64_t size = 0;
  std::uint64_t resumed = 0;
};

struct RobloxDownloadResult {
  bool ok = false;
  bool cancelled = false;
  std::wstring directory;
  std::vector<DownloadedFile> files;
  std::wstring error;
  std::string log;
};

std::wstring RobloxDownloadDirectory(const std::wstring& downloads_dir, const RobloxRelease& release);

bool AcceptDownloadedFile(const std::wstring& path, const RobloxPackageFile& published, std::string* measured,
                          std::wstring* error);

RobloxDownloadResult DownloadRobloxRelease(const RobloxRelease& release, const std::wstring& downloads_dir,
                                           const RobloxProgressCallback& progress, const std::atomic<bool>* cancel);

ApkResult VerifyRobloxPackage(const ApkComponent& component, const std::vector<std::wstring>& files,
                              const SigningPins& pins, const std::wstring& work_dir,
                              const ApkProgressCallback& progress, const std::atomic<bool>* cancel);

void DeleteRobloxDownload(const std::wstring& directory, const std::wstring& downloads_dir);

}

#endif
