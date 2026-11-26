// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/iconv.cpp
// (references/platform_bionic):
//
//   Copyright (C) 2017 The Android Open Source Project
//   All rights reserved.
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in
//      the documentation and/or other materials provided with the
//      distribution.
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
#include <cstdint>
#include <cstring>
#include <new>

#include "bionic/guest_constants.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace {

namespace ti = elixir::bionic::text_internal;
namespace guest = elixir::bionic::guest;
using elixir::bionic::GuestErrno;
using elixir::bionic::SetGuestErrno;
using ti::g_size;

enum class Encoding { kUsAscii, kUtf8, kUtf16Le, kUtf16Be, kUtf32Le, kUtf32Be, kWcharT };
enum class Mode { kError, kIgnore, kTranslit };

void* const kInvalidIconv = reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));

bool MatchEncoding(const char* lhs, const char* rhs) noexcept {
  while (*lhs && *rhs) {
    for (; *lhs; ++lhs) {
      const int c = static_cast<unsigned char>(*lhs);
      if (ti::IsAlnum(c) && (c != '0' || !ti::IsDigit(static_cast<unsigned char>(*(lhs + 1))))) break;
    }
    if (ti::ToLower(static_cast<unsigned char>(*lhs)) != ti::ToLower(static_cast<unsigned char>(*rhs))) break;
    ++lhs;
    ++rhs;
  }
  return (*lhs == '\0' || std::strstr(lhs, "//") == lhs) && *rhs == '\0';
}

bool ParseEncoding(const char* s, Encoding* encoding, Mode* mode) noexcept {
  if (const char* suffix = std::strstr(s, "//")) {
    if (mode == nullptr) return false;
    if (std::strcmp(suffix, "//IGNORE") == 0) {
      *mode = Mode::kIgnore;
    } else if (std::strcmp(suffix, "//TRANSLIT") == 0) {
      *mode = Mode::kTranslit;
    } else {
      return false;
    }
  }
  if (MatchEncoding(s, "utf8")) {
    *encoding = Encoding::kUtf8;
  } else if (MatchEncoding(s, "ascii") || MatchEncoding(s, "usascii")) {
    *encoding = Encoding::kUsAscii;
  } else if (MatchEncoding(s, "utf16le")) {
    *encoding = Encoding::kUtf16Le;
  } else if (MatchEncoding(s, "utf16be")) {
    *encoding = Encoding::kUtf16Be;
  } else if (MatchEncoding(s, "utf32le")) {
    *encoding = Encoding::kUtf32Le;
  } else if (MatchEncoding(s, "utf32be")) {
    *encoding = Encoding::kUtf32Be;
  } else if (MatchEncoding(s, "wchart")) {
    *encoding = Encoding::kWcharT;
  } else {
    return false;
  }
  return true;
}

class Converter {
 public:
  Encoding src_encoding = Encoding::kUtf8;
  Encoding dst_encoding = Encoding::kUtf8;
  Mode mode = Mode::kError;

  g_size Convert(char** src_buf, g_size* src_bytes_left, char** dst_buf, g_size* dst_bytes_left) noexcept {
    wc_ = 0;
    std::memset(state_, 0, sizeof(state_));
    replacement_count_ = 0;
    ignored_ = false;
    src_buf_ = src_buf;
    src_bytes_left_ = src_bytes_left;
    dst_buf_ = dst_buf;
    dst_bytes_left_ = dst_bytes_left;
    while (*src_bytes_left_ > 0) {
      if (!GetNext() || !ConvertOne()) return static_cast<g_size>(-1);
    }
    return Done();
  }

