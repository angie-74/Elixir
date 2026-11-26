// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/strtol.cpp
// (references/platform_bionic):
//
//   Copyright (c) 1992 The Regents of the University of California.
//   All rights reserved.
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//   3. Neither the name of the University nor the names of its contributors
//      may be used to endorse or promote products derived from this software
//      without specific prior written permission.
//   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
//   ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
#include <cstdint>
#include <limits>

#include "bionic/guest_constants.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace {

namespace ti = elixir::bionic::text_internal;
using ti::g_long;
using ti::g_ulong;
using ti::g_wchar;

template <typename CharT>
int CharValue(CharT c) noexcept {
  if constexpr (sizeof(CharT) == 1) {
    return static_cast<unsigned char>(c);
  } else {
    return static_cast<int>(static_cast<std::int32_t>(c));
  }
}

template <typename T, typename CharT>
T StrToI(const CharT* s, CharT** end_ptr, int base) noexcept {
  constexpr bool kSigned = std::numeric_limits<T>::is_signed;
  constexpr T kMin = std::numeric_limits<T>::min();
  constexpr T kMax = std::numeric_limits<T>::max();

  if (base < 0 || base == 1 || base > 36) {
    if (end_ptr != nullptr) *end_ptr = const_cast<CharT*>(s);
    elixir::bionic::SetGuestErrno(elixir::bionic::guest::kEINVAL);
    return 0;
  }

  const CharT* p = s;
  int c;
  while (ti::IsSpace(c = CharValue(*p++))) {
  }
  bool neg = false;
  if (c == '-') {
    neg = true;
    c = CharValue(*p++);
  } else if (c == '+') {
    c = CharValue(*p++);
  }

  if ((base == 0 || base == 16) && c == '0' && (CharValue(*p) == 'x' || CharValue(*p) == 'X') &&
      ti::IsXdigit(CharValue(p[1]))) {
    c = CharValue(p[1]);
    p += 2;
    base = 16;
  }
  if ((base == 0 || base == 2) && c == '0' && (CharValue(*p) == 'b' || CharValue(*p) == 'B') &&
      ti::IsDigit(CharValue(p[1]))) {
    c = CharValue(p[1]);
    p += 2;
    base = 2;
  }
  if (base == 0) base = (c == '0') ? 8 : 10;

  T acc = 0;
  int any = 0;
  for (;; c = CharValue(*p++)) {
    if (ti::IsDigit(c)) {
      c -= '0';
    } else if (ti::IsAlpha(c)) {
      c = 10 + ((c | 0x20) - 'a');
    } else {
      break;
    }
    if (c >= base) break;
    if (any < 0) continue;
    if constexpr (kSigned) {
      if (__builtin_mul_overflow(acc, static_cast<T>(base), &acc) ||
          __builtin_sub_overflow(acc, static_cast<T>(c), &acc)) {
        any = -1;
        continue;
      }
    } else {
      if (__builtin_mul_overflow(acc, static_cast<T>(base), &acc) ||
          __builtin_add_overflow(acc, static_cast<T>(c), &acc)) {
        any = -1;
        continue;
      }
    }
    any = 1;
  }

  if (end_ptr != nullptr) *end_ptr = const_cast<CharT*>(any ? p - 1 : s);

  if (any == -1) {
    elixir::bionic::SetGuestErrno(elixir::bionic::guest::kERANGE);
    return (kSigned && neg) ? kMin : kMax;
  }
  if constexpr (kSigned) {
    if (any > 0 && !neg && acc == kMin) {
      elixir::bionic::SetGuestErrno(elixir::bionic::guest::kERANGE);
      return kMax;
    }
    return neg ? acc : static_cast<T>(-acc);
  } else {
    return neg ? static_cast<T>(-acc) : acc;
  }
}

}

