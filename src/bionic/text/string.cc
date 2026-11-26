// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Functions the host UCRT lacks follow the OpenBSD/bionic sources
// (strlcpy/strlcat, strsep, strtok_r, strcasestr, memmem, memccpy, stpncpy:
// OpenBSD, BSD-3-Clause / ISC; basename/dirname/__gnu_basename:
// libc/bionic/libgen.cpp and __gnu_basename.cpp, BSD-2-Clause, Copyright (C)
// 2008 The Android Open Source Project). The original notices:
//
//   Copyright (c) 1990, 1993 The Regents of the University of California.
//   All rights reserved.
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are
//   met: 1. Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//   3. Neither the name of the University nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
//   ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
//
//   Copyright (c) 1998, 2015 Todd C. Miller <millert@openbsd.org>
//   Permission to use, copy, modify, and distribute this software for any
//   purpose with or without fee is hereby granted, provided that the above
//   copyright notice and this permission notice appear in all copies.
//   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES.
#include <cstring>
#include <memory>

#include "bionic/guest_constants.h"
#include "bionic/memory.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace elixir::bionic::text_internal {

std::size_t StrlcpyImpl(char* dst, const char* src, std::size_t size) noexcept {
  const std::size_t length = std::strlen(src);
  if (size != 0) {
    const std::size_t copy = length < size - 1 ? length : size - 1;
    std::memcpy(dst, src, copy);
    dst[copy] = '\0';
  }
  return length;
}

std::size_t StrlcatImpl(char* dst, const char* src, std::size_t size) noexcept {
  std::size_t dst_length = 0;
  while (dst_length < size && dst[dst_length] != '\0') ++dst_length;
  const std::size_t src_length = std::strlen(src);
  if (dst_length == size) return size + src_length;
  const std::size_t room = size - dst_length - 1;
  const std::size_t copy = src_length < room ? src_length : room;
  std::memcpy(dst + dst_length, src, copy);
  dst[dst_length + copy] = '\0';
  return dst_length + src_length;
}

}

namespace {

using elixir::bionic::text_internal::g_size;
using elixir::bionic::text_internal::kGuestPathMax;
using elixir::bionic::text_internal::ToLower;

int CaseCompare(const char* a, const char* b, std::size_t n) noexcept {
  const auto* s1 = reinterpret_cast<const unsigned char*>(a);
  const auto* s2 = reinterpret_cast<const unsigned char*>(b);
  for (; n != 0; --n, ++s1, ++s2) {
    const int c1 = ToLower(*s1);
    const int c2 = ToLower(*s2);
    if (c1 != c2) return c1 - c2;
    if (*s1 == '\0') break;
  }
  return 0;
}

char* g_strtok_last = nullptr;

char* StrtokR(char* s, const char* delim, char** last) noexcept {
  if (s == nullptr && (s = *last) == nullptr) return nullptr;
  s += std::strspn(s, delim);
  if (*s == '\0') {
    *last = nullptr;
    return nullptr;
  }
  char* token = s;
  s += std::strcspn(s, delim);
  if (*s == '\0') {
    *last = nullptr;
  } else {
    *s = '\0';
    *last = s + 1;
  }
  return token;
}

struct PathBuffers {
  char basename[kGuestPathMax];
  char dirname[kGuestPathMax];
};
thread_local std::unique_ptr<PathBuffers> t_path_buffers;

PathBuffers* ThreadPathBuffers() noexcept {
  if (!t_path_buffers) {
    t_path_buffers.reset(new (std::nothrow) PathBuffers);
  }
  return t_path_buffers.get();
}

int BasenameR(const char* path, char* buffer, std::size_t buffer_size) noexcept {
  const char* startp;
  int length;
  if (path == nullptr || *path == '\0') {
    startp = ".";
    length = 1;
  } else {
    const char* endp = path + std::strlen(path) - 1;
    while (endp > path && *endp == '/') --endp;
    if (endp == path && *endp == '/') {
      startp = "/";
      length = 1;
    } else {
      startp = endp;
      while (startp > path && *(startp - 1) != '/') --startp;
      length = static_cast<int>(endp - startp + 1);
    }
  }
  int result = length;
  if (buffer == nullptr) return result;
  if (length > static_cast<int>(buffer_size) - 1) {
    length = static_cast<int>(buffer_size) - 1;
    result = -1;
    elixir::bionic::SetGuestErrno(elixir::bionic::guest::kERANGE);
  }
  if (length >= 0) {
    std::memcpy(buffer, startp, static_cast<std::size_t>(length));
    buffer[length] = '\0';
  }
  return result;
}

int DirnameR(const char* path, char* buffer, std::size_t buffer_size) noexcept {
  int length;
  if (path == nullptr || *path == '\0') {
    path = ".";
    length = 1;
  } else {
    const char* endp = path + std::strlen(path) - 1;
    while (endp > path && *endp == '/') --endp;
    while (endp > path && *endp != '/') --endp;
    if (endp == path) {
      path = (*endp == '/') ? "/" : ".";
      length = 1;
    } else {
      do {
        --endp;
      } while (endp > path && *endp == '/');
      length = static_cast<int>(endp - path + 1);
    }
  }
  int result = length;
  if (static_cast<std::size_t>(length) + 1 > kGuestPathMax) {
    elixir::bionic::SetGuestErrno(elixir::bionic::guest::kENAMETOOLONG);
    return -1;
  }
  if (buffer == nullptr) return result;
  if (length > static_cast<int>(buffer_size) - 1) {
    length = static_cast<int>(buffer_size) - 1;
    result = -1;
    elixir::bionic::SetGuestErrno(elixir::bionic::guest::kERANGE);
  }
  if (length >= 0) {
    std::memcpy(buffer, path, static_cast<std::size_t>(length));
    buffer[length] = '\0';
  }
  return result;
}

}

