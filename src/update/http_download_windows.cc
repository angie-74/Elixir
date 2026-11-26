// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "update/http_download.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#define JSON_NOEXCEPTION 1
#include <nlohmann/json.hpp>

#include "update/payload_integrity.h"

namespace mocktail::update {
namespace {

constexpr wchar_t kUserAgent[] = L"Elixir-native-updater/1";
constexpr std::size_t kReadChunk = 256 * 1024;
constexpr std::size_t kMaximumStateBytes = 65536;

std::wstring Widen(std::string_view text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
  return wide;
}

std::string Narrow(std::wstring_view text) {
  if (text.empty()) return {};
  const int length =
      WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string narrow(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), length, nullptr,
                      nullptr);
  return narrow;
}

std::string Lower(std::string value) {
  for (char& character : value) {
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
  }
  return value;
}

bool HostMatches(std::string_view host, std::string_view allowed) {
  if (host == allowed) return true;
  return host.size() > allowed.size() && host[host.size() - allowed.size() - 1] == '.' &&
         host.substr(host.size() - allowed.size()) == allowed;
}

std::string ErrorText(DWORD code) {
  wchar_t* buffer = nullptr;
  DWORD length = 0;
  if (code >= WINHTTP_ERROR_BASE && code <= WINHTTP_ERROR_LAST) {
    length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
                            GetModuleHandleW(L"winhttp.dll"), code, 0, reinterpret_cast<wchar_t*>(&buffer), 0,
                            nullptr);
  }
  if (length == 0) {
    length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  }
  std::wstring message = length > 0 && buffer != nullptr ? std::wstring(buffer, length) : std::wstring();
  if (buffer != nullptr) LocalFree(buffer);
  while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ' ||
                              message.back() == L'.')) {
    message.pop_back();
  }
  std::string text = Narrow(message);
  if (text.empty()) text = "Windows error";
  return text + " (error " + std::to_string(code) + ")";
}

bool RetryableTransport(DWORD code) {
  switch (code) {
    case ERROR_WINHTTP_TIMEOUT:
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
    case ERROR_WINHTTP_CONNECTION_ERROR:
    case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:
    case ERROR_WINHTTP_RESEND_REQUEST:
      return true;
    default:
      return false;
  }
}

bool RetryableStatus(long status) { return status == 408 || status == 425 || status == 429 || status >= 500; }

