// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/winhttp_client.h"

#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

namespace elixir::runtime {
namespace {

using mocktail::services::HttpRequest;
using mocktail::services::HttpResponse;

class Handle {
 public:
  explicit Handle(HINTERNET handle = nullptr) : handle_(handle) {}
  ~Handle() {
    if (handle_ != nullptr) WinHttpCloseHandle(handle_);
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  HINTERNET get() const { return handle_; }
  explicit operator bool() const { return handle_ != nullptr; }

 private:
  HINTERNET handle_;
};

std::wstring Widen(const std::string& text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(length > 0 ? length : 0), L'\0');
  if (length > 0) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
  return wide;
}

std::string Narrow(const wchar_t* text, std::size_t size) {
  if (size == 0) return {};
  const int length = WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(size), nullptr, 0, nullptr, nullptr);
  std::string narrow(static_cast<std::size_t>(length > 0 ? length : 0), '\0');
  if (length > 0) WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(size), narrow.data(), length, nullptr, nullptr);
  return narrow;
}

std::string ErrorText(const char* what, DWORD code) {
  wchar_t* buffer = nullptr;
  const DWORD length =
      FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS,
                     GetModuleHandleW(L"winhttp.dll"), code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::string text = length > 0 ? Narrow(buffer, length) : std::string();
  if (buffer != nullptr) LocalFree(buffer);
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
  return std::string(what) + " failed (" + std::to_string(code) + (text.empty() ? ")" : ": " + text + ")");
}

HttpResponse Failure(std::string error) {
  HttpResponse response;
  response.error = std::move(error);
  return response;
}

}

HttpResponse WinHttpClient::Get(const HttpRequest& request) {
  const std::wstring url = Widen(request.url);
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwHostNameLength = static_cast<DWORD>(-1);
  parts.dwUrlPathLength = static_cast<DWORD>(-1);
  parts.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (url.empty() || !WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts)) {
    return Failure("invalid URL: " + request.url);
  }
  if (parts.nScheme != INTERNET_SCHEME_HTTPS) return Failure("only https URLs are allowed: " + request.url);
  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength + parts.dwExtraInfoLength);
  if (path.empty()) path = L"/";

  HINTERNET opened =
      WinHttpOpen(L"Elixir", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (opened == nullptr) {
    opened = WinHttpOpen(L"Elixir", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  }
  Handle session(opened);
  if (!session) return Failure(ErrorText("WinHttpOpen", GetLastError()));
  DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
  if (!WinHttpSetOption(session.get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
    protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(session.get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
  }
  const int timeout = request.timeout_ms > 0 ? static_cast<int>(request.timeout_ms) : 15000;
  WinHttpSetTimeouts(session.get(), timeout, 5000, timeout, timeout);

  Handle connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
  if (!connection) return Failure(ErrorText("WinHttpConnect", GetLastError()));
  Handle http(WinHttpOpenRequest(connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (!http) return Failure(ErrorText("WinHttpOpenRequest", GetLastError()));
  if (request.follow_redirects) {
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(http.get(), WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    DWORD maximum = 5;
    WinHttpSetOption(http.get(), WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS, &maximum, sizeof(maximum));
  } else {
    DWORD feature = WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(http.get(), WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof(feature));
  }
  DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
  WinHttpSetOption(http.get(), WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
  for (const std::string& header : request.headers) {
    std::wstring line = Widen(header);
    if (line.empty()) continue;
    WinHttpAddRequestHeaders(http.get(), line.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    SecureZeroMemory(line.data(), line.size() * sizeof(wchar_t));
  }
  if (!WinHttpSendRequest(http.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    return Failure(ErrorText("WinHttpSendRequest", GetLastError()));
  }
  if (!WinHttpReceiveResponse(http.get(), nullptr)) return Failure(ErrorText("WinHttpReceiveResponse", GetLastError()));

  HttpResponse response;
  DWORD status = 0;
  DWORD size = sizeof(status);
  if (!WinHttpQueryHeaders(http.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
    return Failure(ErrorText("WinHttpQueryHeaders", GetLastError()));
  }
  response.status_code = static_cast<long>(status);
  DWORD headers_size = 0;
  WinHttpQueryHeaders(http.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
                      &headers_size, WINHTTP_NO_HEADER_INDEX);
  if (headers_size > 0) {
    std::wstring raw(headers_size / sizeof(wchar_t), L'\0');
    if (WinHttpQueryHeaders(http.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(),
                            &headers_size, WINHTTP_NO_HEADER_INDEX)) {
      raw.resize(headers_size / sizeof(wchar_t));
      for (std::size_t begin = 0; begin < raw.size();) {
        std::size_t end = raw.find(L"\r\n", begin);
        if (end == std::wstring::npos) end = raw.size();
        if (end > begin) response.headers.push_back(Narrow(raw.data() + begin, end - begin));
        begin = end + 2;
      }
    }
    SecureZeroMemory(raw.data(), raw.size() * sizeof(wchar_t));
  }
  for (;;) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(http.get(), &available)) {
      return Failure(ErrorText("WinHttpQueryDataAvailable", GetLastError()));
    }
    if (available == 0) break;
    if (response.body.size() + available > request.maximum_body_bytes) {
      return Failure("the response from " + request.url + " is larger than " +
                     std::to_string(request.maximum_body_bytes) + " bytes");
    }
    const std::size_t offset = response.body.size();
    response.body.resize(offset + available);
    DWORD read = 0;
    if (!WinHttpReadData(http.get(), response.body.data() + offset, available, &read)) {
      return Failure(ErrorText("WinHttpReadData", GetLastError()));
    }
    response.body.resize(offset + read);
  }
  response.transport_ok = true;
  return response;
}

}
