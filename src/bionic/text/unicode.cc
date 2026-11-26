// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The selection logic is adapted from bionic's libc/bionic/wctype.cpp and
// wcwidth.cpp under their BSD license:
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
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
//
// wcswidth() is OpenBSD's (Copyright (c) 2010 Stefan Sperling, ISC).
#include <windows.h>

#include <cstring>

#include "bionic/guest_constants.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace elixir::bionic::text_internal {
namespace {

constexpr int kUcharEastAsianWidth = 0x1004;
constexpr int kUcharHangulSyllableType = 0x100B;
constexpr int kCategoryNonSpacingMark = 6;
constexpr int kCategoryEnclosingMark = 7;
constexpr int kCategoryDecimalNumber = 9;
constexpr int kCategoryControl = 15;
constexpr int kCategoryFormat = 16;
constexpr int kCategoryDashPunctuation = 19;
constexpr int kCategoryOtherPunctuation = 23;
constexpr int kEaNeutral = 0, kEaAmbiguous = 1, kEaHalfwidth = 2, kEaFullwidth = 3, kEaNarrow = 4, kEaWide = 5;
constexpr int kHstLeadingJamo = 1, kHstVowelJamo = 2, kHstTrailingJamo = 3, kHstLv = 4, kHstLvt = 5;

constexpr std::uint32_t kMaxCodePoint = 0x10ffff;

struct IcuApi {
  std::int8_t (*char_type)(std::int32_t) = nullptr;
  std::int8_t (*has_binary_property)(std::int32_t, int) = nullptr;
  std::int32_t (*get_int_property_value)(std::int32_t, int) = nullptr;
  std::int32_t (*to_lower)(std::int32_t) = nullptr;
  std::int32_t (*to_upper)(std::int32_t) = nullptr;
  bool available = false;
};

template <typename T>
bool Resolve(HMODULE module, const char* name, T* out) {
  FARPROC address = GetProcAddress(module, name);
  if (address == nullptr) return false;
  *out = reinterpret_cast<T>(reinterpret_cast<void*>(address));
  return true;
}

IcuApi LoadIcu() noexcept {
  IcuApi api;
  HMODULE module = LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (module == nullptr) module = LoadLibraryExW(L"icuuc.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (module == nullptr) return api;
  api.available = Resolve(module, "u_charType", &api.char_type) &&
                  Resolve(module, "u_hasBinaryProperty", &api.has_binary_property) &&
                  Resolve(module, "u_getIntPropertyValue", &api.get_int_property_value) &&
                  Resolve(module, "u_tolower", &api.to_lower) && Resolve(module, "u_toupper", &api.to_upper);
  return api;
}

const IcuApi& Icu() noexcept {
  static const IcuApi api = LoadIcu();
  return api;
}

WORD Ctype1(std::uint32_t cp) noexcept {
  if (cp > 0xffff) return 0;
  const wchar_t ch = static_cast<wchar_t>(cp);
  WORD type = 0;
  if (!GetStringTypeW(CT_CTYPE1, &ch, 1, &type)) return 0;
  return type;
}

WORD Ctype3(std::uint32_t cp) noexcept {
  if (cp > 0xffff) return 0;
  const wchar_t ch = static_cast<wchar_t>(cp);
  WORD type = 0;
  if (!GetStringTypeW(CT_CTYPE3, &ch, 1, &type)) return 0;
  return type;
}

std::uint32_t MapCaseFallback(std::uint32_t cp, DWORD flags) noexcept {
  if (cp > 0xffff || (cp >= 0xd800 && cp <= 0xdfff)) return cp;
  wchar_t in = static_cast<wchar_t>(cp);
  wchar_t out[4] = {};
  const int written = LCMapStringEx(LOCALE_NAME_INVARIANT, flags, &in, 1, out, 4, nullptr, nullptr, 0);
  return written == 1 ? static_cast<std::uint32_t>(out[0]) : cp;
}

bool FallbackHasProperty(std::uint32_t cp, UnicodeProperty property) noexcept {
  const WORD t = Ctype1(cp);
  if ((t & C1_DEFINED) == 0) return false;
  switch (property) {
    case UnicodeProperty::kAlphabetic: return (t & C1_ALPHA) != 0;
    case UnicodeProperty::kLowercase: return (t & C1_LOWER) != 0;
    case UnicodeProperty::kUppercase: return (t & C1_UPPER) != 0;
    case UnicodeProperty::kWhiteSpace: return (t & C1_SPACE) != 0;
    case UnicodeProperty::kPosixAlnum: return (t & (C1_ALPHA | C1_DIGIT)) != 0;
    case UnicodeProperty::kPosixBlank: return (t & C1_BLANK) != 0;
    case UnicodeProperty::kPosixGraph: return (t & (C1_SPACE | C1_CNTRL)) == 0;
    case UnicodeProperty::kPosixPrint: return (t & C1_CNTRL) == 0 && ((t & C1_SPACE) == 0 || (t & C1_BLANK) != 0);
    case UnicodeProperty::kPosixXdigit: return (t & C1_XDIGIT) != 0;
    case UnicodeProperty::kDefaultIgnorable: return cp == 0x200b || cp == 0x200c || cp == 0x200d ||
                                                     cp == 0xfeff || (cp >= 0xfe00 && cp <= 0xfe0f);
  }
  return false;
}

int FallbackGeneralCategory(std::uint32_t cp) noexcept {
  const WORD t = Ctype1(cp);
  if ((t & C1_DEFINED) == 0) return 0;
  if (t & C1_CNTRL) return kCategoryControl;
  if (t & C1_DIGIT) return kCategoryDecimalNumber;
  if (t & C1_PUNCT) return kCategoryOtherPunctuation;
  if (Ctype3(cp) & C3_NONSPACING) return kCategoryNonSpacingMark;
  return 0;
}

int EastAsianWidth(std::uint32_t cp) noexcept {
  const IcuApi& icu = Icu();
  if (cp > kMaxCodePoint) return kEaNeutral;
  if (icu.available) return icu.get_int_property_value(static_cast<std::int32_t>(cp), kUcharEastAsianWidth);
  const WORD t3 = Ctype3(cp);
  if (t3 & (C3_FULLWIDTH | C3_IDEOGRAPH)) return kEaWide;
  if ((cp >= 0x20000 && cp <= 0x3fffd) || (cp >= 0x1f300 && cp <= 0x1faff)) return kEaWide;
  return kEaNeutral;
}

int HangulSyllableType(std::uint32_t cp) noexcept {
  const IcuApi& icu = Icu();
  if (cp > kMaxCodePoint) return 0;
  if (icu.available) return icu.get_int_property_value(static_cast<std::int32_t>(cp), kUcharHangulSyllableType);
  if (cp >= 0x1100 && cp <= 0x115f) return kHstLeadingJamo;
  if (cp >= 0x1160 && cp <= 0x11a7) return kHstVowelJamo;
  if (cp >= 0x11a8 && cp <= 0x11ff) return kHstTrailingJamo;
  if (cp >= 0xac00 && cp <= 0xd7a3) return ((cp - 0xac00) % 28 == 0) ? kHstLv : kHstLvt;
  return 0;
}

}

bool UnicodeBackendIsIcu() noexcept { return Icu().available; }

bool UnicodeHasProperty(std::uint32_t cp, UnicodeProperty property) noexcept {
  if (cp > kMaxCodePoint) return false;
  const IcuApi& icu = Icu();
  if (icu.available) return icu.has_binary_property(static_cast<std::int32_t>(cp), static_cast<int>(property)) != 0;
  return FallbackHasProperty(cp, property);
}

int UnicodeGeneralCategory(std::uint32_t cp) noexcept {
  if (cp > kMaxCodePoint) return 0;
  const IcuApi& icu = Icu();
  if (icu.available) return icu.char_type(static_cast<std::int32_t>(cp));
  return FallbackGeneralCategory(cp);
}

std::uint32_t UnicodeToLower(std::uint32_t cp) noexcept {
  if (cp > kMaxCodePoint) return cp;
  const IcuApi& icu = Icu();
  if (icu.available) return static_cast<std::uint32_t>(icu.to_lower(static_cast<std::int32_t>(cp)));
  return MapCaseFallback(cp, LCMAP_LOWERCASE);
}

std::uint32_t UnicodeToUpper(std::uint32_t cp) noexcept {
  if (cp > kMaxCodePoint) return cp;
  const IcuApi& icu = Icu();
  if (icu.available) return static_cast<std::uint32_t>(icu.to_upper(static_cast<std::int32_t>(cp)));
  return MapCaseFallback(cp, LCMAP_UPPERCASE);
}

bool WideIsSpace(g_wint wc) noexcept {
  if (wc < 0x80) return IsSpace(static_cast<int>(wc));
  return UnicodeHasProperty(wc, UnicodeProperty::kWhiteSpace);
}

g_wint WideToLower(g_wint wc) noexcept {
  if (wc < 0x80) return static_cast<g_wint>(ToLower(static_cast<int>(wc)));
  return UnicodeToLower(wc);
}

}

