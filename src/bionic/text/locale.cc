// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/locale.cpp and langinfo.cpp
// (references/platform_bionic):
//
//   Copyright (C) 2008-2016 The Android Open Source Project
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
#include <atomic>
#include <climits>
#include <cstring>

#include "bionic/guest_constants.h"
#include "bionic/guest_types.h"
#include "bionic/memory.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace {

namespace guest = elixir::bionic::guest;
using elixir::bionic::SetGuestErrno;
using elixir::bionic::text_internal::g_size;

struct GuestLocale {
  g_size mb_cur_max;
};
GuestLocale* const kGlobalLocale = reinterpret_cast<GuestLocale*>(static_cast<std::intptr_t>(-1));

std::atomic<bool> g_global_locale_is_utf8{true};
thread_local GuestLocale* t_current_locale = nullptr;

g_size LocaleMbCurMax(const GuestLocale* locale) noexcept {
  if (locale == kGlobalLocale || locale == nullptr) {
    return g_global_locale_is_utf8.load(std::memory_order_relaxed) ? 4 : 1;
  }
  return locale->mb_cur_max;
}

GuestLocale* AllocateLocale(g_size mb_cur_max) noexcept {
  auto* locale = static_cast<GuestLocale*>(elixir::bionic::GuestMalloc(sizeof(GuestLocale)));
  if (locale == nullptr) {
    SetGuestErrno(guest::kENOMEM);
    return nullptr;
  }
  locale->mb_cur_max = mb_cur_max;
  return locale;
}

bool IsSupportedLocale(const char* name) noexcept {
  return std::strcmp(name, "") == 0 || std::strcmp(name, "C") == 0 || std::strcmp(name, "C.UTF-8") == 0 ||
         std::strcmp(name, "en_US.UTF-8") == 0 || std::strcmp(name, "POSIX") == 0;
}

bool IsUtf8Locale(const char* name) noexcept { return *name == '\0' || std::strstr(name, "UTF-8") != nullptr; }

char kDecimalPoint[] = ".";
char kNotAvailable[] = "";

guest::Lconv BuildLconv() noexcept {
  guest::Lconv lconv;
  lconv.decimal_point = kDecimalPoint;
  lconv.thousands_sep = kNotAvailable;
  lconv.grouping = kNotAvailable;
  lconv.int_curr_symbol = kNotAvailable;
  lconv.currency_symbol = kNotAvailable;
  lconv.mon_decimal_point = kNotAvailable;
  lconv.mon_thousands_sep = kNotAvailable;
  lconv.mon_grouping = kNotAvailable;
  lconv.positive_sign = kNotAvailable;
  lconv.negative_sign = kNotAvailable;
  lconv.int_frac_digits = lconv.frac_digits = lconv.p_cs_precedes = lconv.p_sep_by_space = CHAR_MAX;
  lconv.n_cs_precedes = lconv.n_sep_by_space = lconv.p_sign_posn = lconv.n_sign_posn = CHAR_MAX;
  lconv.int_p_cs_precedes = lconv.int_p_sep_by_space = lconv.int_n_cs_precedes = CHAR_MAX;
  lconv.int_n_sep_by_space = lconv.int_p_sign_posn = lconv.int_n_sign_posn = CHAR_MAX;
  return lconv;
}

guest::Lconv g_lconv = BuildLconv();

enum : int {
  kCodeset = 1, kDTFmt, kDFmt, kTFmt, kTFmtAmpm, kAmStr, kPmStr,
  kDay1, kDay2, kDay3, kDay4, kDay5, kDay6, kDay7,
  kAbday1, kAbday2, kAbday3, kAbday4, kAbday5, kAbday6, kAbday7,
  kMon1, kMon2, kMon3, kMon4, kMon5, kMon6, kMon7, kMon8, kMon9, kMon10, kMon11, kMon12,
  kAbmon1, kAbmon2, kAbmon3, kAbmon4, kAbmon5, kAbmon6, kAbmon7, kAbmon8, kAbmon9, kAbmon10, kAbmon11, kAbmon12,
  kEra, kEraDFmt, kEraDTFmt, kEraTFmt, kAltDigits, kRadixchar, kThousep, kYesexpr, kNoexpr, kCrncystr,
};

