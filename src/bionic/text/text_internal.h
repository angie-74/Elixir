// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The ctype predicates reproduce bionic's inline <ctype.h> definitions
// (libc/include/ctype.h, BSD-2-Clause, Copyright The Android Open Source
// Project).
#ifndef ELIXIR_BIONIC_TEXT_TEXT_INTERNAL_H_
#define ELIXIR_BIONIC_TEXT_TEXT_INTERNAL_H_

#include <cstddef>
#include <cstdint>

#include "abi/sysv_abi.h"
#include "bionic/runtime.h"

namespace elixir::bionic::text_internal {

using abi::g_long;
using abi::g_size;
using abi::g_ulong;
using abi::g_wchar;
using abi::g_wint;

inline constexpr g_wint kWeof = 0xffffffffu;
inline constexpr std::size_t kMultibyteIllegal = static_cast<std::size_t>(-1);
inline constexpr std::size_t kMultibyteIncomplete = static_cast<std::size_t>(-2);
inline constexpr std::size_t kGuestPathMax = 4096;
inline constexpr std::size_t kGuestPageSize = 4096;

constexpr bool InRange(unsigned lo, int c, unsigned hi) noexcept {
  return static_cast<unsigned>(c) - lo < hi - lo + 1;
}
constexpr bool IsAlpha(int c) noexcept { return InRange('a', c | 0x20, 'z'); }
constexpr bool IsDigit(int c) noexcept { return InRange('0', c, '9'); }
constexpr bool IsAlnum(int c) noexcept { return IsAlpha(c) || IsDigit(c); }
constexpr bool IsBlank(int c) noexcept { return c == ' ' || c == '\t'; }
constexpr bool IsCntrl(int c) noexcept { return static_cast<unsigned>(c) < ' ' || c == 0x7f; }
constexpr bool IsGraph(int c) noexcept { return InRange('!', c, '~'); }
constexpr bool IsLower(int c) noexcept { return InRange('a', c, 'z'); }
constexpr bool IsUpper(int c) noexcept { return InRange('A', c, 'Z'); }
constexpr bool IsPrint(int c) noexcept { return InRange(' ', c, '~'); }
constexpr bool IsPunct(int c) noexcept { return IsGraph(c) && !IsAlnum(c); }
constexpr bool IsSpace(int c) noexcept { return c == ' ' || InRange('\t', c, '\r'); }
constexpr bool IsXdigit(int c) noexcept { return IsDigit(c) || InRange('a', c | 0x20, 'f'); }
constexpr int ToLower(int c) noexcept { return IsUpper(c) ? (c | 0x20) : c; }
constexpr int ToUpper(int c) noexcept { return IsLower(c) ? (c ^ 0x20) : c; }

enum class UnicodeProperty : int {
  kAlphabetic = 0,
  kDefaultIgnorable = 5,
  kLowercase = 22,
  kUppercase = 30,
  kWhiteSpace = 31,
  kPosixAlnum = 44,
  kPosixBlank = 45,
  kPosixGraph = 46,
  kPosixPrint = 47,
  kPosixXdigit = 48,
};
bool UnicodeHasProperty(std::uint32_t code_point, UnicodeProperty property) noexcept;
int UnicodeGeneralCategory(std::uint32_t code_point) noexcept;
std::uint32_t UnicodeToLower(std::uint32_t code_point) noexcept;
std::uint32_t UnicodeToUpper(std::uint32_t code_point) noexcept;
bool UnicodeBackendIsIcu() noexcept;

bool WideIsSpace(g_wint wc) noexcept;
g_wint WideToLower(g_wint wc) noexcept;

std::size_t Mbrtoc32WithState(std::uint32_t* pc32, const char* s, std::size_t n, void* state) noexcept;
std::size_t C32rtombWithState(char* s, std::uint32_t c32, void* state) noexcept;

std::size_t CurrentMbCurMax() noexcept;

std::size_t StrlcpyImpl(char* dst, const char* src, std::size_t size) noexcept;
std::size_t StrlcatImpl(char* dst, const char* src, std::size_t size) noexcept;

void RegisterMessageExports(ExportTable& table);
void RegisterStringFunctionExports(ExportTable& table);
void RegisterFortifyExports(ExportTable& table);
void RegisterCtypeExports(ExportTable& table);
void RegisterUnicodeExports(ExportTable& table);
void RegisterWcharExports(ExportTable& table);
void RegisterLocaleExports(ExportTable& table);
void RegisterIntegerParseExports(ExportTable& table);
void RegisterFloatParseExports(ExportTable& table);
void RegisterStdlibMiscExports(ExportTable& table);
void RegisterEnvironmentExports(ExportTable& table);
void RegisterMallocExports(ExportTable& table);
void RegisterIconvExports(ExportTable& table);

}

#define ELIXIR_TEXT_EXPORT(table, soname, name) \
  (table).Add((soname), #name, reinterpret_cast<void*>(&elixir_bionic_##name))
#define ELIXIR_TEXT_ALIAS(table, soname, alias, name) \
  (table).Add((soname), #alias, reinterpret_cast<void*>(&elixir_bionic_##name))

#endif