bool RedirectStatus(long status) {
  return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

struct Handle {
  HINTERNET value = nullptr;
  Handle() = default;
  explicit Handle(HINTERNET handle) : value(handle) {}
  Handle(Handle&& other) noexcept : value(other.value) { other.value = nullptr; }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  Handle& operator=(Handle&&) = delete;
  ~Handle() {
    if (value != nullptr) WinHttpCloseHandle(value);
  }
};

struct ParsedUrl {
  std::wstring host;
  std::wstring path;
  INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
};

bool CrackUrl(std::string_view url, ParsedUrl* out, std::string* error) {
  const std::wstring wide = Widen(url);
  if (wide.empty() || wide.size() > 8192) {
    if (error != nullptr) *error = "URL is malformed";
    return false;
  }
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  wchar_t scheme[16] = {};
  std::vector<wchar_t> host(wide.size() + 1, L'\0');
  std::vector<wchar_t> path(wide.size() + 1, L'\0');
  std::vector<wchar_t> extra(wide.size() + 1, L'\0');
  std::vector<wchar_t> user(wide.size() + 1, L'\0');
  std::vector<wchar_t> password(wide.size() + 1, L'\0');
  parts.lpszScheme = scheme;
  parts.dwSchemeLength = 16;
  parts.lpszHostName = host.data();
  parts.dwHostNameLength = static_cast<DWORD>(host.size());
  parts.lpszUrlPath = path.data();
  parts.dwUrlPathLength = static_cast<DWORD>(path.size());
  parts.lpszExtraInfo = extra.data();
  parts.dwExtraInfoLength = static_cast<DWORD>(extra.size());
  parts.lpszUserName = user.data();
  parts.dwUserNameLength = static_cast<DWORD>(user.size());
  parts.lpszPassword = password.data();
  parts.dwPasswordLength = static_cast<DWORD>(password.size());
  if (!WinHttpCrackUrl(wide.c_str(), static_cast<DWORD>(wide.size()), 0, &parts)) {
    if (error != nullptr) *error = "URL is malformed";
    return false;
  }
  const std::size_t authority = wide.find(L"://");
  std::wstring_view rest = authority == std::wstring::npos ? std::wstring_view() : std::wstring_view(wide).substr(authority + 3);
  rest = rest.substr(0, rest.find_first_of(L"/?#"));
  const bool has_user = user[0] != L'\0' || password[0] != L'\0' || rest.find(L'@') != std::wstring_view::npos;
  if (parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != INTERNET_DEFAULT_HTTPS_PORT || has_user ||
      host[0] == L'\0') {
    if (error != nullptr) *error = "URL must be credential-free HTTPS on port 443";
    return false;
  }
  out->host.assign(host.data());
  out->path.assign(path.data());
  out->path += extra.data();
  if (out->path.empty()) out->path = L"/";
  const std::size_t fragment = out->path.find(L'#');
  if (fragment != std::wstring::npos) out->path.resize(fragment);
  out->port = parts.nPort;
  return true;
}

std::string NormalizedHost(const std::wstring& host) {
  std::string normalized = Lower(Narrow(host));
  while (!normalized.empty() && normalized.back() == '.') normalized.pop_back();
  return normalized;
}

std::string StatusFailure(std::string_view action, std::string_view url, long status) {
  ParsedUrl parsed;
  const std::string host = CrackUrl(url, &parsed, nullptr) ? NormalizedHost(parsed.host) : std::string();
  return (host.empty() ? std::string("HTTPS ") + std::string(action) : host + " " + std::string(action)) +
         " returned status " + std::to_string(status);
}

bool Cancelled(const HttpTransferRequest& request) {
  return request.cancel != nullptr && request.cancel->load(std::memory_order_relaxed);
}

void SleepUnlessCancelled(const HttpTransferRequest& request, long milliseconds) {
  const ULONGLONG deadline = GetTickCount64() + static_cast<ULONGLONG>(std::max(0L, milliseconds));
  while (GetTickCount64() < deadline && !Cancelled(request)) Sleep(50);
}

Handle OpenSession(const HttpTransferRequest& request) {
  Handle session(WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                             WINHTTP_NO_PROXY_BYPASS, 0));
  if (session.value == nullptr) {
    session.value =
        WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  }
  if (session.value == nullptr) return session;
  DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
  if (!WinHttpSetOption(session.value, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
    protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(session.value, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
  }
  const int connect = static_cast<int>(std::clamp(request.connect_timeout_ms, 1000L, 120000L));
  const int receive = static_cast<int>(std::clamp(request.low_speed_seconds * 1000L, 5000L, 300000L));
  WinHttpSetTimeouts(session.value, connect, connect, 30000, receive);
  return session;
}

struct Response {
  long status = 0;
  std::string etag;
  std::string encoding;
  std::string range;
  std::string location;
  std::uint64_t content_length = 0;
  bool has_content_length = false;
};

std::string QueryHeader(HINTERNET request, DWORD info) {
  DWORD size = 0;
  WinHttpQueryHeaders(request, info, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size,
                      WINHTTP_NO_HEADER_INDEX);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0 || size > 65536) return {};
  std::wstring value(size / sizeof(wchar_t) + 1, L'\0');
  if (!WinHttpQueryHeaders(request, info, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size,
                           WINHTTP_NO_HEADER_INDEX)) {
    return {};
  }
  value.resize(size / sizeof(wchar_t));
  return Narrow(value);
}

std::string Trim(std::string value) {
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
    value.pop_back();
  std::size_t start = 0;
  while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) ++start;
  return value.substr(start);
}