ELIXIR_BIONIC_FN(void*, memchr)(const void* s, int c, g_size n) noexcept {
  return const_cast<void*>(std::memchr(s, c, n));
}

ELIXIR_BIONIC_FN(void*, memrchr)(const void* s, int c, g_size n) noexcept {
  const auto* p = static_cast<const unsigned char*>(s) + n;
  const auto target = static_cast<unsigned char>(c);
  while (n-- != 0) {
    if (*--p == target) return const_cast<unsigned char*>(p);
  }
  return nullptr;
}

ELIXIR_BIONIC_FN(int, memcmp)(const void* a, const void* b, g_size n) noexcept {
  return std::memcmp(a, b, n);
}

ELIXIR_BIONIC_FN(int, bcmp)(const void* a, const void* b, g_size n) noexcept {
  return std::memcmp(a, b, n);
}

ELIXIR_BIONIC_FN(void*, memcpy)(void* dst, const void* src, g_size n) noexcept {
  return std::memcpy(dst, src, n);
}

ELIXIR_BIONIC_FN(void*, memmove)(void* dst, const void* src, g_size n) noexcept {
  return std::memmove(dst, src, n);
}

ELIXIR_BIONIC_FN(void*, mempcpy)(void* dst, const void* src, g_size n) noexcept {
  return static_cast<char*>(std::memcpy(dst, src, n)) + n;
}

ELIXIR_BIONIC_FN(void*, memset)(void* dst, int c, g_size n) noexcept {
  return std::memset(dst, c, n);
}

ELIXIR_BIONIC_FN(void*, memset_explicit)(void* dst, int c, g_size n) noexcept {
  std::memset(dst, c, n);
  __asm__ __volatile__("" : : "r"(dst) : "memory");
  return dst;
}

ELIXIR_BIONIC_FN(void, explicit_bzero)(void* dst, g_size n) noexcept {
  std::memset(dst, 0, n);
  __asm__ __volatile__("" : : "r"(dst) : "memory");
}

ELIXIR_BIONIC_FN(void, bzero)(void* dst, g_size n) noexcept { std::memset(dst, 0, n); }

ELIXIR_BIONIC_FN(void, bcopy)(const void* src, void* dst, g_size n) noexcept { std::memmove(dst, src, n); }

