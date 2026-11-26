// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/update.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "core/config.h"
#include "core/sha256.h"
#include "core/util.h"

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "0.0.0"
#endif

namespace elixir::launcher {

namespace {

constexpr wchar_t kCannotConnect[] = L"Couldn’t connect. Check your internet connection and try again.";
constexpr wchar_t kInterrupted[] = L"The download was interrupted. Try again.";

std::wstring NetworkError(DWORD code, const wchar_t* fallback) {
  const std::wstring cause = ErrorCause(code);
  return cause.empty() ? std::wstring(fallback) : cause + L" Try again.";
}

struct WinHttpHandle {
  HINTERNET handle = nullptr;
  WinHttpHandle() = default;
  explicit WinHttpHandle(HINTERNET h) : handle(h) {}
  WinHttpHandle(const WinHttpHandle&) = delete;
  WinHttpHandle& operator=(const WinHttpHandle&) = delete;
  ~WinHttpHandle() {
    if (handle != nullptr) WinHttpCloseHandle(handle);
  }
};

using ChunkSink = std::function<bool(const char* data, std::size_t size)>;

HttpResult Fetch(const std::wstring& url, std::uint64_t max_bytes, const ChunkSink& sink,
                 const ProgressCallback& progress, const std::atomic<bool>* cancel) {
  HttpResult result;
  if (!IsHttpsUrl(WideToUtf8(url))) {
    result.error = L"The update link must start with https://.";
    return result;
  }
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwSchemeLength = static_cast<DWORD>(-1);
  parts.dwHostNameLength = static_cast<DWORD>(-1);
  parts.dwUrlPathLength = static_cast<DWORD>(-1);
  parts.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts)) {
    result.error = L"The update link isn’t valid.";
    return result;
  }
  if (parts.nScheme != INTERNET_SCHEME_HTTPS) {
    result.error = L"The update link must start with https://.";
    return result;
  }
  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.lpszExtraInfo != nullptr) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  if (path.empty()) path = L"/";

  const std::wstring agent = L"Elixir-Launcher/" + Utf8ToWide(ELIXIR_VERSION);
  WinHttpHandle session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0));
  if (session.handle == nullptr) {
    session.handle = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0);
  }
  if (session.handle == nullptr) {
    result.error = NetworkError(GetLastError(), kCannotConnect);
    return result;
  }
  DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
  if (!WinHttpSetOption(session.handle, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
    protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(session.handle, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
  }
  WinHttpSetTimeouts(session.handle, 15000, 15000, 30000, 60000);
  WinHttpHandle connection(WinHttpConnect(session.handle, host.c_str(), parts.nPort, 0));
  if (connection.handle == nullptr) {
    result.error = NetworkError(GetLastError(), kCannotConnect);
    return result;
  }
  WinHttpHandle request(WinHttpOpenRequest(connection.handle, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (request.handle == nullptr) {
    result.error = NetworkError(GetLastError(), kCannotConnect);
    return result;
  }
  DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
  WinHttpSetOption(request.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));
  if (!WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(request.handle, nullptr)) {
    result.error = NetworkError(GetLastError(), kCannotConnect);
    return result;
  }
  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
  result.status = status;
  if (status != 200) {
    result.error = L"The update isn’t available right now. Try again later.";
    return result;
  }
  std::uint64_t total = 0;
  wchar_t length_text[32] = {};
  DWORD length_size = sizeof(length_text);
  if (WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, length_text,
                          &length_size, WINHTTP_NO_HEADER_INDEX)) {
    total = _wcstoui64(length_text, nullptr, 10);
  }
  if (total > max_bytes) {
    result.error = L"The update is too large.";
    return result;
  }
  std::vector<char> buffer(64 * 1024);
  std::uint64_t done = 0;
  for (;;) {
    if (cancel != nullptr && cancel->load()) {
      result.error = L"Cancelled.";
      return result;
    }
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request.handle, &available)) {
      result.error = NetworkError(GetLastError(), kInterrupted);
      return result;
    }
    if (available == 0) break;
    while (available > 0) {
      const DWORD want = available < buffer.size() ? available : static_cast<DWORD>(buffer.size());
      DWORD read = 0;
      if (!WinHttpReadData(request.handle, buffer.data(), want, &read)) {
        result.error = NetworkError(GetLastError(), kInterrupted);
        return result;
      }
      if (read == 0) break;
      done += read;
      if (done > max_bytes) {
        result.error = L"The update is too large.";
        return result;
      }
      if (!sink(buffer.data(), read)) {
        result.error = L"The update couldn’t be saved.";
        return result;
      }
      available -= read;
      if (progress) progress(done, total);
    }
  }
  if (total != 0 && done != total) {
    result.error = kInterrupted;
    return result;
  }
  result.ok = true;
  return result;
}