namespace {

namespace ti = elixir::bionic::text_internal;
using ti::g_long;
using ti::g_wchar;
using ti::g_wint;
using ti::UnicodeProperty;

int WideHas(g_wint wc, int (*narrow)(int), UnicodeProperty property) noexcept {
  if (wc < 0x80) return narrow(static_cast<int>(wc)) ? 1 : 0;
  return ti::UnicodeHasProperty(wc, property) ? 1 : 0;
}

int NarrowAlnum(int c) noexcept { return ti::IsAlnum(c); }
int NarrowAlpha(int c) noexcept { return ti::IsAlpha(c); }
int NarrowBlank(int c) noexcept { return ti::IsBlank(c); }
int NarrowGraph(int c) noexcept { return ti::IsGraph(c); }
int NarrowLower(int c) noexcept { return ti::IsLower(c); }
int NarrowPrint(int c) noexcept { return ti::IsPrint(c); }
int NarrowSpace(int c) noexcept { return ti::IsSpace(c); }
int NarrowUpper(int c) noexcept { return ti::IsUpper(c); }
int NarrowXdigit(int c) noexcept { return ti::IsXdigit(c); }

enum : int {
  kWcTypeInvalid = 0,
  kWcTypeAlnum,
  kWcTypeAlpha,
  kWcTypeBlank,
  kWcTypeCntrl,
  kWcTypeDigit,
  kWcTypeGraph,
  kWcTypeLower,
  kWcTypePrint,
  kWcTypePunct,
  kWcTypeSpace,
  kWcTypeUpper,
  kWcTypeXdigit,
  kWcTypeMax
};

const void* const kWctransToLower = reinterpret_cast<const void*>(1);
const void* const kWctransToUpper = reinterpret_cast<const void*>(2);

}