ELIXIR_BIONIC_FN(void*, memmem)(const void* haystack, g_size haystack_length, const void* needle,
                                g_size needle_length) noexcept {
  if (needle_length == 0) return const_cast<void*>(haystack);
  if (haystack_length < needle_length) return nullptr;
  const auto* h = static_cast<const unsigned char*>(haystack);
  const auto* n = static_cast<const unsigned char*>(needle);
  const unsigned char first = n[0];
  const unsigned char* const last_start = h + (haystack_length - needle_length);
  for (const unsigned char* p = h; p <= last_start;) {
    p = static_cast<const unsigned char*>(std::memchr(p, first, static_cast<std::size_t>(last_start - p) + 1));
    if (p == nullptr) return nullptr;
    if (std::memcmp(p + 1, n + 1, needle_length - 1) == 0) return const_cast<unsigned char*>(p);
    ++p;
  }
  return nullptr;
}

ELIXIR_BIONIC_FN(void*, memccpy)(void* dst, const void* src, int c, g_size n) noexcept {
  auto* d = static_cast<unsigned char*>(dst);
  const auto* s = static_cast<const unsigned char*>(src);
  const auto target = static_cast<unsigned char>(c);
  for (; n != 0; --n) {
    if ((*d++ = *s++) == target) return d;
  }
  return nullptr;
}

ELIXIR_BIONIC_FN(g_size, strlen)(const char* s) noexcept { return std::strlen(s); }

ELIXIR_BIONIC_FN(g_size, strnlen)(const char* s, g_size max_length) noexcept {
  const void* end = std::memchr(s, 0, max_length);
  return end != nullptr ? static_cast<g_size>(static_cast<const char*>(end) - s) : max_length;
}

ELIXIR_BIONIC_FN(char*, strcpy)(char* dst, const char* src) noexcept {
  std::memcpy(dst, src, std::strlen(src) + 1);
  return dst;
}

ELIXIR_BIONIC_FN(char*, strncpy)(char* dst, const char* src, g_size n) noexcept {
  return std::strncpy(dst, src, n);
}

ELIXIR_BIONIC_FN(char*, stpcpy)(char* dst, const char* src) noexcept {
  const std::size_t length = std::strlen(src);
  std::memcpy(dst, src, length + 1);
  return dst + length;
}

ELIXIR_BIONIC_FN(char*, stpncpy)(char* dst, const char* src, g_size n) noexcept {
  std::size_t length = 0;
  while (length < n && src[length] != '\0') ++length;
  std::memcpy(dst, src, length);
  if (length < n) std::memset(dst + length, 0, n - length);
  return dst + length;
}

ELIXIR_BIONIC_FN(char*, strcat)(char* dst, const char* src) noexcept {
  char* end = dst + std::strlen(dst);
  std::memcpy(end, src, std::strlen(src) + 1);
  return dst;
}

ELIXIR_BIONIC_FN(char*, strncat)(char* dst, const char* src, g_size n) noexcept {
  char* end = dst + std::strlen(dst);
  std::size_t length = 0;
  while (length < n && src[length] != '\0') ++length;
  std::memcpy(end, src, length);
  end[length] = '\0';
  return dst;
}

ELIXIR_BIONIC_FN(g_size, strlcpy)(char* dst, const char* src, g_size size) noexcept {
  return elixir::bionic::text_internal::StrlcpyImpl(dst, src, size);
}

ELIXIR_BIONIC_FN(g_size, strlcat)(char* dst, const char* src, g_size size) noexcept {
  return elixir::bionic::text_internal::StrlcatImpl(dst, src, size);
}

ELIXIR_BIONIC_FN(char*, strdup)(const char* s) noexcept {
  char* copy = elixir::bionic::GuestStrdup(s);
  if (copy == nullptr) elixir::bionic::SetGuestErrno(elixir::bionic::guest::kENOMEM);
  return copy;
}

ELIXIR_BIONIC_FN(char*, strndup)(const char* s, g_size n) noexcept {
  char* copy = elixir::bionic::GuestStrndup(s, n);
  if (copy == nullptr) elixir::bionic::SetGuestErrno(elixir::bionic::guest::kENOMEM);
  return copy;
}

ELIXIR_BIONIC_FN(int, strcmp)(const char* a, const char* b) noexcept { return std::strcmp(a, b); }

ELIXIR_BIONIC_FN(int, strncmp)(const char* a, const char* b, g_size n) noexcept {
  return std::strncmp(a, b, n);
}

ELIXIR_BIONIC_FN(int, strcasecmp)(const char* a, const char* b) noexcept {
  return CaseCompare(a, b, static_cast<std::size_t>(-1));
}