bool IsReservedDeviceName(std::wstring_view name) {
  std::wstring_view stem = name.substr(0, name.find(L'.'));
  while (!stem.empty() && stem.back() == L' ') stem.remove_suffix(1);
  for (const wchar_t* reserved : {L"CON", L"PRN", L"AUX", L"NUL", L"CONIN$", L"CONOUT$"}) {
    if (PathsEqual(stem, reserved)) return true;
  }
  if (stem.size() == 4 && (StartsWithI(stem, L"COM") || StartsWithI(stem, L"LPT")) && stem[3] >= L'0' &&
      stem[3] <= L'9') {
    return true;
  }
  return false;
}

std::wstring SanitizeComponent(std::string_view text, std::wstring_view fallback) {
  std::wstring result;
  for (const char c : text) {
    const bool allowed =
        (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_';
    result.push_back(allowed ? static_cast<wchar_t>(c) : L'_');
    if (result.size() >= 96) break;
  }
  while (!result.empty() && result.front() == L'.') result.erase(result.begin());
  if (IsReservedDeviceName(result)) result.insert(result.begin(), L'_');
  return result.empty() ? std::wstring(fallback) : result;
}

void CleanupRecursive(const std::wstring& directory, int depth) {
  if (depth > 3) return;
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileW(JoinPath(directory, L"*").c_str(), &data);
  if (find == INVALID_HANDLE_VALUE) return;
  do {
    const std::wstring name = data.cFileName;
    if (name == L"." || name == L"..") continue;
    const std::wstring child = JoinPath(directory, name);
    if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (depth == 0 && PathsEqual(name, L"data")) continue;
      CleanupRecursive(child, depth + 1);
    } else if (name.size() > 11 && PathsEqual(std::wstring_view(name).substr(name.size() - 11), L".old-update")) {
      DeleteFileW(child.c_str());
    }
  } while (FindNextFileW(find, &data));
  FindClose(find);
}

}

int CompareVersions(std::string_view a, std::string_view b) {
  const auto split = [](std::string_view v, std::string_view* core, std::string_view* pre) {
    const std::size_t plus = v.find('+');
    if (plus != std::string_view::npos) v = v.substr(0, plus);
    if (!v.empty() && (v.front() == 'v' || v.front() == 'V')) v.remove_prefix(1);
    const std::size_t dash = v.find('-');
    *core = v.substr(0, dash);
    *pre = dash == std::string_view::npos ? std::string_view() : v.substr(dash + 1);
  };
  std::string_view core_a, pre_a, core_b, pre_b;
  split(a, &core_a, &pre_a);
  split(b, &core_b, &pre_b);
  for (;;) {
    if (core_a.empty() && core_b.empty()) break;
    const auto next = [](std::string_view* core) -> unsigned long long {
      unsigned long long value = 0;
      while (!core->empty() && core->front() != '.') {
        if (core->front() >= '0' && core->front() <= '9') value = value * 10 + static_cast<unsigned>(core->front() - '0');
        core->remove_prefix(1);
      }
      if (!core->empty()) core->remove_prefix(1);
      return value;
    };
    const unsigned long long part_a = next(&core_a);
    const unsigned long long part_b = next(&core_b);
    if (part_a != part_b) return part_a < part_b ? -1 : 1;
  }
  if (pre_a.empty() != pre_b.empty()) return pre_a.empty() ? 1 : -1;
  if (pre_a == pre_b) return 0;
  return pre_a < pre_b ? -1 : 1;
}

