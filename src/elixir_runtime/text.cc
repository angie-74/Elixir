// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/text.h"

#include <windows.h>

#include <cstdio>

namespace elixir::runtime {

std::wstring WideFromUtf8(std::string_view text) {
  if (text.empty()) return {};
  const int length =
      MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
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

std::string GenericUtf8(const std::filesystem::path& path) { return Utf8FromWide(path.generic_wstring()); }

std::string NativeUtf8(const std::filesystem::path& path) { return Utf8FromWide(path.native()); }

std::filesystem::path PathFromUtf8(std::string_view text) { return std::filesystem::path(WideFromUtf8(text)); }

std::string JsonQuote(std::string_view text) {
  std::string out = "\"";
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (byte < 0x20) {
          char escape[8];
          std::snprintf(escape, sizeof(escape), "\\u%04x", byte);
          out += escape;
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
  return out;
}

}