Response ReadResponse(HINTERNET request) {
  Response response;
  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                      &status, &status_size, WINHTTP_NO_HEADER_INDEX);
  response.status = static_cast<long>(status);
  response.etag = Trim(QueryHeader(request, WINHTTP_QUERY_ETAG));
  response.encoding = Lower(Trim(QueryHeader(request, WINHTTP_QUERY_CONTENT_ENCODING)));
  response.range = Trim(QueryHeader(request, WINHTTP_QUERY_CONTENT_RANGE));
  response.location = Trim(QueryHeader(request, WINHTTP_QUERY_LOCATION));
  const std::string length = Trim(QueryHeader(request, WINHTTP_QUERY_CONTENT_LENGTH));
  if (!length.empty() && length.size() <= 19 &&
      std::all_of(length.begin(), length.end(), [](char c) { return c >= '0' && c <= '9'; })) {
    response.content_length = std::stoull(length);
    response.has_content_length = true;
  }
  return response;
}

std::string ResolveLocation(std::string_view current, const std::string& location) {
  if (location.rfind("https://", 0) == 0 || location.rfind("http://", 0) == 0 ||
      location.find("://") != std::string::npos) {
    return location;
  }
  ParsedUrl parsed;
  if (location.empty() || !CrackUrl(current, &parsed, nullptr)) return {};
  if (location.rfind("//", 0) == 0) return "https:" + location;
  const std::string origin = "https://" + Narrow(parsed.host);
  if (location.front() == '/') return origin + location;
  std::string base = Narrow(parsed.path);
  base = base.substr(0, base.find('?'));
  base = base.substr(0, base.rfind('/') + 1);
  return origin + (base.empty() ? "/" : base) + location;
}

struct Attempt {
  Handle connection;
  Handle request;
  Response response;
  DWORD transport_error = 0;
  std::string error;
};

