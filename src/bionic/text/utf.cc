// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// DecodeUtf8/EncodeUtf8 follow bionic's mbrtoc32()/c32rtomb() rules
// (libc/bionic/mbrtoc32.cpp, c32rtomb.cpp, BSD-2-Clause, Copyright (C) 2014 The
// Android Open Source Project).
#include "bionic/text.h"

#include <windows.h>

namespace elixir::bionic::text {

std::size_t DecodeUtf8(const char* input, std::size_t length, abi::g_wchar* out) noexcept {
  if (length == 0) return static_cast<std::size_t>(-2);
  const auto* bytes = reinterpret_cast<const unsigned char*>(input);
  const unsigned char lead = bytes[0];
  if (lead < 0x80) {
    if (out != nullptr) *out = lead;
    return lead != 0 ? 1 : 0;
  }
  std::size_t needed;
  std::uint32_t value;
  std::uint32_t lower_bound;
  if ((lead & 0xe0) == 0xc0) {
    needed = 2;
    value = lead & 0x1f;
    lower_bound = 0x80;
  } else if ((lead & 0xf0) == 0xe0) {
    needed = 3;
    value = lead & 0x0f;
    lower_bound = 0x800;
  } else if ((lead & 0xf8) == 0xf0) {
    needed = 4;
    value = lead & 0x07;
    lower_bound = 0x10000;
  } else {
    return static_cast<std::size_t>(-1);
  }
  for (std::size_t i = 1; i < needed; ++i) {
    if (i >= length) return static_cast<std::size_t>(-2);
    if ((bytes[i] & 0xc0) != 0x80) return static_cast<std::size_t>(-1);
    value = (value << 6) | (bytes[i] & 0x3f);
  }
  if (value < lower_bound) return static_cast<std::size_t>(-1);
  if ((value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff) return static_cast<std::size_t>(-1);
  if (out != nullptr) *out = value;
  return needed;
}

std::size_t EncodeUtf8(abi::g_wchar code_point, char* out) noexcept {
  if (code_point < 0x80) {
    out[0] = static_cast<char>(code_point);
    return 1;
  }
  std::size_t length;
  unsigned char lead;
  if (code_point < 0x800) {
    lead = 0xc0;
    length = 2;
  } else if (code_point < 0x10000) {
    lead = 0xe0;
    length = 3;
  } else if (code_point < 0x200000) {
    lead = 0xf0;
    length = 4;
  } else {
    return static_cast<std::size_t>(-1);
  }
  for (std::size_t i = length - 1; i > 0; --i) {
    out[i] = static_cast<char>((code_point & 0x3f) | 0x80);
    code_point >>= 6;
  }
  out[0] = static_cast<char>((code_point & 0xff) | lead);
  return length;
}

std::wstring Utf8ToWide(const char* text) {
  if (text == nullptr || *text == '\0') return std::wstring();
  const int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (needed <= 1) return std::wstring();
  std::wstring result(static_cast<std::size_t>(needed - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), needed);
  return result;
}

std::string WideToUtf8(const wchar_t* text, int length) {
  if (text == nullptr || length == 0) return std::string();
  const int needed = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
  if (needed <= 0) return std::string();
  std::string result(static_cast<std::size_t>(needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, length, result.data(), needed, nullptr, nullptr);
  if (length < 0 && !result.empty() && result.back() == '\0') result.pop_back();
  return result;
}

}