 private:
  bool GetNext() noexcept {
    SetGuestErrno(0);
    switch (src_encoding) {
      case Encoding::kUsAscii:
        wc_ = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(**src_buf_)));
        src_bytes_used_ = 1;
        if (wc_ > 0x7f) SetGuestErrno(guest::kEILSEQ);
        break;
      case Encoding::kUtf8:
        src_bytes_used_ = ti::Mbrtoc32WithState(&wc_, *src_buf_, *src_bytes_left_, state_);
        if (src_bytes_used_ == ti::kMultibyteIllegal) break;
        if (src_bytes_used_ == ti::kMultibyteIncomplete) {
          SetGuestErrno(guest::kEINVAL);
          return false;
        }
        break;
      case Encoding::kUtf16Be:
      case Encoding::kUtf16Le: {
        if (*src_bytes_left_ < 2) {
          SetGuestErrno(guest::kEINVAL);
          return false;
        }
        const bool swap = src_encoding == Encoding::kUtf16Be;
        wc_ = In16(*src_buf_, swap);
        if (wc_ >= 0xd800 && wc_ <= 0xdfff) {
          if (wc_ >= 0xdc00) {
            SetGuestErrno(guest::kEILSEQ);
            return false;
          }
          if (*src_bytes_left_ < 4) {
            SetGuestErrno(guest::kEINVAL);
            return false;
          }
          const std::uint32_t hi = wc_;
          const std::uint32_t lo = In16(*src_buf_ + 2, swap);
          wc_ = 0x10000 + ((hi - 0xd800) << 10) + (lo - 0xdc00);
          src_bytes_used_ = 4;
        }
        break;
      }
      case Encoding::kUtf32Be:
      case Encoding::kUtf32Le:
      case Encoding::kWcharT:
        if (*src_bytes_left_ < 4) {
          SetGuestErrno(guest::kEINVAL);
          return false;
        }
        wc_ = In32(*src_buf_, src_encoding == Encoding::kUtf32Be);
        break;
    }
    if (GuestErrno() == guest::kEILSEQ) {
      switch (mode) {
        case Mode::kError:
          return false;
        case Mode::kIgnore:
          *src_buf_ += src_bytes_used_;
          *src_bytes_left_ -= src_bytes_used_;
          ignored_ = true;
          if (*src_bytes_left_ == 0) {
            SetGuestErrno(guest::kEINVAL);
            return false;
          }
          return GetNext();
        case Mode::kTranslit:
          wc_ = '?';
          ++replacement_count_;
          return true;
      }
    }
    return true;
  }

  bool ConvertOne() noexcept {
    SetGuestErrno(0);
    switch (dst_encoding) {
      case Encoding::kUsAscii:
        buf_[0] = static_cast<char>(wc_);
        dst_bytes_used_ = 1;
        if (wc_ > 0x7f) SetGuestErrno(guest::kEILSEQ);
        break;
      case Encoding::kUtf8:
        dst_bytes_used_ = ti::C32rtombWithState(buf_, wc_, state_);
        if (dst_bytes_used_ == ti::kMultibyteIllegal) break;
        if (dst_bytes_used_ == ti::kMultibyteIncomplete) {
          SetGuestErrno(guest::kEINVAL);
          return false;
        }
        break;
      case Encoding::kUtf16Be:
      case Encoding::kUtf16Le: {
        const bool swap = dst_encoding == Encoding::kUtf16Be;
        if (wc_ < 0x10000) {
          Out16(buf_, static_cast<std::uint16_t>(wc_), swap);
        } else {
          const std::uint32_t value = wc_ - 0x10000;
          Out16(buf_ + 0, static_cast<std::uint16_t>(0xd800 | (value >> 10)), swap);
          Out16(buf_ + 2, static_cast<std::uint16_t>(0xdc00 | (value & 0x3ff)), swap);
          dst_bytes_used_ = 4;
        }
        break;
      }
      case Encoding::kUtf32Be:
      case Encoding::kUtf32Le:
      case Encoding::kWcharT:
        Out32(wc_, dst_encoding == Encoding::kUtf32Be);
        break;
    }
    if (GuestErrno() == guest::kEILSEQ) {
      if (mode == Mode::kIgnore) {
        *src_buf_ += src_bytes_used_;
        *src_bytes_left_ -= src_bytes_used_;
        ignored_ = true;
        return true;
      }
      if (mode == Mode::kTranslit) {
        wc_ = '?';
        ++replacement_count_;
        return ConvertOne();
      }
      return false;
    }
    return Emit();
  }

  std::uint32_t In16(const char* buf, bool swap) noexcept {
    const auto* src = reinterpret_cast<const std::uint8_t*>(buf);
    std::uint16_t value = static_cast<std::uint16_t>(src[0] | (src[1] << 8));
    if (swap) value = static_cast<std::uint16_t>((value >> 8) | (value << 8));
    src_bytes_used_ = 2;
    return value;
  }

  std::uint32_t In32(const char* buf, bool swap) noexcept {
    const auto* src = reinterpret_cast<const std::uint8_t*>(buf);
    std::uint32_t value = static_cast<std::uint32_t>(src[0]) | (static_cast<std::uint32_t>(src[1]) << 8) |
                          (static_cast<std::uint32_t>(src[2]) << 16) | (static_cast<std::uint32_t>(src[3]) << 24);
    if (swap) value = __builtin_bswap32(value);
    src_bytes_used_ = 4;
    return value;
  }

  void Out16(char* dst, std::uint16_t ch, bool swap) noexcept {
    if (swap) ch = static_cast<std::uint16_t>((ch >> 8) | (ch << 8));
    dst[0] = static_cast<char>(ch);
    dst[1] = static_cast<char>(ch >> 8);
    dst_bytes_used_ = 2;
  }

  void Out32(std::uint32_t ch, bool swap) noexcept {
    if (swap) ch = __builtin_bswap32(ch);
    buf_[0] = static_cast<char>(ch);
    buf_[1] = static_cast<char>(ch >> 8);
    buf_[2] = static_cast<char>(ch >> 16);
    buf_[3] = static_cast<char>(ch >> 24);
    dst_bytes_used_ = 4;
  }

  bool Emit() noexcept {
    if (dst_bytes_used_ > *dst_bytes_left_) {
      SetGuestErrno(guest::kE2BIG);
      return false;
    }
    std::memcpy(*dst_buf_, buf_, dst_bytes_used_);
    *src_buf_ += src_bytes_used_;
    *src_bytes_left_ -= src_bytes_used_;
    *dst_buf_ += dst_bytes_used_;
    *dst_bytes_left_ -= dst_bytes_used_;
    return true;
  }

  g_size Done() noexcept {
    if (mode == Mode::kTranslit) return replacement_count_;
    if (ignored_) {
      SetGuestErrno(guest::kEILSEQ);
      return static_cast<g_size>(-1);
    }
    return 0;
  }

  std::uint32_t wc_ = 0;
  char buf_[16] = {};
  g_size src_bytes_used_ = 0;
  g_size dst_bytes_used_ = 0;
  std::uint8_t state_[8] = {};
  g_size replacement_count_ = 0;
  bool ignored_ = false;
  char** src_buf_ = nullptr;
  g_size* src_bytes_left_ = nullptr;
  char** dst_buf_ = nullptr;
  g_size* dst_bytes_left_ = nullptr;
};

}