ELIXIR_BIONIC_FN(int, strncasecmp)(const char* a, const char* b, g_size n) noexcept {
  return CaseCompare(a, b, n);
}

ELIXIR_BIONIC_FN(int, strcoll)(const char* a, const char* b) noexcept { return std::strcmp(a, b); }

ELIXIR_BIONIC_FN(g_size, strxfrm)(char* dst, const char* src, g_size n) noexcept {
  if (n == 0) return std::strlen(src);
  return elixir::bionic::text_internal::StrlcpyImpl(dst, src, n);
}

ELIXIR_BIONIC_FN(char*, strchr)(const char* s, int c) noexcept {
  return const_cast<char*>(std::strchr(s, c));
}

ELIXIR_BIONIC_FN(char*, strrchr)(const char* s, int c) noexcept {
  return const_cast<char*>(std::strrchr(s, c));
}

ELIXIR_BIONIC_FN(char*, strchrnul)(const char* s, int c) noexcept {
  const char target = static_cast<char>(c);
  while (*s != target && *s != '\0') ++s;
  return const_cast<char*>(s);
}

ELIXIR_BIONIC_FN(char*, strstr)(const char* haystack, const char* needle) noexcept {
  return const_cast<char*>(std::strstr(haystack, needle));
}

ELIXIR_BIONIC_FN(char*, strcasestr)(const char* haystack, const char* needle) noexcept {
  const std::size_t needle_length = std::strlen(needle);
  if (needle_length == 0) return const_cast<char*>(haystack);
  for (; *haystack != '\0'; ++haystack) {
    if (CaseCompare(haystack, needle, needle_length) == 0) return const_cast<char*>(haystack);
  }
  return nullptr;
}

ELIXIR_BIONIC_FN(char*, strpbrk)(const char* s, const char* accept) noexcept {
  return const_cast<char*>(std::strpbrk(s, accept));
}

ELIXIR_BIONIC_FN(g_size, strspn)(const char* s, const char* accept) noexcept {
  return std::strspn(s, accept);
}

ELIXIR_BIONIC_FN(g_size, strcspn)(const char* s, const char* reject) noexcept {
  return std::strcspn(s, reject);
}

ELIXIR_BIONIC_FN(char*, strsep)(char** string_ptr, const char* delim) noexcept {
  char* s = *string_ptr;
  if (s == nullptr) return nullptr;
  char* end = s + std::strcspn(s, delim);
  if (*end == '\0') {
    *string_ptr = nullptr;
  } else {
    *end = '\0';
    *string_ptr = end + 1;
  }
  return s;
}

ELIXIR_BIONIC_FN(char*, strtok_r)(char* s, const char* delim, char** last) noexcept {
  return StrtokR(s, delim, last);
}

ELIXIR_BIONIC_FN(char*, strtok)(char* s, const char* delim) noexcept {
  return StrtokR(s, delim, &g_strtok_last);
}

ELIXIR_BIONIC_FN(int, ffs)(int value) noexcept { return __builtin_ffs(value); }
ELIXIR_BIONIC_FN(int, ffsl)(elixir::abi::g_long value) noexcept { return __builtin_ffsll(value); }
ELIXIR_BIONIC_FN(int, ffsll)(long long value) noexcept { return __builtin_ffsll(value); }

ELIXIR_BIONIC_FN(char*, basename)(const char* path) noexcept {
  PathBuffers* buffers = ThreadPathBuffers();
  if (buffers == nullptr) {
    elixir::bionic::SetGuestErrno(elixir::bionic::guest::kENOMEM);
    return nullptr;
  }
  const int rc = BasenameR(path, buffers->basename, sizeof(buffers->basename));
  return rc < 0 ? nullptr : buffers->basename;
}

ELIXIR_BIONIC_FN(char*, dirname)(const char* path) noexcept {
  PathBuffers* buffers = ThreadPathBuffers();
  if (buffers == nullptr) {
    elixir::bionic::SetGuestErrno(elixir::bionic::guest::kENOMEM);
    return nullptr;
  }
  const int rc = DirnameR(path, buffers->dirname, sizeof(buffers->dirname));
  return rc < 0 ? nullptr : buffers->dirname;
}

ELIXIR_BIONIC_FN(int, basename_r)(const char* path, char* buffer, g_size buffer_size) noexcept {
  return BasenameR(path, buffer, buffer_size);
}

