// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/fortify.cpp and
// libc/private/bionic_fortify.h (references/platform_bionic):
//
//   Copyright (C) 2008-2016 The Android Open Source Project
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in
//      the documentation and/or other materials provided with the
//      distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
//   OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
//   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
//   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
//   SUCH DAMAGE.
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace {

using elixir::bionic::text_internal::g_size;

constexpr int kAndroidLogFatal = 7;
constexpr g_size kSsizeMax = static_cast<g_size>(INT64_MAX);

[[noreturn]] void FortifyFatal(const char* format, ...) noexcept {
  char message[512];
  int prefix = std::snprintf(message, sizeof(message), "FORTIFY: ");
  va_list args;
  va_start(args, format);
  std::vsnprintf(message + prefix, sizeof(message) - static_cast<std::size_t>(prefix), format, args);
  va_end(args);
  elixir::bionic::HostLog(kAndroidLogFatal, "libc", message);
  RaiseFailFastException(nullptr, nullptr, 0);
  TerminateProcess(GetCurrentProcess(), 134);
  __builtin_unreachable();
}

void CheckCount(const char* fn, const char* identifier, g_size value) noexcept {
  if (value > kSsizeMax) {
    FortifyFatal("%s: %s %llu > SSIZE_MAX", fn, identifier, static_cast<unsigned long long>(value));
  }
}

void CheckBufferAccess(const char* fn, const char* action, g_size claim, g_size actual) noexcept {
  if (claim > actual) {
    FortifyFatal("%s: prevented %llu-byte %s %llu-byte buffer", fn, static_cast<unsigned long long>(claim),
                 action, static_cast<unsigned long long>(actual));
  }
}

g_size StrlenChk(const char* s, g_size s_len) noexcept {
  const g_size length = std::strlen(s);
  if (length >= s_len) FortifyFatal("strlen: detected read past end of buffer");
  return length;
}

}

ELIXIR_BIONIC_FN(void*, __memcpy_chk)(void* dst, const void* src, g_size count, g_size dst_len) noexcept {
  if (count > dst_len) {
    CheckCount("memcpy", "count", count);
    CheckBufferAccess("memcpy", "write into", count, dst_len);
  }
  return std::memcpy(dst, src, count);
}

ELIXIR_BIONIC_FN(void*, __memmove_chk)(void* dst, const void* src, g_size len, g_size dst_len) noexcept {
  CheckBufferAccess("memmove", "write into", len, dst_len);
  return std::memmove(dst, src, len);
}

ELIXIR_BIONIC_FN(void*, __memset_chk)(void* dst, int byte, g_size count, g_size dst_len) noexcept {
  if (count > dst_len) {
    CheckCount("memset", "count", count);
    CheckBufferAccess("memset", "write into", count, dst_len);
  }
  return std::memset(dst, byte, count);
}

ELIXIR_BIONIC_FN(void*, __memchr_chk)(const void* s, int c, g_size n, g_size actual_size) noexcept {
  CheckBufferAccess("memchr", "read from", n, actual_size);
  return const_cast<void*>(std::memchr(s, c, n));
}

ELIXIR_BIONIC_FN(void*, __memrchr_chk)(const void* s, int c, g_size n, g_size actual_size) noexcept {
  CheckBufferAccess("memrchr", "read from", n, actual_size);
  const auto* p = static_cast<const unsigned char*>(s) + n;
  const auto target = static_cast<unsigned char>(c);
  while (n-- != 0) {
    if (*--p == target) return const_cast<unsigned char*>(p);
  }
  return nullptr;
}

ELIXIR_BIONIC_FN(void*, __mempcpy_chk)(void* dst, const void* src, g_size count, g_size dst_len) noexcept {
  CheckCount("mempcpy", "count", count);
  CheckBufferAccess("mempcpy", "write into", count, dst_len);
  return static_cast<char*>(std::memcpy(dst, src, count)) + count;
}

ELIXIR_BIONIC_FN(char*, __strcpy_chk)(char* dst, const char* src, g_size dst_len) noexcept {
  const g_size src_len = std::strlen(src) + 1;
  CheckBufferAccess("strcpy", "write into", src_len, dst_len);
  std::memcpy(dst, src, src_len);
  return dst;
}

ELIXIR_BIONIC_FN(char*, __stpcpy_chk)(char* dst, const char* src, g_size dst_len) noexcept {
  const g_size src_len = std::strlen(src) + 1;
  CheckBufferAccess("stpcpy", "write into", src_len, dst_len);
  std::memcpy(dst, src, src_len);
  return dst + src_len - 1;
}

ELIXIR_BIONIC_FN(char*, __strncpy_chk)(char* dst, const char* src, g_size len, g_size dst_len) noexcept {
  CheckBufferAccess("strncpy", "write into", len, dst_len);
  return std::strncpy(dst, src, len);
}