ELIXIR_BIONIC_FN(int, iswalnum)(g_wint wc) noexcept { return WideHas(wc, NarrowAlnum, UnicodeProperty::kPosixAlnum); }
ELIXIR_BIONIC_FN(int, iswalpha)(g_wint wc) noexcept { return WideHas(wc, NarrowAlpha, UnicodeProperty::kAlphabetic); }
ELIXIR_BIONIC_FN(int, iswblank)(g_wint wc) noexcept { return WideHas(wc, NarrowBlank, UnicodeProperty::kPosixBlank); }
ELIXIR_BIONIC_FN(int, iswgraph)(g_wint wc) noexcept { return WideHas(wc, NarrowGraph, UnicodeProperty::kPosixGraph); }
ELIXIR_BIONIC_FN(int, iswlower)(g_wint wc) noexcept { return WideHas(wc, NarrowLower, UnicodeProperty::kLowercase); }
ELIXIR_BIONIC_FN(int, iswprint)(g_wint wc) noexcept { return WideHas(wc, NarrowPrint, UnicodeProperty::kPosixPrint); }
ELIXIR_BIONIC_FN(int, iswspace)(g_wint wc) noexcept { return WideHas(wc, NarrowSpace, UnicodeProperty::kWhiteSpace); }
ELIXIR_BIONIC_FN(int, iswupper)(g_wint wc) noexcept { return WideHas(wc, NarrowUpper, UnicodeProperty::kUppercase); }
ELIXIR_BIONIC_FN(int, iswxdigit)(g_wint wc) noexcept {
  return WideHas(wc, NarrowXdigit, UnicodeProperty::kPosixXdigit);
}

