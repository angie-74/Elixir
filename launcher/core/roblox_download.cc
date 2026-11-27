// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/roblox_download.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <iterator>
#include <cstdio>

#include "core/json.h"
#include "core/paths.h"
#include "core/sha256.h"
#include "core/util.h"
#include "core/i18n.h"

#if defined(ELIXIR_WITH_ROBLOX_DOWNLOAD)
#include "update/http_download.h"
#endif

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "0.0.0"
#endif

namespace elixir::launcher {

namespace {

constexpr char kGitHubAssetPrefix[] =
    "https://github.com/Roblox-DeployHistory-Updates/android-runner/releases/download/";
constexpr std::uint64_t kMaximumApkBytes = 1024ull * 1024ull * 1024ull;
constexpr ULONGLONG kUnixEpochAsFileTime = 116444736000000000ull;

bool AllDigits(std::string_view text) {
  return !text.empty() && text.size() <= 18 &&
         std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

bool ParseCount(std::string_view text, std::int64_t* value) {
  const std::string trimmed = TrimAscii(text);
  if (!AllDigits(trimmed)) return false;
  *value = std::stoll(trimmed);
  return true;
}

std::string VersionNameOf(const std::string& release_name, std::int64_t code) {
  const std::string suffix = " (" + std::to_string(code) + ")";
  if (release_name.size() > suffix.size() &&
      release_name.compare(release_name.size() - suffix.size(), suffix.size(), suffix) == 0) {
    return TrimAscii(release_name.substr(0, release_name.size() - suffix.size()));
  }
  return TrimAscii(release_name);
}

std::int64_t UnixTimeOf(const FILETIME& time) {
  ULARGE_INTEGER value;
  value.LowPart = time.dwLowDateTime;
  value.HighPart = time.dwHighDateTime;
  if (value.QuadPart < kUnixEpochAsFileTime) return 0;
  return static_cast<std::int64_t>((value.QuadPart - kUnixEpochAsFileTime) / 10000000ull);
}

std::int64_t UnixTimeOf(const SYSTEMTIME& utc) {
  FILETIME time{};
  return SystemTimeToFileTime(&utc, &time) ? UnixTimeOf(time) : 0;
}

std::int64_t ParseHttpDate(std::string_view text) {
  const std::wstring wide = Utf8ToWide(TrimAscii(text));
  SYSTEMTIME utc{};
  if (wide.empty() || !WinHttpTimeToSystemTime(wide.c_str(), &utc)) return 0;
  return UnixTimeOf(utc);
}

std::wstring GitHubMessage(std::string_view body) {
  if (body.empty() || body.size() > (64u << 10)) return {};
  std::string error;
  const auto json = Json::Parse(body, &error);
  if (!json || !json->is_object()) return {};
  std::wstring message = Utf8ToWide(TrimAscii(json->GetString("message")));
  if (message.size() > 200) message = message.substr(0, 200) + L"…";
  return message;
}

std::wstring LocateConfigFile(const std::wstring& exe_dir, const wchar_t* name) {
  const std::wstring relative = std::wstring(L"config\\") + name;
  const std::wstring beside = JoinPath(exe_dir, relative);
  if (PathIsFile(beside)) return beside;
  if (!BundleDirectory().empty()) {
    const std::wstring bundled = JoinPath(BundleDirectory(), relative);
    if (PathIsFile(bundled)) return bundled;
  }
#if defined(ELIXIR_SOURCE_DIR)
  std::wstring source = Utf8ToWide(ELIXIR_SOURCE_DIR);
  std::replace(source.begin(), source.end(), L'/', L'\\');
  const std::wstring in_source = JoinPath(source, relative);
  if (PathIsFile(in_source)) return in_source;
#endif
  return {};
}

}

const std::vector<std::string>& RobloxAssetHosts() {
  static const std::vector<std::string> hosts = {"github.com", "release-assets.githubusercontent.com"};
  return hosts;
}

bool RobloxDownloadAvailable() {
#if defined(ELIXIR_WITH_ROBLOX_DOWNLOAD)
  return true;
#else
  return false;
#endif
}

std::uint64_t RobloxRelease::TotalSize() const {
  std::uint64_t total = 0;
  for (const RobloxPackageFile& file : files) total += file.size;
  return total;
}

std::string ParseGitHubAssetDigest(std::string_view digest) {
  constexpr std::string_view kPrefix = "sha256:";
  if (digest.size() != kPrefix.size() + 64 || digest.substr(0, kPrefix.size()) != kPrefix) return {};
  const std::string_view hex = digest.substr(kPrefix.size());
  if (!IsSha256Hex(hex)) return {};
  return NormalizeSha256(hex);
}

bool ParseGitHubReleases(std::string_view text, std::vector<RobloxRelease>* releases, std::size_t* skipped,
                         std::string* error) {
  std::string parse_error;
  const auto json = Json::Parse(text, &parse_error);
  if (!json) {
    if (error != nullptr) *error = "the GitHub response is not JSON: " + parse_error;
    return false;
  }
  if (!json->is_array()) {
    if (error != nullptr) {
      const std::string message = json->is_object() ? json->GetString("message") : std::string();
      *error = message.empty() ? "the GitHub response is not a list of releases" : "GitHub said: " + message;
    }
    return false;
  }
  std::vector<RobloxRelease> out;
  std::size_t left_out = 0;
  for (const Json& item : json->items()) {
    if (!item.is_object() || item.GetBool("draft", false)) {
      ++left_out;
      continue;
    }
    const std::string tag = item.GetString("tag_name");
    if (!AllDigits(tag)) {
      ++left_out;
      continue;
    }
    RobloxRelease release;
    release.version_code = std::stoll(tag);
    release.version_name = VersionNameOf(item.GetString("name"), release.version_code);
    if (release.version_name.empty()) release.version_name = tag;
    release.published_at = item.GetString("published_at");
    const std::string base_name = "com.roblox.client-" + tag + ".apk";
    const std::string split_name = "com.roblox.client-config.x86_64-" + tag + ".apk";
    RobloxPackageFile base;
    RobloxPackageFile split;
    if (const Json* assets = item.Find("assets"); assets != nullptr && assets->is_array()) {
      for (const Json& asset : assets->items()) {
        if (!asset.is_object()) continue;
        const std::string name = asset.GetString("name");
        if (name != base_name && name != split_name) continue;
        RobloxPackageFile file;
        file.name = name;
        file.url = asset.GetString("browser_download_url");
        file.sha256 = ParseGitHubAssetDigest(asset.GetString("digest"));
        const std::int64_t size = asset.GetInt("size", 0);
        file.size = size > 0 ? static_cast<std::uint64_t>(size) : 0;
        file.split = name == split_name;
        if (file.url != kGitHubAssetPrefix + tag + "/" + name || file.sha256.empty() || file.size == 0 ||
            file.size > kMaximumApkBytes) {
          continue;
        }
        (file.split ? split : base) = file;
      }
    }
    if (base.name.empty() || split.name.empty()) {
      ++left_out;
      continue;
    }
    release.files = {base, split};
    out.push_back(std::move(release));
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const RobloxRelease& a, const RobloxRelease& b) { return a.version_code > b.version_code; });
  *releases = std::move(out);
  if (skipped != nullptr) *skipped = left_out;
  return true;
}

bool ParsePinnedRobloxReleases(std::string_view text, std::vector<RobloxRelease>* releases, std::string* error) {
  const auto fail = [&](std::string message) {
    if (error != nullptr) *error = std::move(message);
    return false;
  };
  std::string parse_error;
  const auto json = Json::Parse(text, &parse_error);
  if (!json) return fail("it is not JSON: " + parse_error);
  if (!json->is_object()) return fail("it is not a JSON object");
  if (const Json* schema = json->Find("schema_version");
      schema == nullptr || !schema->is_number() || schema->AsInt(0) != 1) {
    return fail("its schema_version is not 1");
  }
  const Json* sources = json->Find("sources");
  if (sources == nullptr || !sources->is_array()) return fail("it has no \"sources\" list");
  std::vector<RobloxRelease> out;
  for (const Json& source : sources->items()) {
    if (!source.is_object() || source.GetString("package") != "com.roblox.client" ||
        source.GetString("abi") != "x86_64") {
      continue;
    }
    const std::int64_t code = source.GetInt("version_code", 0);
    if (code <= 0) return fail("an x86_64 entry has no version_code");
    const std::string tag = std::to_string(code);
    for (const RobloxRelease& earlier : out) {
      if (earlier.version_code == code) return fail("versionCode " + tag + " is pinned twice");
    }
    RobloxRelease release;
    release.pinned = true;
    release.version_code = code;
    release.version_name = TrimAscii(source.GetString("version_name"));
    if (release.version_name.empty()) release.version_name = tag;
    const std::string base_name = "com.roblox.client-" + tag + ".apk";
    const std::string split_name = "com.roblox.client-config.x86_64-" + tag + ".apk";
    const std::string base_url = kGitHubAssetPrefix + tag + "/" + base_name;
    const std::string split_url = kGitHubAssetPrefix + tag + "/" + split_name;
    const Json* archives = source.Find("archives");
    if (archives == nullptr || !archives->is_array() || archives->items().size() != 2) {
      return fail("versionCode " + tag + " does not list exactly two archives (" + base_name + " and " +
                  split_name + ")");
    }
    RobloxPackageFile base;
    RobloxPackageFile split;
    for (const Json& archive : archives->items()) {
      const std::string url = archive.is_object() ? archive.GetString("url") : std::string();
      if (url != base_url && url != split_url) {
        return fail("versionCode " + tag + " lists " + (url.empty() ? std::string("an archive without a URL") : url) +
                    ", which is not " + base_url + " or " + split_url);
      }
      RobloxPackageFile file;
      file.split = url == split_url;
      file.name = file.split ? split_name : base_name;
      file.url = url;
      const std::string sha256 = archive.GetString("sha256");
      if (!IsSha256Hex(sha256)) return fail(file.name + ": its sha256 is not 64 hexadecimal digits");
      file.sha256 = NormalizeSha256(sha256);
      const std::int64_t size = archive.GetInt("size", 0);
      if (size <= 0 || static_cast<std::uint64_t>(size) > kMaximumApkBytes) {
        return fail(file.name + ": its size is missing or larger than 1 GiB");
      }
      file.size = static_cast<std::uint64_t>(size);
      RobloxPackageFile& slot = file.split ? split : base;
      if (!slot.name.empty()) return fail(file.name + " is listed twice");
      slot = std::move(file);
    }
    release.files = {base, split};
    out.push_back(std::move(release));
  }
  if (out.empty()) return fail("it pins no com.roblox.client x86_64 release");
  std::stable_sort(out.begin(), out.end(),
                   [](const RobloxRelease& a, const RobloxRelease& b) { return a.version_code > b.version_code; });
  *releases = std::move(out);
  return true;
}

std::wstring LocateDownloadSources(const std::wstring& exe_dir) {
  return LocateConfigFile(exe_dir, L"roblox_download_sources.json");
}

GitHubApiFailure DescribeGitHubApiFailure(const GitHubApiResponse& response, std::int64_t received_at) {
  GitHubApiFailure failure;
  const std::int64_t server_now = response.date > 0 ? response.date : received_at;
  const auto local_time_of = [&](std::int64_t server_time) {
    return received_at + std::max<std::int64_t>(0, server_time - server_now);
  };
  constexpr wchar_t kTooManyRequests[] = ELIXIR_TR(L"Too many requests right now.");
  const std::wstring message = GitHubMessage(response.body);
  std::int64_t value = 0;
  if (response.status == 403 || response.status == 429) {
    if (TrimAscii(response.ratelimit_remaining) == "0") {
      failure.rate_limited = true;
      failure.reason = T(kTooManyRequests);
      if (ParseCount(response.ratelimit_reset, &value)) failure.retry_at = local_time_of(value);
      return failure;
    }
    if (!TrimAscii(response.retry_after).empty()) {
      failure.rate_limited = true;
      failure.reason = T(kTooManyRequests);
      if (ParseCount(response.retry_after, &value)) {
        failure.retry_at = received_at + value;
      } else if (const std::int64_t date = ParseHttpDate(response.retry_after); date > 0) {
        failure.retry_at = local_time_of(date);
      }
      return failure;
    }
    if (ToLowerAscii(WideToUtf8(message)).find("rate limit") != std::string::npos) {
      failure.rate_limited = true;
      failure.reason = T(kTooManyRequests);
      failure.retry_at = received_at + 60;
      return failure;
    }
  }
  if (response.status == 0) {
    failure.reason = T(L"Couldn’t connect. Check your internet connection and try again.");
    return failure;
  }
  if (!response.error.empty()) {
    failure.reason = T(L"The connection was interrupted. Try again.");
    return failure;
  }
  failure.reason = T(L"The version list isn’t available right now. Try again later.");
  return failure;
}

RobloxReleaseList ReleaseListFromGitHubResponse(const GitHubApiResponse& response, std::int64_t received_at) {
  RobloxReleaseList list;
  if (response.error.empty() && response.status >= 200 && response.status < 300) {
    std::string parse_error;
    if (!ParseGitHubReleases(response.body, &list.releases, &list.skipped, &parse_error)) {
      list.error = T(L"The version list couldn’t be read. Try again later.");
    } else if (list.releases.empty()) {
      list.error = T(L"No Roblox versions are available right now. Try again later.");
    } else {
      list.ok = true;
    }
    return list;
  }
  const GitHubApiFailure failure = DescribeGitHubApiFailure(response, received_at);
  list.error = failure.reason;
  list.rate_limited = failure.rate_limited;
  list.retry_at = failure.retry_at;
  return list;
}

namespace {

constexpr wchar_t kGitHubApiHost[] = L"api.github.com";
constexpr wchar_t kGitHubReleasesPath[] = L"/repos/Roblox-DeployHistory-Updates/android-runner/releases?per_page=30";
constexpr wchar_t kLatestAndroidVersionPath[] = L"/repos/Roblox-DeployHistory-Updates/android-runner/releases/latest";
constexpr wchar_t kGitHubApiHeaders[] = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
constexpr std::size_t kMaximumListBytes = 4u << 20;
constexpr ULONGLONG kListTimeoutMs = 30000;

struct InternetHandle {
  HINTERNET value = nullptr;
  explicit InternetHandle(HINTERNET handle = nullptr) : value(handle) {}
  InternetHandle(const InternetHandle&) = delete;
  InternetHandle& operator=(const InternetHandle&) = delete;
  ~InternetHandle() {
    if (value != nullptr) WinHttpCloseHandle(value);
  }
};

std::string HeaderText(HINTERNET request, DWORD info, const wchar_t* name) {
  DWORD size = 0;
  WinHttpQueryHeaders(request, info, name, WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0 || size > 4096) return {};
  std::wstring value(size / sizeof(wchar_t) + 1, L'\0');
  if (!WinHttpQueryHeaders(request, info, name, value.data(), &size, WINHTTP_NO_HEADER_INDEX)) return {};
  value.resize(size / sizeof(wchar_t));
  return TrimAscii(WideToUtf8(value));
}

std::string LastErrorText(const char* what) { return std::string(what) + ": " + WideToUtf8(Win32ErrorText(GetLastError())); }

bool Cancelled(const std::atomic<bool>* cancel) { return cancel != nullptr && cancel->load(); }

GitHubApiResponse GetGitHubReleasesOnce(HINTERNET session, const wchar_t* path, const std::atomic<bool>* cancel,
                                        bool* retryable) {
  GitHubApiResponse response;
  *retryable = false;
  InternetHandle connection(WinHttpConnect(session, kGitHubApiHost, INTERNET_DEFAULT_HTTPS_PORT, 0));
  if (connection.value == nullptr) {
    response.error = LastErrorText("cannot connect to api.github.com");
    *retryable = true;
    return response;
  }
  InternetHandle request(WinHttpOpenRequest(connection.value, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (request.value == nullptr) {
    response.error = LastErrorText("cannot open the request");
    return response;
  }
  DWORD feature = WINHTTP_DISABLE_REDIRECTS;
  WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof(feature));
  DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
  WinHttpSetOption(request.value, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
  if (!WinHttpSendRequest(request.value, kGitHubApiHeaders, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0,
                          0) ||
      !WinHttpReceiveResponse(request.value, nullptr)) {
    response.error = LastErrorText("the request to api.github.com failed");
    *retryable = true;
    return response;
  }
  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
  response.status = static_cast<long>(status);
  response.ratelimit_limit = HeaderText(request.value, WINHTTP_QUERY_CUSTOM, L"x-ratelimit-limit");
  response.ratelimit_remaining = HeaderText(request.value, WINHTTP_QUERY_CUSTOM, L"x-ratelimit-remaining");
  response.ratelimit_reset = HeaderText(request.value, WINHTTP_QUERY_CUSTOM, L"x-ratelimit-reset");
  response.retry_after = HeaderText(request.value, WINHTTP_QUERY_RETRY_AFTER, WINHTTP_HEADER_NAME_BY_INDEX);
  SYSTEMTIME date{};
  DWORD date_size = sizeof(date);
  if (WinHttpQueryHeaders(request.value, WINHTTP_QUERY_DATE | WINHTTP_QUERY_FLAG_SYSTEMTIME,
                          WINHTTP_HEADER_NAME_BY_INDEX, &date, &date_size, WINHTTP_NO_HEADER_INDEX)) {
    response.date = UnixTimeOf(date);
  }
  const bool transient_status = response.status >= 500 || response.status == 408;
  const bool interrupted_may_retry = transient_status || (response.status >= 200 && response.status < 300);
  std::vector<char> buffer(64 * 1024);
  const ULONGLONG started = GetTickCount64();
  for (;;) {
    if (Cancelled(cancel)) {
      response.error = "cancelled";
      return response;
    }
    DWORD read = 0;
    if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
      response.error = LastErrorText("reading the answer failed");
      *retryable = interrupted_may_retry;
      return response;
    }
    if (read == 0) break;
    if (response.body.size() + read > kMaximumListBytes) {
      response.error = "the answer is larger than 4 MB";
      return response;
    }
    response.body.append(buffer.data(), read);
    if (GetTickCount64() - started > kListTimeoutMs) {
      response.error = "the answer took longer than 30 seconds";
      *retryable = interrupted_may_retry;
      return response;
    }
  }
  *retryable = transient_status;
  return response;
}

GitHubApiResponse GetGitHubReleases(const wchar_t* path, const std::atomic<bool>* cancel) {
  GitHubApiResponse response;
  const std::wstring agent = L"Elixir-Launcher/" + Utf8ToWide(ELIXIR_VERSION);
  InternetHandle session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0));
  if (session.value == nullptr) {
    session.value =
        WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  }
  if (session.value == nullptr) {
    response.error = LastErrorText("WinHttpOpen failed");
    return response;
  }
  DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
  if (!WinHttpSetOption(session.value, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
    protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(session.value, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
  }
  WinHttpSetTimeouts(session.value, 10000, 10000, 30000, 30000);
  for (int attempt = 1;; ++attempt) {
    bool retryable = false;
    if (Cancelled(cancel)) return response;
    response = GetGitHubReleasesOnce(session.value, path, cancel, &retryable);
    if (!retryable || attempt >= 2 || Cancelled(cancel)) return response;
    const ULONGLONG resume = GetTickCount64() + 1500;
    while (GetTickCount64() < resume && !Cancelled(cancel)) Sleep(50);
    if (Cancelled(cancel)) return response;
  }
}

}

RobloxReleaseList LatestRobloxVersionFromGitHubResponse(const GitHubApiResponse& response, std::int64_t received_at) {
  RobloxReleaseList list;
  if (!response.error.empty() || response.status != 200) {
    const GitHubApiFailure failure = DescribeGitHubApiFailure(response, received_at);
    list.error = failure.reason;
    list.rate_limited = failure.rate_limited;
    list.retry_at = failure.retry_at;
    return list;
  }
  std::string parse_error;
  const auto json = Json::Parse(response.body, &parse_error);
  if (!json || !json->is_object() || json->GetBool("draft", true) || json->GetBool("prerelease", true)) {
    list.error = T(L"The Android release tracker didn’t return a stable version. Try again later.");
    return list;
  }
  RobloxRelease release;
  if (!ParseCount(json->GetString("tag_name"), &release.version_code) || release.version_code <= 0 ||
      release.version_code > INT32_MAX) {
    list.error = T(L"The Android release tracker returned an invalid version code. Try again later.");
    return list;
  }
  release.version_name = VersionNameOf(json->GetString("name"), release.version_code);
  if (release.version_name.empty() || release.version_name.size() > 80 ||
      release.version_name.find_first_not_of("0123456789.") != std::string::npos) {
    release.version_name = std::to_string(release.version_code);
  }
  release.published_at = json->GetString("published_at");
  list.releases.push_back(std::move(release));
  list.ok = true;
  return list;
}

RobloxReleaseList FetchRobloxVersions(const std::atomic<bool>* cancel) {
  const GitHubApiResponse response = GetGitHubReleases(kLatestAndroidVersionPath, cancel);
  if (Cancelled(cancel)) {
    RobloxReleaseList list;
    list.error = L"Cancelled.";
    return list;
  }
  return LatestRobloxVersionFromGitHubResponse(response, UnixNow());
}

RobloxReleaseList FetchRobloxReleases(const std::atomic<bool>* cancel) {
#if defined(ELIXIR_WITH_ROBLOX_DOWNLOAD)
  const GitHubApiResponse response = GetGitHubReleases(kGitHubReleasesPath, cancel);
  if (Cancelled(cancel)) {
    RobloxReleaseList list;
    list.error = L"Cancelled.";
    return list;
  }
  return ReleaseListFromGitHubResponse(response, UnixNow());
#else
  (void)cancel;
  RobloxReleaseList list;
  list.error = T(L"Downloading Roblox isn’t available in this version of Elixir.");
  return list;
#endif
}

RobloxReleaseList FetchAllRobloxReleases(const std::atomic<bool>* cancel) {
#if defined(ELIXIR_WITH_ROBLOX_DOWNLOAD)
  constexpr std::size_t kPageSize = 100;
  constexpr int kMaximumPages = 5;
  RobloxReleaseList all;
  for (int page = 1; page <= kMaximumPages; ++page) {
    const std::wstring path = L"/repos/Roblox-DeployHistory-Updates/android-runner/releases?per_page=" +
                              std::to_wstring(kPageSize) + L"&page=" + std::to_wstring(page);
    const GitHubApiResponse response = GetGitHubReleases(path.c_str(), cancel);
    if (Cancelled(cancel)) {
      RobloxReleaseList list;
      list.error = L"Cancelled.";
      return list;
    }
    RobloxReleaseList list = ReleaseListFromGitHubResponse(response, UnixNow());
    if (!list.ok) {
      if (page == 1) return list;
      break;
    }
    const std::size_t in_page = list.releases.size() + list.skipped;
    if (page == 1) {
      all = std::move(list);
    } else {
      all.releases.insert(all.releases.end(), std::make_move_iterator(list.releases.begin()),
                          std::make_move_iterator(list.releases.end()));
      all.skipped += list.skipped;
    }
    if (in_page < kPageSize) break;
  }
  std::stable_sort(all.releases.begin(), all.releases.end(),
                   [](const RobloxRelease& a, const RobloxRelease& b) { return a.version_code > b.version_code; });
  return all;
#else
  return FetchRobloxReleases(cancel);
#endif
}

RobloxReleaseList WithPinnedFallback(RobloxReleaseList list, const std::wstring& pinned_sources) {
  if (list.ok) return list;
  if (pinned_sources.empty()) return list;
  std::string bytes;
  if (!ReadFileBytes(pinned_sources, &bytes, nullptr, 1u << 20)) return list;
  std::vector<RobloxRelease> pinned;
  if (!ParsePinnedRobloxReleases(bytes, &pinned, nullptr)) return list;
  list.releases = std::move(pinned);
  list.skipped = 0;
  list.pinned = true;
  list.ok = true;
  return list;
}

std::int64_t UnixNow() {
  FILETIME now{};
  GetSystemTimeAsFileTime(&now);
  return UnixTimeOf(now);
}

std::wstring FormatLocalClockTime(std::int64_t unix_time, std::int64_t now) {
  const auto to_local = [](std::int64_t time, SYSTEMTIME* local) {
    if (time <= 0) return false;
    ULARGE_INTEGER value;
    value.QuadPart = static_cast<ULONGLONG>(time) * 10000000ull + kUnixEpochAsFileTime;
    FILETIME file_time{value.LowPart, value.HighPart};
    SYSTEMTIME utc{};
    return FileTimeToSystemTime(&file_time, &utc) && SystemTimeToTzSpecificLocalTime(nullptr, &utc, local);
  };
  SYSTEMTIME when{};
  if (!to_local((unix_time + 59) / 60 * 60, &when)) return L"?";
  SYSTEMTIME today{};
  wchar_t text[32];
  if (to_local(now, &today) && today.wYear == when.wYear && today.wMonth == when.wMonth && today.wDay == when.wDay) {
    std::swprintf(text, 32, L"%02u:%02u", when.wHour, when.wMinute);
  } else {
    std::swprintf(text, 32, L"%04u-%02u-%02u %02u:%02u", when.wYear, when.wMonth, when.wDay, when.wHour, when.wMinute);
  }
  return text;
}

std::wstring LocateSigningPins(const std::wstring& exe_dir) {
  return LocateConfigFile(exe_dir, L"roblox_signing_certificates.json");
}

bool LoadSigningPins(const std::wstring& path, SigningPins* pins, std::wstring* error) {
  const auto fail = [&](std::wstring message) {
    if (error != nullptr) *error = std::move(message);
    return false;
  };
  constexpr wchar_t kDamaged[] = ELIXIR_TR(L"Part of Elixir is damaged. Reinstall Elixir.");
  if (path.empty()) return fail(T(L"Part of Elixir is missing. Reinstall Elixir."));
  std::string bytes;
  if (!ReadFileBytes(path, &bytes, nullptr, 1u << 20)) return fail(T(kDamaged));
  std::string parse_error;
  const auto json = Json::Parse(bytes, &parse_error);
  if (!json || !json->is_object()) return fail(T(kDamaged));
  SigningPins parsed;
  parsed.path = path;
  parsed.package = json->GetString("package");
  if (const Json* trusted = json->Find("trusted_sha256"); trusted != nullptr && trusted->is_array()) {
    for (const Json& value : trusted->items()) {
      if (!value.is_string() || !IsSha256Hex(value.string_ref())) return fail(T(kDamaged));
      parsed.sha256.push_back(NormalizeSha256(value.string_ref()));
    }
  }
  if (parsed.package.empty() || parsed.sha256.empty()) return fail(T(kDamaged));
  *pins = std::move(parsed);
  return true;
}

std::wstring RobloxDownloadDirectory(const std::wstring& downloads_dir, const RobloxRelease& release) {
  return JoinPath(JoinPath(downloads_dir, L"roblox"), L"github-" + std::to_wstring(release.version_code));
}

bool AcceptDownloadedFile(const std::wstring& path, const RobloxPackageFile& published, std::string* measured,
                          std::wstring* error) {
  const auto reject = [&](const std::wstring& message) {
    DeleteFileW(path.c_str());
    if (error != nullptr) *error = message;
    return false;
  };
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) ||
      (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
    if (error != nullptr) *error = T(L"The downloaded file is missing. Try again.");
    return false;
  }
  const std::uint64_t size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  std::string digest;
  std::wstring hash_error;
  if (!Sha256File(path, &digest, &hash_error)) return reject(T(L"The download couldn’t be checked. Try again."));
  if (measured != nullptr) *measured = digest;
  if (published.sha256.empty()) return reject(T(L"The download couldn’t be checked. Try again."));
  if ((published.size != 0 && size != published.size) || !Sha256Equal(digest, published.sha256)) {
    return reject(T(L"The download was damaged. Try again."));
  }
  return true;
}

void DeleteRobloxDownload(const std::wstring& directory, const std::wstring& downloads_dir) {
  if (directory.empty() || downloads_dir.empty()) return;
  const std::wstring prefix = JoinPath(downloads_dir, L"roblox") + L"\\";
  if (directory.size() <= prefix.size() || _wcsnicmp(directory.c_str(), prefix.c_str(), prefix.size()) != 0 ||
      directory.find(L"..") != std::wstring::npos) {
    return;
  }
  if (!IsRealDirectoryBelow(downloads_dir, directory)) return;
  DeleteDirectoryTree(directory);
}

#if defined(ELIXIR_WITH_ROBLOX_DOWNLOAD)
namespace {

bool SafeFileName(std::string_view name) {
  if (name.empty() || name.size() > 128 || name.front() == '.') return false;
  for (const char c : name) {
    const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                         c == '_' || c == '-';
    if (!allowed) return false;
  }
  return name.size() > 4 && name.compare(name.size() - 4, 4, ".apk") == 0;
}

std::string HttpsHostOf(std::string_view url) {
  constexpr std::string_view kScheme = "https://";
  if (url.size() <= kScheme.size() || ToLowerAscii(url.substr(0, kScheme.size())) != kScheme) return {};
  std::string_view rest = url.substr(kScheme.size());
  rest = rest.substr(0, rest.find_first_of("/?#"));
  if (rest.empty() || rest.find_first_of("@:") != std::string_view::npos) return {};
  std::string host = ToLowerAscii(rest);
  while (!host.empty() && host.back() == '.') host.pop_back();
  return host;
}

std::string AssetHostList() {
  std::string joined;
  for (const std::string& host : RobloxAssetHosts()) joined += (joined.empty() ? "" : ", ") + host;
  return joined;
}

std::string FormatBytes(std::uint64_t bytes) { return WideToUtf8(FormatByteSize(bytes)); }

void AppendLog(std::string* log, const std::string& line) { *log += line + "\n"; }

}
#endif

RobloxDownloadResult DownloadRobloxRelease(const RobloxRelease& release, const std::wstring& downloads_dir,
                                           const RobloxProgressCallback& progress, const std::atomic<bool>* cancel) {
  RobloxDownloadResult result;
  result.directory = RobloxDownloadDirectory(downloads_dir, release);
#if defined(ELIXIR_WITH_ROBLOX_DOWNLOAD)
  if (release.files.empty()) {
    result.error = T(L"This version can’t be downloaded.");
    return result;
  }
  const std::string tag = std::to_string(release.version_code);
  for (const RobloxPackageFile& file : release.files) {
    if (!SafeFileName(file.name) || !IsSha256Hex(file.sha256) || file.size == 0 || file.size > kMaximumApkBytes ||
        file.url != kGitHubAssetPrefix + tag + "/" + file.name) {
      AppendLog(&result.log, "refused " + file.name + " (" + file.url + ")");
      result.error = T(L"This version can’t be downloaded.");
      return result;
    }
  }
  if (!EnsureDirectory(result.directory, &result.error)) return result;
  const auto report = [&](RobloxDownloadProgress value) {
    if (progress) progress(value);
  };
  const std::vector<std::string>& hosts = RobloxAssetHosts();
  const int count = static_cast<int>(release.files.size());
  for (int index = 0; index < count; ++index) {
    const RobloxPackageFile& file = release.files[static_cast<std::size_t>(index)];
    const std::wstring destination = JoinPath(result.directory, Utf8ToWide(file.name));
    DownloadedFile downloaded;
    downloaded.path = destination;
    downloaded.name = file.name;
    downloaded.published_sha256 = file.sha256;
    if (PathIsFile(destination)) {
      std::string measured;
      std::wstring hash_error;
      if (Sha256File(destination, &measured, &hash_error) && Sha256Equal(measured, file.sha256)) {
        downloaded.sha256 = measured;
        downloaded.size = file.size;
        downloaded.resumed = file.size;
        AppendLog(&result.log, "reused " + file.name + " (" + FormatBytes(file.size) + ", sha256 " + measured +
                                   " matches the published digest)");
        result.files.push_back(downloaded);
        report({"Already downloaded", index + 1, count, file.size, file.size});
        continue;
      }
      DeleteFileW(destination.c_str());
    }
    mocktail::update::HttpTransferRequest request;
    request.url = file.url;
    request.allowed_hosts = hosts;
    request.maximum_bytes = static_cast<std::size_t>(file.size);
    request.transfer_timeout_ms = 0;
    request.cancel = cancel;
    request.progress = [&, index, count](std::uint64_t done, std::uint64_t total) {
      report({"Downloading Roblox", index + 1, count, done, total > 0 ? total : file.size});
    };
    report({"Downloading Roblox", index + 1, count, 0, file.size});
    AppendLog(&result.log, "GET " + file.url);
    const mocktail::update::HttpDownloadResult downloaded_file =
        mocktail::update::DownloadFile(request, std::filesystem::path(destination));
    if (!downloaded_file) {
      result.cancelled = downloaded_file.cancelled;
      std::string reason = downloaded_file.error;
      if (reason.find("not trusted") != std::string::npos) {
        reason += " (Elixir downloads only from " + AssetHostList() + "; GitHub may have moved its release downloads)";
      }
      result.error = downloaded_file.cancelled ? L"Cancelled."
                                               : T(L"The download failed. Check your internet connection and try again.");
      AppendLog(&result.log, "  failed: " + reason);
      return result;
    }
    const std::string served_by = HttpsHostOf(downloaded_file.final_url);
    AppendLog(&result.log, "  " + std::to_string(downloaded_file.bytes_written) + " bytes" +
                               (downloaded_file.resumed_bytes > 0
                                    ? " (resumed after " + std::to_string(downloaded_file.resumed_bytes) + ")"
                                    : std::string()) +
                               " from " + downloaded_file.final_url.substr(0, downloaded_file.final_url.find('?')));
    if (std::find(hosts.begin(), hosts.end(), served_by) == hosts.end()) {
      DeleteFileW(destination.c_str());
      const std::string host = served_by.empty() ? std::string("an unknown host") : served_by;
      AppendLog(&result.log, "  rejected: served by " + host + ", not one of " + AssetHostList());
      result.error = T(L"The download didn’t come from the expected place. Try again later.");
      return result;
    }
    report({"Checking the download", index + 1, count, file.size, file.size});
    std::string on_disk;
    std::wstring mismatch;
    if (!Sha256Equal(downloaded_file.sha256, file.sha256) ||
        !AcceptDownloadedFile(destination, file, &on_disk, &mismatch)) {
      if (mismatch.empty()) {
        DeleteFileW(destination.c_str());
        mismatch = T(L"The download was damaged. Try again.");
      }
      AppendLog(&result.log, "  rejected: sha256 " + (on_disk.empty() ? downloaded_file.sha256 : on_disk) +
                                 ", published " + file.sha256);
      result.error = mismatch;
      return result;
    }
    AppendLog(&result.log, "  sha256 " + on_disk + " matches the published digest");
    downloaded.sha256 = on_disk;
    downloaded.size = downloaded_file.bytes_written;
    downloaded.resumed = downloaded_file.resumed_bytes;
    result.files.push_back(downloaded);
  }
  result.ok = true;
  return result;
#else
  (void)release;
  (void)progress;
  (void)cancel;
  result.error = T(L"Downloading Roblox isn’t available in this version of Elixir.");
  return result;
#endif
}

ApkResult VerifyRobloxPackage(const ApkComponent& component, const std::vector<std::wstring>& files,
                              const SigningPins& pins, const std::wstring& work_dir,
                              const ApkProgressCallback& progress, const std::atomic<bool>* cancel) {
  ApkResult result;
  if (pins.sha256.empty() || pins.path.empty()) {
    result.error = T(L"Part of Elixir is missing. Reinstall Elixir.");
    return result;
  }
  ApkComponent pinned = component;
  pinned.trust_manifest = pins.path;
  if (files.size() == 1 && (ExtensionOf(files.front()) == L".xapk" || ExtensionOf(files.front()) == L".apkm")) {
    result = ValidateApk(pinned, files.front(), work_dir, progress, cancel);
  } else {
    result = ValidateApkFiles(pinned, files, progress, cancel);
  }
  if (!result.ok) return result;
  const auto pinned_digest = [&](const std::string& digest) {
    return std::any_of(pins.sha256.begin(), pins.sha256.end(),
                       [&](const std::string& pin) { return Sha256Equal(pin, digest); });
  };
  const auto reject = [&](std::string detail, const std::wstring& message) {
    result.log += "rejected: " + detail + "\n";
    result.ok = false;
    result.metadata.signature_verified = false;
    result.metadata.signer_status = WideToUtf8(message);
    result.error = message;
    return result;
  };
  constexpr wchar_t kNotOfficial[] = ELIXIR_TR(L"This isn’t an official Roblox file.");
  if (result.metadata.package != pins.package) {
    return reject("package " + result.metadata.package + ", not " + pins.package, T(L"This file isn’t Roblox."));
  }
  if (!pinned_digest(result.metadata.signer_sha256)) {
    return reject("signer " + result.metadata.signer_sha256 + " is not pinned in " + WideToUtf8(pins.path),
                  T(kNotOfficial));
  }
  for (const ApkFileReport& report : result.reports) {
    if (report.certificates.empty() || !pinned_digest(report.certificates.front())) {
      const std::string signer = report.certificates.empty() ? std::string("none") : report.certificates.front();
      return reject(WideToUtf8(FileNameOf(report.path)) + ": signer " + signer + " is not pinned", T(kNotOfficial));
    }
  }
  if (!result.metadata.has_x86_64_engine) {
    return reject("missing x86-64 Roblox engine", T(L"This package doesn’t contain the x86-64 Roblox engine."));
  }
  result.metadata.signer_status = "Official Roblox file";
  return result;
}

}