ELIXIR_BIONIC_FN(char*, __strncpy_chk2)(char* dst, const char* src, g_size n, g_size dst_len,
                                        g_size src_len) noexcept {
  CheckBufferAccess("strncpy", "write into", n, dst_len);
  if (n != 0) {
    char* d = dst;
    const char* s = src;
    do {
      const g_size s_copy_len = static_cast<g_size>(s - src);
      if (s_copy_len >= src_len) {
        FortifyFatal("strncpy: detected read past end of %llu-byte buffer", static_cast<unsigned long long>(src_len));
      }
      if ((*d++ = *s++) == 0) {
        while (--n != 0) *d++ = 0;
        break;
      }
    } while (--n != 0);
  }
  return dst;
}

ELIXIR_BIONIC_FN(char*, __stpncpy_chk)(char* dst, const char* src, g_size len, g_size dst_len) noexcept {
  CheckBufferAccess("stpncpy", "write into", len, dst_len);
  g_size length = 0;
  while (length < len && src[length] != '\0') ++length;
  std::memcpy(dst, src, length);
  if (length < len) std::memset(dst + length, 0, len - length);
  return dst + length;
}

ELIXIR_BIONIC_FN(char*, __stpncpy_chk2)(char* dst, const char* src, g_size n, g_size dst_len,
                                        g_size src_len) noexcept {
  CheckBufferAccess("stpncpy", "write into", n, dst_len);
  if (n != 0) {
    char* d = dst;
    const char* s = src;
    do {
      const g_size s_copy_len = static_cast<g_size>(s - src);
      if (s_copy_len >= src_len) {
        FortifyFatal("stpncpy: detected read past end of %llu-byte buffer",
                     static_cast<unsigned long long>(src_len));
      }
      if ((*d++ = *s++) == 0) {
        while (--n != 0) *d++ = 0;
        break;
      }
    } while (--n != 0);
  }
  return dst;
}

ELIXIR_BIONIC_FN(char*, __strcat_chk)(char* dst, const char* src, g_size dst_buf_size) noexcept {
  char* save = dst;
  const g_size dst_len = StrlenChk(dst, dst_buf_size);
  dst += dst_len;
  dst_buf_size -= dst_len;
  while ((*dst++ = *src++) != '\0') {
    dst_buf_size--;
    if (dst_buf_size == 0) {
      FortifyFatal("strcat: prevented write past end of %llu-byte buffer",
                   static_cast<unsigned long long>(dst_buf_size));
    }
  }
  return save;
}

ELIXIR_BIONIC_FN(char*, __strncat_chk)(char* dst, const char* src, g_size len, g_size dst_buf_size) noexcept {
  if (len == 0) return dst;
  const g_size dst_len = StrlenChk(dst, dst_buf_size);
  char* d = dst + dst_len;
  dst_buf_size -= dst_len;
  while (*src != '\0') {
    *d++ = *src++;
    len--;
    dst_buf_size--;
    if (dst_buf_size == 0) FortifyFatal("strncat: prevented write past end of buffer");
    if (len == 0) break;
  }
  *d = '\0';
  return dst;
}

ELIXIR_BIONIC_FN(g_size, __strlcpy_chk)(char* dst, const char* src, g_size supplied_size,
                                        g_size dst_len_from_compiler) noexcept {
  CheckBufferAccess("strlcpy", "write into", supplied_size, dst_len_from_compiler);
  return elixir::bionic::text_internal::StrlcpyImpl(dst, src, supplied_size);
}

ELIXIR_BIONIC_FN(g_size, __strlcat_chk)(char* dst, const char* src, g_size supplied_size,
                                        g_size dst_len_from_compiler) noexcept {
  CheckBufferAccess("strlcat", "write into", supplied_size, dst_len_from_compiler);
  return elixir::bionic::text_internal::StrlcatImpl(dst, src, supplied_size);
}

ELIXIR_BIONIC_FN(g_size, __strlen_chk)(const char* s, g_size s_len) noexcept { return StrlenChk(s, s_len); }

ELIXIR_BIONIC_FN(char*, __strchr_chk)(const char* p, int ch, g_size s_len) noexcept {
  for (;; ++p, s_len--) {
    if (s_len == 0) FortifyFatal("strchr: prevented read past end of buffer");
    if (*p == static_cast<char>(ch)) return const_cast<char*>(p);
    if (*p == '\0') return nullptr;
  }
}

ELIXIR_BIONIC_FN(char*, __strrchr_chk)(const char* p, int ch, g_size s_len) noexcept {
  for (const char* save = nullptr;; ++p, s_len--) {
    if (s_len == 0) FortifyFatal("strrchr: prevented read past end of buffer");
    if (*p == static_cast<char>(ch)) save = p;
    if (*p == '\0') return const_cast<char*>(save);
  }
}

namespace elixir::bionic::text_internal {

void RegisterFortifyExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, __memcpy_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __memmove_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __memset_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __memchr_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __memrchr_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __mempcpy_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strcpy_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __stpcpy_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strncpy_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strncpy_chk2);
  ELIXIR_TEXT_EXPORT(table, kLibc, __stpncpy_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __stpncpy_chk2);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strcat_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strncat_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strlcpy_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strlcat_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strlen_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strchr_chk);
  ELIXIR_TEXT_EXPORT(table, kLibc, __strrchr_chk);
}

}