ELIXIR_BIONIC_FN(g_long, strtol)(const char* s, char** end, int base) noexcept {
  return StrToI<g_long>(s, end, base);
}
ELIXIR_BIONIC_FN(long long, strtoll)(const char* s, char** end, int base) noexcept {
  return StrToI<long long>(s, end, base);
}
ELIXIR_BIONIC_FN(g_ulong, strtoul)(const char* s, char** end, int base) noexcept {
  return StrToI<g_ulong>(s, end, base);
}
ELIXIR_BIONIC_FN(unsigned long long, strtoull)(const char* s, char** end, int base) noexcept {
  return StrToI<unsigned long long>(s, end, base);
}
ELIXIR_BIONIC_FN(std::int64_t, strtoimax)(const char* s, char** end, int base) noexcept {
  return StrToI<std::int64_t>(s, end, base);
}
ELIXIR_BIONIC_FN(std::uint64_t, strtoumax)(const char* s, char** end, int base) noexcept {
  return StrToI<std::uint64_t>(s, end, base);
}

ELIXIR_BIONIC_FN(g_long, wcstol)(const g_wchar* s, g_wchar** end, int base) noexcept {
  return StrToI<g_long>(s, end, base);
}
ELIXIR_BIONIC_FN(long long, wcstoll)(const g_wchar* s, g_wchar** end, int base) noexcept {
  return StrToI<long long>(s, end, base);
}
ELIXIR_BIONIC_FN(g_ulong, wcstoul)(const g_wchar* s, g_wchar** end, int base) noexcept {
  return StrToI<g_ulong>(s, end, base);
}
ELIXIR_BIONIC_FN(unsigned long long, wcstoull)(const g_wchar* s, g_wchar** end, int base) noexcept {
  return StrToI<unsigned long long>(s, end, base);
}
ELIXIR_BIONIC_FN(std::int64_t, wcstoimax)(const g_wchar* s, g_wchar** end, int base) noexcept {
  return StrToI<std::int64_t>(s, end, base);
}
ELIXIR_BIONIC_FN(std::uint64_t, wcstoumax)(const g_wchar* s, g_wchar** end, int base) noexcept {
  return StrToI<std::uint64_t>(s, end, base);
}

ELIXIR_BIONIC_FN(int, atoi)(const char* s) noexcept {
  return static_cast<int>(StrToI<g_long>(s, static_cast<char**>(nullptr), 10));
}
ELIXIR_BIONIC_FN(g_long, atol)(const char* s) noexcept {
  return StrToI<g_long>(s, static_cast<char**>(nullptr), 10);
}
ELIXIR_BIONIC_FN(long long, atoll)(const char* s) noexcept {
  return StrToI<long long>(s, static_cast<char**>(nullptr), 10);
}

namespace elixir::bionic::text_internal {

void RegisterIntegerParseExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, strtol);
  ELIXIR_TEXT_ALIAS(table, kLibc, strtol_l, strtol);
  ELIXIR_TEXT_EXPORT(table, kLibc, strtoll);
  ELIXIR_TEXT_ALIAS(table, kLibc, strtoll_l, strtoll);
  ELIXIR_TEXT_ALIAS(table, kLibc, strtoq, strtoll);
  ELIXIR_TEXT_EXPORT(table, kLibc, strtoul);
  ELIXIR_TEXT_ALIAS(table, kLibc, strtoul_l, strtoul);
  ELIXIR_TEXT_EXPORT(table, kLibc, strtoull);
  ELIXIR_TEXT_ALIAS(table, kLibc, strtoull_l, strtoull);
  ELIXIR_TEXT_ALIAS(table, kLibc, strtouq, strtoull);
  ELIXIR_TEXT_EXPORT(table, kLibc, strtoimax);
  ELIXIR_TEXT_EXPORT(table, kLibc, strtoumax);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstol);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcstol_l, wcstol);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstoll);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcstoll_l, wcstoll);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstoul);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcstoul_l, wcstoul);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstoull);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcstoull_l, wcstoull);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstoimax);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstoumax);
  ELIXIR_TEXT_EXPORT(table, kLibc, atoi);
  ELIXIR_TEXT_EXPORT(table, kLibc, atol);
  ELIXIR_TEXT_EXPORT(table, kLibc, atoll);
}

}