HttpResult HttpsGet(const std::wstring& url, std::uint64_t max_bytes, const std::atomic<bool>* cancel) {
  HttpResult result;
  std::string body;
  result = Fetch(
      url, max_bytes,
      [&](const char* data, std::size_t size) {
        body.append(data, size);
        return true;
      },
      {}, cancel);
  result.body = std::move(body);
  return result;
}

std::wstring StagingFileNameForUrl(std::string_view url) {
  std::string_view path = url;
  const std::size_t scheme = path.find("://");
  if (scheme != std::string_view::npos) path.remove_prefix(scheme + 3);
  const std::size_t end = path.find_first_of("?#");
  if (end != std::string_view::npos) path = path.substr(0, end);
  const std::size_t slash = path.find('/');
  std::string_view name;
  if (slash != std::string_view::npos) {
    path.remove_prefix(slash);
    const std::size_t last = path.find_last_of('/');
    name = path.substr(last + 1);
  }
  return SanitizeComponent(name, L"elixir-update.bin");
}

bool VerifyAndPromote(const std::wstring& source, std::string_view expected_sha256, const std::wstring& destination,
                      std::string* measured, std::wstring* error) {
  std::string actual;
  std::wstring hash_error;
  if (!Sha256File(source, &actual, &hash_error)) {
    DeleteFileW(source.c_str());
    if (error) *error = L"The update couldn’t be checked. Try again.";
    return false;
  }
  if (measured != nullptr) *measured = actual;
  const std::string expected = NormalizeSha256(expected_sha256);
  if (!Sha256Equal(actual, expected)) {
    DeleteFileW(source.c_str());
    if (error) *error = L"The update was damaged during download. Try again.";
    return false;
  }
  if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    if (error) *error = WithErrorCause(L"The update couldn’t be saved.", GetLastError());
    DeleteFileW(source.c_str());
    return false;
  }
  return true;
}

StagedUpdate DownloadAndStage(const UpdateManifest& manifest, const std::wstring& updates_dir,
                              const ProgressCallback& progress, const std::atomic<bool>* cancel) {
  StagedUpdate staged;
  if (!IsHttpsUrl(manifest.url) || !IsSha256Hex(manifest.sha256)) {
    staged.error = L"The update information is incomplete.";
    return staged;
  }
  const std::wstring directory =
      JoinPath(JoinPath(updates_dir, L"staging"), SanitizeComponent(manifest.version, L"unknown"));
  if (!EnsureDirectory(directory, &staged.error)) return staged;
  const std::wstring final_path = JoinPath(directory, StagingFileNameForUrl(manifest.url));
  const std::wstring part_path = final_path + L".part";
  HANDLE file = CreateFileW(part_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    staged.error = WithErrorCause(L"The update couldn’t be saved.", GetLastError());
    return staged;
  }
  std::wstring write_error;
  const HttpResult result = Fetch(
      Utf8ToWide(manifest.url), 4ull << 30,
      [&](const char* data, std::size_t size) {
        DWORD written = 0;
        if (!WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr) || written != size) {
          write_error = ErrorCause(GetLastError());
          return false;
        }
        staged.bytes += written;
        return true;
      },
      progress, cancel);
  FlushFileBuffers(file);
  CloseHandle(file);
  if (!result.ok) {
    DeleteFileW(part_path.c_str());
    staged.error = result.error;
    if (!write_error.empty()) staged.error += L" " + write_error;
    return staged;
  }
  if (!VerifyAndPromote(part_path, manifest.sha256, final_path, &staged.sha256, &staged.error)) return staged;
  staged.file = final_path;
  staged.ok = true;
  return staged;
}

void CleanupAfterUpdate(const std::wstring& install_dir) { CleanupRecursive(install_dir, 0); }

}