bool Send(HINTERNET session, const HttpTransferRequest& request, std::string_view url,
          const std::vector<std::string>& extra_headers, bool decompress, Attempt* attempt) {
  ParsedUrl parsed;
  if (!CrackUrl(url, &parsed, &attempt->error)) return false;
  attempt->connection.value = WinHttpConnect(session, parsed.host.c_str(), parsed.port, 0);
  if (attempt->connection.value == nullptr) {
    attempt->transport_error = GetLastError();
    attempt->error = ErrorText(attempt->transport_error);
    return false;
  }
  attempt->request.value = WinHttpOpenRequest(attempt->connection.value, L"GET", parsed.path.c_str(), nullptr,
                                              WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (attempt->request.value == nullptr) {
    attempt->transport_error = GetLastError();
    attempt->error = ErrorText(attempt->transport_error);
    return false;
  }
  DWORD feature = WINHTTP_DISABLE_REDIRECTS;
  WinHttpSetOption(attempt->request.value, WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof(feature));
  if (decompress) {
    DWORD flags = WINHTTP_DECOMPRESSION_FLAG_ALL;
    WinHttpSetOption(attempt->request.value, WINHTTP_OPTION_DECOMPRESSION, &flags, sizeof(flags));
  }
  std::wstring headers;
  for (const std::string& header : request.headers) headers += Widen(header) + L"\r\n";
  for (const std::string& header : extra_headers) headers += Widen(header) + L"\r\n";
  if (!headers.empty() &&
      !WinHttpAddRequestHeaders(attempt->request.value, headers.c_str(), static_cast<DWORD>(headers.size()),
                                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
    attempt->error = "cannot set HTTP request headers";
    return false;
  }
  if (!WinHttpSendRequest(attempt->request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0,
                          0) ||
      !WinHttpReceiveResponse(attempt->request.value, nullptr)) {
    attempt->transport_error = GetLastError();
    attempt->error = ErrorText(attempt->transport_error);
    return false;
  }
  attempt->response = ReadResponse(attempt->request.value);
  return true;
}

enum class ReadOutcome { kDone, kSinkFailed, kTransportError, kStalled, kTimedOut, kCancelled };

template <typename Sink>
ReadOutcome ReadBody(HINTERNET handle, const HttpTransferRequest& request, ULONGLONG started, Sink&& sink,
                     DWORD* transport_error) {
  std::vector<char> buffer(kReadChunk);
  ULONGLONG window_start = GetTickCount64();
  std::uint64_t window_bytes = 0;
  for (;;) {
    if (Cancelled(request)) return ReadOutcome::kCancelled;
    DWORD read = 0;
    if (!WinHttpReadData(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
      *transport_error = GetLastError();
      return *transport_error == ERROR_WINHTTP_TIMEOUT ? ReadOutcome::kStalled : ReadOutcome::kTransportError;
    }
    if (read == 0) return ReadOutcome::kDone;
    if (!sink(buffer.data(), static_cast<std::size_t>(read))) return ReadOutcome::kSinkFailed;
    const ULONGLONG now = GetTickCount64();
    window_bytes += read;
    if (request.low_speed_seconds > 0 &&
        now - window_start >= static_cast<ULONGLONG>(request.low_speed_seconds) * 1000ull) {
      if (window_bytes < static_cast<std::uint64_t>(std::max(0L, request.low_speed_bytes_per_second)) *
                             static_cast<std::uint64_t>(request.low_speed_seconds)) {
        return ReadOutcome::kStalled;
      }
      window_start = now;
      window_bytes = 0;
    }
    if (request.transfer_timeout_ms > 0 && now - started > static_cast<ULONGLONG>(request.transfer_timeout_ms)) {
      return ReadOutcome::kTimedOut;
    }
  }
}

HttpBytesResult DownloadBytesAttempt(HINTERNET session, const HttpTransferRequest& request, bool* retryable) {
  HttpBytesResult result;
  std::string current = request.url;
  const ULONGLONG started = GetTickCount64();
  for (int redirect = 0; redirect <= request.maximum_redirects; ++redirect) {
    Attempt attempt;
    if (!Send(session, request, current, {}, true, &attempt)) {
      result.error = attempt.error;
      *retryable = attempt.transport_error != 0 && RetryableTransport(attempt.transport_error);
      return result;
    }
    result.status_code = attempt.response.status;
    if (RedirectStatus(result.status_code)) {
      const std::string next = ResolveLocation(current, attempt.response.location);
      if (next.empty() || redirect == request.maximum_redirects ||
          !IsTrustedHttpsUrl(next, request.allowed_hosts, &result.error)) {
        if (result.error.empty()) result.error = "invalid HTTPS redirect";
        return result;
      }
      current = next;
      continue;
    }
    if (result.status_code < 200 || result.status_code >= 300) {
      result.error = StatusFailure("request", current, result.status_code);
      *retryable = RetryableStatus(result.status_code);
      return result;
    }
    if (attempt.response.has_content_length && attempt.response.encoding.empty() &&
        attempt.response.content_length > request.maximum_bytes) {
      result.error = "HTTPS response exceeds its size limit";
      return result;
    }
    bool exceeded = false;
    DWORD transport_error = 0;
    const ReadOutcome outcome = ReadBody(
        attempt.request.value, request, started,
        [&](const char* data, std::size_t size) {
          if (size > request.maximum_bytes || result.bytes.size() > request.maximum_bytes - size) {
            exceeded = true;
            return false;
          }
          result.bytes.append(data, size);
          return true;
        },
        &transport_error);
    switch (outcome) {
      case ReadOutcome::kDone:
        result.final_url = current;
        return result;
      case ReadOutcome::kSinkFailed:
        result.error = exceeded ? "HTTPS response exceeds its size limit" : "cannot store HTTPS response";
        return result;
      case ReadOutcome::kCancelled:
        result.error = "cancelled";
        return result;
      case ReadOutcome::kStalled:
        result.error = "HTTPS transfer stalled";
        *retryable = true;
        return result;
      case ReadOutcome::kTimedOut:
        result.error = "HTTPS transfer took too long";
        *retryable = true;
        return result;
      case ReadOutcome::kTransportError:
        result.error = ErrorText(transport_error);
        *retryable = RetryableTransport(transport_error);
        return result;
    }
  }
  result.error = "too many HTTPS redirects";
  return result;
}

bool StrongEtag(std::string_view value) {
  if (value.size() < 2 || value.front() != '"' || value.back() != '"') return false;
  for (const unsigned char character : value) {
    if (character < 0x21 || character == 0x7f) return false;
  }
  return true;
}

bool ParseContentRange(std::string_view range, std::uint64_t offset, std::uint64_t maximum, std::uint64_t* total) {
  if (range.substr(0, 6) != "bytes ") return false;
  range.remove_prefix(6);
  std::uint64_t values[3] = {};
  for (int index = 0; index < 3; ++index) {
    std::size_t digits = 0;
    std::uint64_t value = 0;
    while (digits < range.size() && range[digits] >= '0' && range[digits] <= '9') {
      if (value > (std::numeric_limits<std::uint64_t>::max() - 9) / 10) return false;
      value = value * 10 + static_cast<std::uint64_t>(range[digits] - '0');
      ++digits;
    }
    if (digits == 0) return false;
    values[index] = value;
    range.remove_prefix(digits);
    if (index < 2) {
      if (range.empty() || range.front() != (index == 0 ? '-' : '/')) return false;
      range.remove_prefix(1);
    }
  }
  *total = values[2];
  return range.empty() && values[0] == offset && values[1] >= values[0] && values[1] + 1 == values[2] &&
         values[2] <= maximum;
}

std::wstring PathText(const std::filesystem::path& path) { return path.wstring(); }

bool FileSize(const std::filesystem::path& path, std::uint64_t* size) {
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(PathText(path).c_str(), GetFileExInfoStandard, &data)) return false;
  if (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) return false;
  *size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  return true;
}

void RemoveFile(const std::filesystem::path& path) { DeleteFileW(PathText(path).c_str()); }

nlohmann::json ReadState(const std::filesystem::path& path) {
  HANDLE file = CreateFileW(PathText(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (file == INVALID_HANDLE_VALUE) return nullptr;
  LARGE_INTEGER size{};
  std::string bytes;
  if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= static_cast<LONGLONG>(kMaximumStateBytes)) {
    bytes.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    if (!ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size()) {
      bytes.clear();
    }
  }
  CloseHandle(file);
  if (bytes.empty()) return nullptr;
  nlohmann::json parsed = nlohmann::json::parse(bytes, nullptr, false);
  return parsed.is_object() ? parsed : nlohmann::json(nullptr);
}

bool WriteState(const std::filesystem::path& path, const std::string& url, const std::string& etag) {
  const std::string bytes = nlohmann::json{{"url", url}, {"etag", etag}}.dump();
  const std::wstring temporary = PathText(path) + L".tmp";
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
                  written == bytes.size() && FlushFileBuffers(file);
  CloseHandle(file);
  if (!ok || !MoveFileExW(temporary.c_str(), PathText(path).c_str(), MOVEFILE_REPLACE_EXISTING)) {
    DeleteFileW(temporary.c_str());
    return false;
  }
  return true;
}

class ProgressReporter {
 public:
  explicit ProgressReporter(const HttpTransferRequest& request) : request_(request) {}
  void Report(std::uint64_t done, std::uint64_t total, bool force) {
    if (!request_.progress) return;
    const ULONGLONG now = GetTickCount64();
    if (!force && now - last_ < 200) return;
    last_ = now;
    request_.progress(done, total);
  }

 private:
  const HttpTransferRequest& request_;
  ULONGLONG last_ = 0;
};

HttpDownloadResult DownloadFileAttempt(HINTERNET session, const HttpTransferRequest& request,
                                       const std::filesystem::path& temporary, bool* retryable) {
  HttpDownloadResult result;
  const std::filesystem::path state_path = temporary.wstring() + L".json";
  const nlohmann::json state = ReadState(state_path);
  std::string etag;
  if (state.is_object() && state.contains("url") && state["url"].is_string() &&
      state["url"].get<std::string>() == request.url && state.contains("etag") && state["etag"].is_string()) {
    etag = state["etag"].get<std::string>();
  }
  std::uint64_t existing = 0;
  if (!FileSize(temporary, &existing) || existing > request.maximum_bytes || !StrongEtag(etag)) {
    RemoveFile(temporary);
    RemoveFile(state_path);
    existing = 0;
    etag.clear();
  }
  result.resumed_bytes = static_cast<std::size_t>(existing);
  ProgressReporter progress(request);
  std::string current = request.url;
  const ULONGLONG started = GetTickCount64();
  for (int redirect = 0; redirect <= request.maximum_redirects; ++redirect) {
    const bool resuming = existing > 0;
    std::vector<std::string> extra;
    if (resuming) {
      extra.push_back("Range: bytes=" + std::to_string(existing) + "-");
      extra.push_back("If-Range: " + etag);
    }
    Attempt attempt;
    if (!Send(session, request, current, extra, false, &attempt)) {
      result.error = attempt.error;
      *retryable = attempt.transport_error != 0 && RetryableTransport(attempt.transport_error);
      return result;
    }
    const Response& response = attempt.response;
    if (RedirectStatus(response.status)) {
      const std::string next = ResolveLocation(current, response.location);
      if (next.empty() || redirect == request.maximum_redirects ||
          !IsTrustedHttpsUrl(next, request.allowed_hosts, &result.error)) {
        RemoveFile(temporary);
        RemoveFile(state_path);
        if (result.error.empty()) result.error = "invalid HTTPS redirect";
        return result;
      }
      current = next;
      continue;
    }
    if (RetryableStatus(response.status)) {
      result.error = StatusFailure("download", current, response.status);
      *retryable = true;
      return result;
    }
    if (resuming && response.status == 416) {
      RemoveFile(temporary);
      RemoveFile(state_path);
      result.error = "HTTPS download must restart with a complete archive";
      result.resumed_bytes = 0;
      *retryable = true;
      return result;
    }
    if (response.status != 200 && response.status != 206) {
      result.error = StatusFailure("download", current, response.status);
      RemoveFile(temporary);
      RemoveFile(state_path);
      return result;
    }
    if (!response.encoding.empty() && response.encoding != "identity") {
      RemoveFile(temporary);
      RemoveFile(state_path);
      result.error = "HTTPS download arrived with a content encoding (" + response.encoding + ")";
      return result;
    }
    std::uint64_t offset = existing;
    std::uint64_t total = 0;
    if (response.status == 200) {
      offset = 0;
      result.resumed_bytes = 0;
      if (response.has_content_length) total = response.content_length;
    } else {
      if (!resuming || !ParseContentRange(response.range, existing, request.maximum_bytes, &total) ||
          (!etag.empty() && !response.etag.empty() && response.etag != etag)) {
        RemoveFile(temporary);
        RemoveFile(state_path);
        result.error = "HTTPS download must restart with a complete archive";
        result.resumed_bytes = 0;
        *retryable = true;
        return result;
      }
    }
    if (total > request.maximum_bytes) {
      RemoveFile(temporary);
      RemoveFile(state_path);
      result.error = "HTTPS download exceeds its size limit";
      return result;
    }
    if (offset == 0) {
      RemoveFile(state_path);
      if (StrongEtag(response.etag) && !WriteState(state_path, request.url, response.etag)) {
        result.error = "cannot persist HTTPS download state";
        return result;
      }
    }
    HANDLE file = CreateFileW(PathText(temporary).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              offset == 0 ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      result.error = "cannot create temporary download: " + ErrorText(GetLastError());
      return result;
    }
    LARGE_INTEGER end{};
    if (offset > 0 && !SetFilePointerEx(file, end, nullptr, FILE_END)) {
      CloseHandle(file);
      RemoveFile(temporary);
      RemoveFile(state_path);
      result.error = "cannot resume temporary download";
      return result;
    }
    FileDigest digest;
    if (offset == 0 && !digest.valid()) {
      CloseHandle(file);
      RemoveFile(temporary);
      result.error = "cannot initialize sha256 digest";
      return result;
    }
    std::uint64_t written = offset;
    bool exceeded = false;
    bool write_failed = false;
    bool hash_failed = false;
    DWORD transport_error = 0;
    progress.Report(written, total, true);
    const ReadOutcome outcome = ReadBody(
        attempt.request.value, request, started,
        [&](const char* data, std::size_t size) {
          if (size > request.maximum_bytes || written > request.maximum_bytes - size) {
            exceeded = true;
            return false;
          }
          DWORD chunk = 0;
          if (!WriteFile(file, data, static_cast<DWORD>(size), &chunk, nullptr) || chunk != size) {
            write_failed = true;
            return false;
          }
          if (offset == 0 && !digest.Update(data, size)) {
            hash_failed = true;
            return false;
          }
          written += size;
          progress.Report(written, total, false);
          return true;
        },
        &transport_error);
    const bool flushed = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    progress.Report(written, total, true);
    switch (outcome) {
      case ReadOutcome::kDone:
        break;
      case ReadOutcome::kCancelled:
        result.error = "cancelled";
        result.cancelled = true;
        return result;
      case ReadOutcome::kSinkFailed:
        result.error = hash_failed      ? "cannot compute SHA-256 digest during download"
                       : exceeded       ? "HTTPS download exceeds its size limit"
                       : write_failed   ? "cannot persist HTTPS download"
                                        : "cannot persist HTTPS download";
        RemoveFile(temporary);
        RemoveFile(state_path);
        return result;
      case ReadOutcome::kStalled:
        result.error = "HTTPS download stalled";
        *retryable = flushed;
        return result;
      case ReadOutcome::kTimedOut:
        result.error = "HTTPS download took too long";
        *retryable = flushed;
        return result;
      case ReadOutcome::kTransportError:
        result.error = ErrorText(transport_error);
        *retryable = flushed && RetryableTransport(transport_error);
        if (!*retryable) {
          RemoveFile(temporary);
          RemoveFile(state_path);
        }
        return result;
    }
    if (!flushed) {
      RemoveFile(temporary);
      RemoveFile(state_path);
      result.error = "cannot persist HTTPS download";
      return result;
    }
    if (written == 0) {
      RemoveFile(temporary);
      RemoveFile(state_path);
      result.error = StatusFailure("download", current, response.status);
      return result;
    }
    if (total > 0 && written != total) {
      result.error = "HTTPS download is incomplete (" + std::to_string(written) + " of " + std::to_string(total) +
                     " bytes)";
      *retryable = true;
      return result;
    }
    result.bytes_written = static_cast<std::size_t>(written);
    result.sha256 = offset == 0 ? digest.FinalHex() : HashRegularFile(temporary, &result.error);
    if (result.sha256.empty()) {
      RemoveFile(temporary);
      RemoveFile(state_path);
      if (result.error.empty()) result.error = "cannot finalize SHA-256 digest of downloaded file";
      return result;
    }
    result.error.clear();
    result.final_url = current;
    return result;
  }
  RemoveFile(temporary);
  RemoveFile(state_path);
  result.error = "too many HTTPS redirects";
  return result;
}

}

bool IsTrustedHttpsUrl(std::string_view url, const std::vector<std::string>& allowed_hosts, std::string* error) {
  for (const char character : url) {
    if (static_cast<unsigned char>(character) <= 0x20 || character == 0x7f) {
      if (error != nullptr) *error = "URL is malformed";
      return false;
    }
  }
  if (Lower(std::string(url.substr(0, 8))) != "https://") {
    if (error != nullptr) *error = "URL must be credential-free HTTPS on port 443";
    return false;
  }
  ParsedUrl parsed;
  if (!CrackUrl(url, &parsed, error)) return false;
  const std::string host = NormalizedHost(parsed.host);
  for (const std::string& allowed : allowed_hosts) {
    if (HostMatches(host, Lower(allowed))) return true;
  }
  if (error != nullptr) *error = "URL host is not trusted";
  return false;
}

HttpBytesResult DownloadBytes(const HttpTransferRequest& request) {
  HttpBytesResult result;
  if (request.maximum_bytes == 0 || !IsTrustedHttpsUrl(request.url, request.allowed_hosts, &result.error)) {
    if (result.error.empty()) result.error = "HTTP byte limit is zero";
    return result;
  }
  Handle session = OpenSession(request);
  if (session.value == nullptr) {
    result.error = "WinHttpOpen failed: " + ErrorText(GetLastError());
    return result;
  }
  const int attempts = request.maximum_attempts > 0 ? request.maximum_attempts : 1;
  for (int attempt = 1;; ++attempt) {
    bool retryable = false;
    result = DownloadBytesAttempt(session.value, request, &retryable);
    if (result || !retryable || attempt >= attempts || Cancelled(request)) return result;
    SleepUnlessCancelled(request, request.retry_delay_ms * attempt);
  }
}

HttpDownloadResult DownloadFile(const HttpTransferRequest& request, const std::filesystem::path& destination,
                                int progress_fd) {
  (void)progress_fd;
  HttpDownloadResult result;
  if (request.maximum_bytes == 0 || !IsTrustedHttpsUrl(request.url, request.allowed_hosts, &result.error)) {
    if (result.error.empty()) result.error = "HTTP byte limit is zero";
    return result;
  }
  std::error_code filesystem_error;
  std::filesystem::create_directories(destination.parent_path(), filesystem_error);
  if (filesystem_error) {
    result.error = "cannot create download directory";
    return result;
  }
  const std::filesystem::path temporary = destination.wstring() + L".part";
  const std::filesystem::path state_path = temporary.wstring() + L".json";
  const std::wstring lock_path = destination.wstring() + L".lock";
  HANDLE lock = CreateFileW(lock_path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
  if (lock == INVALID_HANDLE_VALUE) {
    result.error = "cannot lock HTTPS download (is it already being downloaded?)";
    return result;
  }
  struct LockCloser {
    HANDLE handle;
    ~LockCloser() { CloseHandle(handle); }
  } closer{lock};
  Handle session = OpenSession(request);
  if (session.value == nullptr) {
    result.error = "WinHttpOpen failed: " + ErrorText(GetLastError());
    return result;
  }
  const int attempts = request.maximum_attempts > 0 ? request.maximum_attempts : 1;
  for (int attempt = 1;; ++attempt) {
    bool retryable = false;
    result = DownloadFileAttempt(session.value, request, temporary, &retryable);
    if (result) break;
    if (result.cancelled) return result;
    if (!retryable || attempt >= attempts || Cancelled(request)) {
      if (!retryable && !Cancelled(request)) {
        RemoveFile(temporary);
        RemoveFile(state_path);
      }
      if (Cancelled(request)) result.cancelled = true;
      return result;
    }
    SleepUnlessCancelled(request, request.retry_delay_ms * attempt);
  }
  if (!MoveFileExW(temporary.wstring().c_str(), destination.wstring().c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    result.error = "cannot publish completed download: " + ErrorText(GetLastError());
  } else {
    RemoveFile(state_path);
  }
  return result;
}

}
