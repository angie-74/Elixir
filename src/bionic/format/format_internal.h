// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_FORMAT_FORMAT_INTERNAL_H_
#define ELIXIR_BIONIC_FORMAT_FORMAT_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "abi/sysv_abi.h"

namespace elixir::bionic::format::internal {

inline constexpr std::size_t kMbIllegal = static_cast<std::size_t>(-1);
inline constexpr std::size_t kMbIncomplete = static_cast<std::size_t>(-2);
inline constexpr std::size_t kMbCurMax = 4;

inline std::size_t EncodeUtf8(std::uint32_t c, char* out) noexcept {
  if (c < 0x80) {
    out[0] = static_cast<char>(c);
    return 1;
  }
  std::size_t length;
  std::uint8_t lead;
  if (c < 0x800) {
    length = 2;
    lead = 0xc0;
  } else if (c < 0x10000) {
    length = 3;
    lead = 0xe0;
  } else if (c < 0x200000) {
    length = 4;
    lead = 0xf0;
  } else {
    return kMbIllegal;
  }
  for (std::size_t i = length - 1; i > 0; --i) {
    out[i] = static_cast<char>((c & 0x3f) | 0x80);
    c >>= 6;
  }
  out[0] = static_cast<char>(lead | c);
  return length;
}

inline std::size_t DecodeUtf8(const char* input, std::size_t length, std::uint32_t* out) noexcept {
  if (length == 0) return kMbIncomplete;
  const auto first = static_cast<std::uint8_t>(input[0]);
  if (first < 0x80) {
    *out = first;
    return first == 0 ? 0 : 1;
  }
  std::size_t needed;
  std::uint32_t value;
  std::uint32_t lower_bound;
  if ((first & 0xe0) == 0xc0) {
    needed = 2;
    value = first & 0x1f;
    lower_bound = 0x80;
  } else if ((first & 0xf0) == 0xe0) {
    needed = 3;
    value = first & 0x0f;
    lower_bound = 0x800;
  } else if ((first & 0xf8) == 0xf0) {
    needed = 4;
    value = first & 0x07;
    lower_bound = 0x10000;
  } else {
    return kMbIllegal;
  }
  for (std::size_t i = 1; i < needed; ++i) {
    if (i >= length) return kMbIncomplete;
    const auto byte = static_cast<std::uint8_t>(input[i]);
    if ((byte & 0xc0) != 0x80) return kMbIllegal;
    value = (value << 6) | (byte & 0x3f);
  }
  if (value < lower_bound) return kMbIllegal;
  if ((value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff) return kMbIllegal;
  *out = value;
  return value == 0 ? 0 : needed;
}

inline bool IsSpace(int c) noexcept { return c == ' ' || (c >= '\t' && c <= '\r'); }
inline bool IsDigit(std::int64_t c) noexcept { return c >= '0' && c <= '9'; }
inline bool IsXdigit(std::int64_t c) noexcept {
  return IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
inline bool IsAlnum(std::int64_t c) noexcept {
  return IsDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
inline bool IsUpper(std::int64_t c) noexcept { return c >= 'A' && c <= 'Z'; }
inline bool IsWideSpace(std::int64_t c) noexcept {
  if (c < 0x80) return c >= 0 && IsSpace(static_cast<int>(c));
  return c == 0x85 || c == 0xa0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200a) || c == 0x2028 ||
         c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000;
}

const char* ErrnoName(int guest_errno) noexcept;
const char* ErrnoMessage(int guest_errno) noexcept;

[[noreturn]] void FatalError(const char* message) noexcept;
[[noreturn]] void AbortProcess() noexcept;

enum class FloatKind : std::uint8_t { kFloat, kDouble, kQuad };

struct DecodedFloat {
  enum Class : std::uint8_t { kZero, kFinite, kInfinite, kNan } cls = kZero;
  bool negative = false;
  FloatKind kind = FloatKind::kDouble;
  std::uint64_t sig_high = 0;
  std::uint64_t sig_low = 0;
  int exponent2 = 0;
  int biased_exponent = 0;
  std::uint64_t fraction_high = 0;
  std::uint64_t fraction_low = 0;
};

DecodedFloat DecodeDouble(double value) noexcept;
DecodedFloat DecodeQuad(const abi::GuestLongDouble& value) noexcept;

void DecimalDigits(const DecodedFloat& value, int mode, int ndigits, std::string* digits,
                   int* decimal_point);

void HexDigits(const DecodedFloat& value, int ndigits, std::string* digits, int* exponent);

std::size_t ParseFloat(const char* text, std::size_t length, FloatKind kind, std::uint64_t* bits_high,
                       std::uint64_t* bits_low, bool* range_error);

inline constexpr int kLogUnknown = 0;
inline constexpr int kLogDefault = 1;
inline constexpr int kLogVerbose = 2;
inline constexpr int kLogDebug = 3;
inline constexpr int kLogInfo = 4;
inline constexpr int kLogWarn = 5;
inline constexpr int kLogError = 6;
inline constexpr int kLogFatal = 7;
inline constexpr int kLogSilent = 8;

}

#endif
