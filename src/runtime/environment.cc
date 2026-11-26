// Modified by Elixir.
#include "runtime/environment.h"

#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>

#include <vector>
#endif

namespace mocktail {
namespace runtime {

bool Environment::HasNonEmpty(std::string_view name) const {
  const std::optional<std::string> value = Get(name);
  return value.has_value() && !value->empty();
}

std::string Environment::GetOr(std::string_view name,
                               std::string_view default_value) const {
  const std::optional<std::string> value = Get(name);
  if (!value.has_value() || value->empty()) {
    return std::string(default_value);
  }
  return *value;
}

#if defined(_WIN32)

namespace {

std::wstring WideFromUtf8(std::string_view text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(),
                      length);
  return wide;
}

std::string Utf8FromWide(std::wstring_view text) {
  if (text.empty()) return {};
  const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                                         nullptr, nullptr);
  if (length <= 0) return {};
  std::string narrow(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), length, nullptr,
                      nullptr);
  return narrow;
}

}

std::optional<std::string> ProcessEnvironment::Get(
    std::string_view name) const {
  if (name.empty() || name.find('=') != std::string_view::npos ||
      name.find('\0') != std::string_view::npos) {
    return std::nullopt;
  }
  const std::wstring wide_name = WideFromUtf8(name);
  if (wide_name.empty()) return std::nullopt;
  std::vector<wchar_t> buffer(256);
  for (;;) {
    SetLastError(ERROR_SUCCESS);
    const DWORD length = GetEnvironmentVariableW(wide_name.c_str(), buffer.data(),
                                                 static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) return std::nullopt;
      return std::string();
    }
    if (length < buffer.size()) return Utf8FromWide(std::wstring_view(buffer.data(), length));
    buffer.resize(length);
  }
}

#else

std::optional<std::string> ProcessEnvironment::Get(
    std::string_view name) const {
  const std::string owned_name(name);
  const char* value = std::getenv(owned_name.c_str());
  if (value == nullptr) {
    return std::nullopt;
  }
  return std::string(value);
}

#endif

}
}