ELIXIR_BIONIC_FN(void*, iconv_open)(const char* dst_encoding, const char* src_encoding) noexcept {
  auto* converter = new (std::nothrow) Converter;
  if (converter == nullptr) {
    SetGuestErrno(guest::kENOMEM);
    return kInvalidIconv;
  }
  if (!ParseEncoding(src_encoding, &converter->src_encoding, nullptr) ||
      !ParseEncoding(dst_encoding, &converter->dst_encoding, &converter->mode)) {
    delete converter;
    SetGuestErrno(guest::kEINVAL);
    return kInvalidIconv;
  }
  return converter;
}

ELIXIR_BIONIC_FN(g_size, iconv)(void* converter, char** src_buf, g_size* src_bytes_left, char** dst_buf,
                                g_size* dst_bytes_left) noexcept {
  if (converter == kInvalidIconv) {
    SetGuestErrno(guest::kEBADF);
    return static_cast<g_size>(-1);
  }
  if (src_buf == nullptr) return 0;
  return static_cast<Converter*>(converter)->Convert(src_buf, src_bytes_left, dst_buf, dst_bytes_left);
}

ELIXIR_BIONIC_FN(int, iconv_close)(void* converter) noexcept {
  if (converter == kInvalidIconv) {
    SetGuestErrno(guest::kEBADF);
    return -1;
  }
  delete static_cast<Converter*>(converter);
  return 0;
}

namespace elixir::bionic::text_internal {

void RegisterIconvExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, iconv_open);
  ELIXIR_TEXT_EXPORT(table, kLibc, iconv);
  ELIXIR_TEXT_EXPORT(table, kLibc, iconv_close);
}

}