ELIXIR_BIONIC_FN(int, iswcntrl)(g_wint wc) noexcept {
  if (wc < 0x80) return ti::IsCntrl(static_cast<int>(wc));
  return ti::UnicodeGeneralCategory(wc) == ti::kCategoryControl;
}

ELIXIR_BIONIC_FN(int, iswdigit)(g_wint wc) noexcept {
  if (wc < 0x80) return ti::IsDigit(static_cast<int>(wc));
  return ti::UnicodeGeneralCategory(wc) == ti::kCategoryDecimalNumber;
}

ELIXIR_BIONIC_FN(int, iswpunct)(g_wint wc) noexcept {
  if (wc < 0x80) return ti::IsPunct(static_cast<int>(wc));
  const int category = ti::UnicodeGeneralCategory(wc);
  return category >= ti::kCategoryDashPunctuation && category <= ti::kCategoryOtherPunctuation;
}

ELIXIR_BIONIC_FN(int, iswctype)(g_wint wc, g_long char_class) noexcept {
  switch (char_class) {
    case kWcTypeAlnum: return elixir_bionic_iswalnum(wc);
    case kWcTypeAlpha: return elixir_bionic_iswalpha(wc);
    case kWcTypeBlank: return elixir_bionic_iswblank(wc);
    case kWcTypeCntrl: return elixir_bionic_iswcntrl(wc);
    case kWcTypeDigit: return elixir_bionic_iswdigit(wc);
    case kWcTypeGraph: return elixir_bionic_iswgraph(wc);
    case kWcTypeLower: return elixir_bionic_iswlower(wc);
    case kWcTypePrint: return elixir_bionic_iswprint(wc);
    case kWcTypePunct: return elixir_bionic_iswpunct(wc);
    case kWcTypeSpace: return elixir_bionic_iswspace(wc);
    case kWcTypeUpper: return elixir_bionic_iswupper(wc);
    case kWcTypeXdigit: return elixir_bionic_iswxdigit(wc);
    default: return 0;
  }
}

ELIXIR_BIONIC_FN(g_long, wctype)(const char* property) noexcept {
  static const char* const kProperties[kWcTypeMax - 1] = {"alnum", "alpha", "blank", "cntrl", "digit", "graph",
                                                          "lower", "print", "punct", "space", "upper", "xdigit"};
  for (int i = 0; i < kWcTypeMax - 1; ++i) {
    if (std::strcmp(kProperties[i], property) == 0) return kWcTypeAlnum + i;
  }
  return 0;
}

ELIXIR_BIONIC_FN(g_wint, towlower)(g_wint wc) noexcept { return ti::WideToLower(wc); }

ELIXIR_BIONIC_FN(g_wint, towupper)(g_wint wc) noexcept {
  if (wc < 0x80) return static_cast<g_wint>(ti::ToUpper(static_cast<int>(wc)));
  return ti::UnicodeToUpper(wc);
}

ELIXIR_BIONIC_FN(const void*, wctrans)(const char* name) noexcept {
  if (std::strcmp(name, "tolower") == 0) return kWctransToLower;
  if (std::strcmp(name, "toupper") == 0) return kWctransToUpper;
  elixir::bionic::SetGuestErrno(elixir::bionic::guest::kEINVAL);
  return nullptr;
}