ELIXIR_BIONIC_FN(int, dirname_r)(const char* path, char* buffer, g_size buffer_size) noexcept {
  return DirnameR(path, buffer, buffer_size);
}

ELIXIR_BIONIC_FN(const char*, __gnu_basename)(const char* path) noexcept {
  const char* last_slash = std::strrchr(path, '/');
  return last_slash != nullptr ? last_slash + 1 : path;
}

namespace elixir::bionic::text_internal {

void RegisterStringFunctionExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, memchr);
  ELIXIR_TEXT_EXPORT(table, kLibc, memrchr);
  ELIXIR_TEXT_EXPORT(table, kLibc, memcmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, bcmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, memcpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, memmove);
  ELIXIR_TEXT_EXPORT(table, kLibc, mempcpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, memset);
  ELIXIR_TEXT_EXPORT(table, kLibc, memset_explicit);
  ELIXIR_TEXT_EXPORT(table, kLibc, explicit_bzero);
  ELIXIR_TEXT_EXPORT(table, kLibc, bzero);
  ELIXIR_TEXT_EXPORT(table, kLibc, bcopy);
  ELIXIR_TEXT_EXPORT(table, kLibc, memmem);
  ELIXIR_TEXT_EXPORT(table, kLibc, memccpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, strlen);
  ELIXIR_TEXT_EXPORT(table, kLibc, strnlen);
  ELIXIR_TEXT_EXPORT(table, kLibc, strcpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, strncpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, stpcpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, stpncpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, strcat);
  ELIXIR_TEXT_EXPORT(table, kLibc, strncat);
  ELIXIR_TEXT_EXPORT(table, kLibc, strlcpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, strlcat);
  ELIXIR_TEXT_EXPORT(table, kLibc, strdup);
  ELIXIR_TEXT_EXPORT(table, kLibc, strndup);
  ELIXIR_TEXT_EXPORT(table, kLibc, strcmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, strncmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, strcasecmp);
  ELIXIR_TEXT_ALIAS(table, kLibc, strcasecmp_l, strcasecmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, strncasecmp);
  ELIXIR_TEXT_ALIAS(table, kLibc, strncasecmp_l, strncasecmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, strcoll);
  ELIXIR_TEXT_ALIAS(table, kLibc, strcoll_l, strcoll);
  ELIXIR_TEXT_EXPORT(table, kLibc, strxfrm);
  ELIXIR_TEXT_ALIAS(table, kLibc, strxfrm_l, strxfrm);
  ELIXIR_TEXT_EXPORT(table, kLibc, strchr);
  ELIXIR_TEXT_ALIAS(table, kLibc, index, strchr);
  ELIXIR_TEXT_EXPORT(table, kLibc, strrchr);
  ELIXIR_TEXT_ALIAS(table, kLibc, rindex, strrchr);
  ELIXIR_TEXT_EXPORT(table, kLibc, strchrnul);
  ELIXIR_TEXT_EXPORT(table, kLibc, strstr);
  ELIXIR_TEXT_EXPORT(table, kLibc, strcasestr);
  ELIXIR_TEXT_EXPORT(table, kLibc, strpbrk);
  ELIXIR_TEXT_EXPORT(table, kLibc, strspn);
  ELIXIR_TEXT_EXPORT(table, kLibc, strcspn);
  ELIXIR_TEXT_EXPORT(table, kLibc, strsep);
  ELIXIR_TEXT_EXPORT(table, kLibc, strtok);
  ELIXIR_TEXT_EXPORT(table, kLibc, strtok_r);
  ELIXIR_TEXT_EXPORT(table, kLibc, ffs);
  ELIXIR_TEXT_EXPORT(table, kLibc, ffsl);
  ELIXIR_TEXT_EXPORT(table, kLibc, ffsll);
  ELIXIR_TEXT_EXPORT(table, kLibc, basename);
  ELIXIR_TEXT_EXPORT(table, kLibc, dirname);
  ELIXIR_TEXT_EXPORT(table, kLibc, basename_r);
  ELIXIR_TEXT_EXPORT(table, kLibc, dirname_r);
  ELIXIR_TEXT_EXPORT(table, kLibc, __gnu_basename);
}

}