const char* const kDayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* const kDayAbbreviations[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* const kMonthNames[] = {"January", "February", "March",     "April",   "May",      "June",
                                   "July",    "August",   "September", "October", "November", "December"};
const char* const kMonthAbbreviations[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

}

namespace elixir::bionic::text_internal {

std::size_t CurrentMbCurMax() noexcept { return LocaleMbCurMax(t_current_locale); }

}

ELIXIR_BIONIC_FN(g_size, __ctype_get_mb_cur_max)() noexcept { return LocaleMbCurMax(t_current_locale); }

ELIXIR_BIONIC_FN(guest::Lconv*, localeconv)() noexcept { return &g_lconv; }

ELIXIR_BIONIC_FN(char*, setlocale)(int category, const char* locale_name) noexcept {
  if (category < guest::kLC_CTYPE || category > guest::kLC_IDENTIFICATION) {
    SetGuestErrno(guest::kEINVAL);
    return nullptr;
  }
  if (locale_name != nullptr) {
    if (!IsSupportedLocale(locale_name)) {
      SetGuestErrno(guest::kENOENT);
      return nullptr;
    }
    g_global_locale_is_utf8.store(IsUtf8Locale(locale_name), std::memory_order_relaxed);
  }
  static char kUtf8Name[] = "C.UTF-8";
  static char kCName[] = "C";
  return g_global_locale_is_utf8.load(std::memory_order_relaxed) ? kUtf8Name : kCName;
}

ELIXIR_BIONIC_FN(void*, newlocale)(int category_mask, const char* locale_name, void* base) noexcept {
  if ((category_mask & ~guest::kLC_ALL_MASK) != 0 || locale_name == nullptr) {
    SetGuestErrno(guest::kEINVAL);
    return nullptr;
  }
  if (!IsSupportedLocale(locale_name)) {
    SetGuestErrno(guest::kENOENT);
    return nullptr;
  }
  return AllocateLocale(IsUtf8Locale(locale_name) ? 4 : 1);
}

ELIXIR_BIONIC_FN(void*, duplocale)(void* locale) noexcept {
  return AllocateLocale(LocaleMbCurMax(static_cast<GuestLocale*>(locale)));
}

ELIXIR_BIONIC_FN(void, freelocale)(void* locale) noexcept {
  if (locale == nullptr || locale == kGlobalLocale) return;
  elixir::bionic::GuestFree(locale);
}

ELIXIR_BIONIC_FN(void*, uselocale)(void* new_locale) noexcept {
  GuestLocale* old_locale = t_current_locale;
  if (old_locale == nullptr) old_locale = kGlobalLocale;
  if (new_locale != nullptr) t_current_locale = static_cast<GuestLocale*>(new_locale);
  return old_locale;
}

ELIXIR_BIONIC_FN(char*, nl_langinfo)(int item) noexcept {
  const char* result = "";
  switch (item) {
    case kCodeset: result = LocaleMbCurMax(t_current_locale) == 1 ? "ASCII" : "UTF-8"; break;
    case kDTFmt: result = "%F %T %z"; break;
    case kDFmt: result = "%F"; break;
    case kTFmt: result = "%T"; break;
    case kTFmtAmpm: result = "%I:%M:%S %p"; break;
    case kAmStr: result = "AM"; break;
    case kPmStr: result = "PM"; break;
    case kRadixchar: result = "."; break;
    case kYesexpr: result = "^[yY]"; break;
    case kNoexpr: result = "^[nN]"; break;
    default:
      if (item >= kDay1 && item <= kDay7) {
        result = kDayNames[item - kDay1];
      } else if (item >= kAbday1 && item <= kAbday7) {
        result = kDayAbbreviations[item - kAbday1];
      } else if (item >= kMon1 && item <= kMon12) {
        result = kMonthNames[item - kMon1];
      } else if (item >= kAbmon1 && item <= kAbmon12) {
        result = kMonthAbbreviations[item - kAbmon1];
      }
      break;
  }
  return const_cast<char*>(result);
}

ELIXIR_BIONIC_FN(void*, catopen)(const char* name, int flags) noexcept {
  return reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
}

ELIXIR_BIONIC_FN(char*, catgets)(void* catalog, int set_id, int message_id, const char* message) noexcept {
  return const_cast<char*>(message);
}

ELIXIR_BIONIC_FN(int, catclose)(void* catalog) noexcept {
  SetGuestErrno(guest::kEBADF);
  return -1;
}

namespace elixir::bionic::text_internal {

void RegisterLocaleExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, catopen);
  ELIXIR_TEXT_EXPORT(table, kLibc, catgets);
  ELIXIR_TEXT_EXPORT(table, kLibc, catclose);
  ELIXIR_TEXT_EXPORT(table, kLibc, __ctype_get_mb_cur_max);
  ELIXIR_TEXT_EXPORT(table, kLibc, localeconv);
  ELIXIR_TEXT_EXPORT(table, kLibc, setlocale);
  ELIXIR_TEXT_EXPORT(table, kLibc, newlocale);
  ELIXIR_TEXT_EXPORT(table, kLibc, duplocale);
  ELIXIR_TEXT_EXPORT(table, kLibc, freelocale);
  ELIXIR_TEXT_EXPORT(table, kLibc, uselocale);
  ELIXIR_TEXT_EXPORT(table, kLibc, nl_langinfo);
  ELIXIR_TEXT_ALIAS(table, kLibc, nl_langinfo_l, nl_langinfo);
}

}