ELIXIR_BIONIC_FN(g_wint, towctrans)(g_wint c, const void* transform) noexcept {
  if (transform == kWctransToLower) return elixir_bionic_towlower(c);
  if (transform == kWctransToUpper) return elixir_bionic_towupper(c);
  elixir::bionic::SetGuestErrno(elixir::bionic::guest::kEINVAL);
  return c;
}

ELIXIR_BIONIC_FN(int, wcwidth)(g_wchar raw) noexcept {
  using namespace elixir::bionic::text_internal;
  const auto wc = static_cast<std::int32_t>(raw);
  if (wc >= 0x20 && wc < 0x7f) return 1;
  if (wc == 0) return 0;
  if (wc < ' ' || (wc >= 0x7f && wc <= 0xa0)) return -1;
  const auto cp = static_cast<std::uint32_t>(wc);
  switch (UnicodeGeneralCategory(cp)) {
    case kCategoryControl: return -1;
    case kCategoryNonSpacingMark:
    case kCategoryEnclosingMark: return 0;
    case kCategoryFormat: return cp == 0x00ad ? 1 : 0;
    default: break;
  }
  switch (HangulSyllableType(cp)) {
    case kHstVowelJamo:
    case kHstTrailingJamo: return 0;
    case kHstLeadingJamo:
    case kHstLv:
    case kHstLvt: return 2;
    default: break;
  }
  if (UnicodeHasProperty(cp, UnicodeProperty::kDefaultIgnorable)) return 0;
  if (cp >= 0x3248 && cp <= 0x4dff) {
    if (cp <= 0x324f) return 2;
    if (cp >= 0x4dc0) return 2;
  }
  switch (EastAsianWidth(cp)) {
    case kEaAmbiguous:
    case kEaHalfwidth:
    case kEaNarrow:
    case kEaNeutral: return 1;
    case kEaFullwidth:
    case kEaWide: return 2;
    default: return 0;
  }
}

ELIXIR_BIONIC_FN(int, wcswidth)(const g_wchar* s, ti::g_size n) noexcept {
  int width = 0;
  while (n != 0 && *s != 0) {
    const int w = elixir_bionic_wcwidth(*s);
    if (w == -1) return -1;
    width += w;
    ++s;
    --n;
  }
  return width;
}

namespace elixir::bionic::text_internal {

void RegisterUnicodeExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, iswalnum);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswalpha);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswblank);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswcntrl);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswdigit);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswgraph);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswlower);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswprint);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswpunct);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswspace);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswupper);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswxdigit);
  ELIXIR_TEXT_EXPORT(table, kLibc, iswctype);
  ELIXIR_TEXT_EXPORT(table, kLibc, wctype);
  ELIXIR_TEXT_EXPORT(table, kLibc, towlower);
  ELIXIR_TEXT_EXPORT(table, kLibc, towupper);
  ELIXIR_TEXT_EXPORT(table, kLibc, wctrans);
  ELIXIR_TEXT_EXPORT(table, kLibc, towctrans);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcwidth);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcswidth);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswalnum_l, iswalnum);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswalpha_l, iswalpha);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswblank_l, iswblank);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswcntrl_l, iswcntrl);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswdigit_l, iswdigit);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswgraph_l, iswgraph);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswlower_l, iswlower);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswprint_l, iswprint);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswpunct_l, iswpunct);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswspace_l, iswspace);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswupper_l, iswupper);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswxdigit_l, iswxdigit);
  ELIXIR_TEXT_ALIAS(table, kLibc, iswctype_l, iswctype);
  ELIXIR_TEXT_ALIAS(table, kLibc, wctype_l, wctype);
  ELIXIR_TEXT_ALIAS(table, kLibc, towlower_l, towlower);
  ELIXIR_TEXT_ALIAS(table, kLibc, towupper_l, towupper);
  ELIXIR_TEXT_ALIAS(table, kLibc, wctrans_l, wctrans);
  ELIXIR_TEXT_ALIAS(table, kLibc, towctrans_l, towctrans);
}

}
