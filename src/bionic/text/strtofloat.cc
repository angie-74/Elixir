// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <emmintrin.h>

#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "abi/sysv_abi.h"
#include "bionic/guest_constants.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace elixir::bionic::text_internal {
namespace {

using u128 = unsigned __int128;

class BigInt {
 public:
  BigInt() = default;
  explicit BigInt(u128 value) {
    while (value != 0) {
      limbs_.push_back(static_cast<std::uint32_t>(value));
      value >>= 32;
    }
  }

  bool IsZero() const noexcept { return limbs_.empty(); }
  bool IsOne() const noexcept { return limbs_.size() == 1 && limbs_[0] == 1; }

  std::int64_t BitLength() const noexcept {
    if (limbs_.empty()) return 0;
    return static_cast<std::int64_t>(limbs_.size() - 1) * 32 + (32 - __builtin_clz(limbs_.back()));
  }

  void MulSmall(std::uint32_t factor) {
    std::uint64_t carry = 0;
    for (std::uint32_t& limb : limbs_) {
      const std::uint64_t product = static_cast<std::uint64_t>(limb) * factor + carry;
      limb = static_cast<std::uint32_t>(product);
      carry = product >> 32;
    }
    if (carry != 0) limbs_.push_back(static_cast<std::uint32_t>(carry));
  }

  void AddSmall(std::uint32_t addend) {
    std::uint64_t carry = addend;
    for (std::size_t i = 0; carry != 0 && i < limbs_.size(); ++i) {
      const std::uint64_t sum = static_cast<std::uint64_t>(limbs_[i]) + carry;
      limbs_[i] = static_cast<std::uint32_t>(sum);
      carry = sum >> 32;
    }
    if (carry != 0) limbs_.push_back(static_cast<std::uint32_t>(carry));
  }

  void MulPow5(std::int64_t exponent) {
    constexpr std::uint32_t kPow5[14] = {1,       5,        25,        125,        625,         3125,       15625,
                                         78125,   390625,   1953125,   9765625,    48828125,    244140625,  1220703125};
    while (exponent >= 13) {
      MulSmall(kPow5[13]);
      exponent -= 13;
    }
    if (exponent > 0) MulSmall(kPow5[exponent]);
  }

  void ShiftLeft(std::int64_t bits) {
    if (limbs_.empty() || bits <= 0) return;
    const std::size_t words = static_cast<std::size_t>(bits / 32);
    const unsigned shift = static_cast<unsigned>(bits % 32);
    if (shift != 0) {
      std::uint32_t carry = 0;
      for (std::uint32_t& limb : limbs_) {
        const std::uint32_t next = limb >> (32 - shift);
        limb = (limb << shift) | carry;
        carry = next;
      }
      if (carry != 0) limbs_.push_back(carry);
    }
    if (words != 0) limbs_.insert(limbs_.begin(), words, 0u);
  }

  bool ShiftRight(std::int64_t bits) {
    if (bits <= 0 || limbs_.empty()) return false;
    const std::size_t words = static_cast<std::size_t>(bits / 32);
    const unsigned shift = static_cast<unsigned>(bits % 32);
    bool lost = false;
    if (words >= limbs_.size()) {
      limbs_.clear();
      return true;
    }
    for (std::size_t i = 0; i < words; ++i) lost |= limbs_[i] != 0;
    limbs_.erase(limbs_.begin(), limbs_.begin() + static_cast<std::ptrdiff_t>(words));
    if (shift != 0) {
      lost |= (limbs_[0] & ((1u << shift) - 1)) != 0;
      for (std::size_t i = 0; i < limbs_.size(); ++i) {
        const std::uint32_t high = i + 1 < limbs_.size() ? limbs_[i + 1] << (32 - shift) : 0;
        limbs_[i] = (limbs_[i] >> shift) | high;
      }
    }
    Trim();
    return lost;
  }

  void ShiftRightOne() noexcept {
    for (std::size_t i = 0; i < limbs_.size(); ++i) {
      const std::uint32_t high = i + 1 < limbs_.size() ? limbs_[i + 1] << 31 : 0;
      limbs_[i] = (limbs_[i] >> 1) | high;
    }
    Trim();
  }

  int Compare(const BigInt& other) const noexcept {
    if (limbs_.size() != other.limbs_.size()) return limbs_.size() < other.limbs_.size() ? -1 : 1;
    for (std::size_t i = limbs_.size(); i-- > 0;) {
      if (limbs_[i] != other.limbs_[i]) return limbs_[i] < other.limbs_[i] ? -1 : 1;
    }
    return 0;
  }

  void Subtract(const BigInt& other) noexcept {
    std::int64_t borrow = 0;
    for (std::size_t i = 0; i < limbs_.size(); ++i) {
      std::int64_t difference = static_cast<std::int64_t>(limbs_[i]) - borrow -
                                (i < other.limbs_.size() ? static_cast<std::int64_t>(other.limbs_[i]) : 0);
      borrow = difference < 0 ? 1 : 0;
      if (difference < 0) difference += (static_cast<std::int64_t>(1) << 32);
      limbs_[i] = static_cast<std::uint32_t>(difference);
    }
    Trim();
  }

  u128 Low128() const noexcept {
    u128 value = 0;
    for (std::size_t i = limbs_.size() < 4 ? limbs_.size() : 4; i-- > 0;) value = (value << 32) | limbs_[i];
    return value;
  }

 private:
  void Trim() noexcept {
    while (!limbs_.empty() && limbs_.back() == 0) limbs_.pop_back();
  }

  std::vector<std::uint32_t> limbs_;
};

struct FloatFormat {
  int precision;
  int min_exponent;
  int max_exponent;
};
constexpr FloatFormat kBinary32{24, -126, 127};
constexpr FloatFormat kBinary64{53, -1022, 1023};
constexpr FloatFormat kBinary128{113, -16382, 16383};

struct Encoded {
  std::uint32_t biased_exponent = 0;
  u128 fraction = 0;
  bool overflow = false;
  bool underflow = false;
};

int BitLength128(u128 value) noexcept {
  const auto high = static_cast<std::uint64_t>(value >> 64);
  if (high != 0) return 128 - __builtin_clzll(high);
  const auto low = static_cast<std::uint64_t>(value);
  return low != 0 ? 64 - __builtin_clzll(low) : 0;
}

Encoded Infinity(const FloatFormat& format) noexcept {
  Encoded result;
  result.biased_exponent = static_cast<std::uint32_t>(2 * format.max_exponent + 1);
  result.overflow = true;
  return result;
}

Encoded Zero(bool underflow) noexcept {
  Encoded result;
  result.underflow = underflow;
  return result;
}

Encoded RoundToFormat(BigInt num, BigInt den, std::int64_t binary_exponent, bool sticky, const FloatFormat& format) {
  const int precision = format.precision;
  const std::int64_t shift = (precision + 2) - (num.BitLength() - den.BitLength());
  u128 quotient;
  if (den.IsOne()) {
    if (shift >= 0) {
      num.ShiftLeft(shift);
    } else {
      sticky |= num.ShiftRight(-shift);
    }
    quotient = num.Low128();
  } else {
    if (shift >= 0) {
      num.ShiftLeft(shift);
    } else {
      den.ShiftLeft(-shift);
    }
    const int quotient_bits = precision + 3;
    BigInt divisor = den;
    divisor.ShiftLeft(quotient_bits - 1);
    quotient = 0;
    for (int bit = quotient_bits - 1; bit >= 0; --bit) {
      if (num.Compare(divisor) >= 0) {
        num.Subtract(divisor);
        quotient |= static_cast<u128>(1) << bit;
      }
      divisor.ShiftRightOne();
    }
    sticky |= !num.IsZero();
  }

  const int length = BitLength128(quotient);
  const std::int64_t exponent = length - 1 + binary_exponent - shift;
  if (exponent > format.max_exponent + 1) return Infinity(format);
  const std::int64_t keep =
      exponent >= format.min_exponent ? precision : precision - (format.min_exponent - exponent);
  const std::int64_t drop = length - keep;
  const bool tiny = exponent < format.min_exponent;

  u128 mantissa;
  bool inexact;
  if (drop > length) {
    mantissa = 0;
    inexact = true;
  } else {
    mantissa = quotient >> drop;
    const u128 remainder = quotient & ((static_cast<u128>(1) << drop) - 1);
    const u128 half = static_cast<u128>(1) << (drop - 1);
    inexact = remainder != 0 || sticky;
    if (remainder > half || (remainder == half && (sticky || (mantissa & 1) != 0))) ++mantissa;
  }
  std::int64_t scale = exponent - (keep - 1);
  if (mantissa == (static_cast<u128>(1) << precision)) {
    mantissa >>= 1;
    ++scale;
  }
  if (mantissa == 0) return Zero(inexact);

  Encoded result;
  const u128 hidden = static_cast<u128>(1) << (precision - 1);
  if (mantissa >= hidden) {
    const std::int64_t final_exponent = scale + precision - 1;
    if (final_exponent > format.max_exponent) return Infinity(format);
    result.biased_exponent = static_cast<std::uint32_t>(final_exponent + format.max_exponent);
    result.fraction = mantissa - hidden;
  } else {
    result.biased_exponent = 0;
    result.fraction = mantissa;
  }
  result.underflow = tiny && inexact;
  return result;
}

enum class Special { kNone, kNan, kInfinity };

struct Parsed {
  bool negative = false;
  Special special = Special::kNone;
  Encoded value;
  bool converted = false;
  bool out_of_memory = false;
};

constexpr std::size_t kMaxSignificantDigits = 12000;
constexpr std::int64_t kExponentLimit = 100000000;

constexpr double kExactPow10[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                                  1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
constexpr float kExactPow10f[] = {1e0f, 1e1f, 1e2f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f, 1e10f};

Encoded EncodedFromDouble(double value) noexcept {
  std::uint64_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  Encoded result;
  result.biased_exponent = static_cast<std::uint32_t>((bits >> 52) & 0x7ff);
  result.fraction = bits & ((1ull << 52) - 1);
  return result;
}

Encoded EncodedFromFloat(float value) noexcept {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  Encoded result;
  result.biased_exponent = (bits >> 23) & 0xff;
  result.fraction = bits & ((1u << 23) - 1);
  return result;
}

template <typename CharT>
int At(const CharT* p) noexcept {
  if constexpr (sizeof(CharT) == 1) {
    return static_cast<unsigned char>(*p);
  } else {
    return *p < 0x80 ? static_cast<int>(*p) : 0x100;
  }
}

template <typename CharT>
bool MatchIgnoreCase(const CharT* p, const char* word) noexcept {
  for (; *word != '\0'; ++word, ++p) {
    if ((At(p) | 0x20) != *word) return false;
  }
  return true;
}

int HexValue(int c) noexcept {
  if (IsDigit(c)) return c - '0';
  return (c | 0x20) - 'a' + 10;
}

template <typename CharT>
Parsed ParseFloat(const CharT* const start, CharT** end, const FloatFormat& format) noexcept {
  Parsed parsed;
  const CharT* p = start;
  if constexpr (sizeof(CharT) == 1) {
    while (IsSpace(At(p))) ++p;
  } else {
    while (WideIsSpace(*p)) ++p;
  }
  if (At(p) == '-' || At(p) == '+') parsed.negative = At(p++) == '-';

  if (MatchIgnoreCase(p, "inf")) {
    p += 3;
    if (MatchIgnoreCase(p, "inity")) p += 5;
    parsed.special = Special::kInfinity;
    parsed.converted = true;
    if (end != nullptr) *end = const_cast<CharT*>(p);
    return parsed;
  }
  if (MatchIgnoreCase(p, "nan")) {
    p += 3;
    if (At(p) == '(') {
      const CharT* q = p + 1;
      while (IsAlnum(At(q)) || At(q) == '_') ++q;
      if (At(q) == ')') p = q + 1;
    }
    parsed.special = Special::kNan;
    parsed.converted = true;
    if (end != nullptr) *end = const_cast<CharT*>(p);
    return parsed;
  }

  try {
    if (At(p) == '0' && (At(p + 1) | 0x20) == 'x' &&
        (IsXdigit(At(p + 2)) || (At(p + 2) == '.' && IsXdigit(At(p + 3))))) {
      p += 2;
      u128 mantissa = 0;
      int significant_digits = 0;
      std::int64_t binary_exponent = 0;
      bool sticky = false;
      bool seen_point = false;
      for (;; ++p) {
        const int c = At(p);
        if (c == '.' && !seen_point) {
          seen_point = true;
          continue;
        }
        if (!IsXdigit(c)) break;
        const int digit = HexValue(c);
        if (significant_digits == 0 && digit == 0) {
          if (seen_point) binary_exponent -= 4;
          continue;
        }
        if (significant_digits < 30) {
          mantissa = (mantissa << 4) | static_cast<u128>(digit);
          ++significant_digits;
          if (seen_point) binary_exponent -= 4;
        } else {
          sticky |= digit != 0;
          if (!seen_point) binary_exponent += 4;
        }
      }
      if ((At(p) | 0x20) == 'p') {
        const CharT* q = p + 1;
        bool exponent_negative = false;
        if (At(q) == '-' || At(q) == '+') exponent_negative = At(q++) == '-';
        if (IsDigit(At(q))) {
          std::int64_t exponent = 0;
          while (IsDigit(At(q))) {
            if (exponent < kExponentLimit) exponent = exponent * 10 + (At(q) - '0');
            ++q;
          }
          binary_exponent += exponent_negative ? -exponent : exponent;
          p = q;
        }
      }
      parsed.converted = true;
      if (end != nullptr) *end = const_cast<CharT*>(p);
      if (mantissa == 0) return parsed;
      parsed.value = RoundToFormat(BigInt(mantissa), BigInt(1), binary_exponent, sticky, format);
      return parsed;
    }

    std::string digits;
    std::int64_t decimal_exponent = 0;
    bool any_digit = false;
    bool sticky = false;
    bool seen_point = false;
    for (;; ++p) {
      const int c = At(p);
      if (c == '.' && !seen_point) {
        seen_point = true;
        continue;
      }
      if (!IsDigit(c)) break;
      any_digit = true;
      if (digits.empty() && c == '0') {
        if (seen_point) --decimal_exponent;
        continue;
      }
      if (digits.size() < kMaxSignificantDigits) {
        digits.push_back(static_cast<char>(c));
        if (seen_point) --decimal_exponent;
      } else {
        sticky |= c != '0';
        if (!seen_point) ++decimal_exponent;
      }
    }
    if (!any_digit) {
      if (end != nullptr) *end = const_cast<CharT*>(start);
      return parsed;
    }
    if ((At(p) | 0x20) == 'e') {
      const CharT* q = p + 1;
      bool exponent_negative = false;
      if (At(q) == '-' || At(q) == '+') exponent_negative = At(q++) == '-';
      if (IsDigit(At(q))) {
        std::int64_t exponent = 0;
        while (IsDigit(At(q))) {
          if (exponent < kExponentLimit) exponent = exponent * 10 + (At(q) - '0');
          ++q;
        }
        decimal_exponent += exponent_negative ? -exponent : exponent;
        p = q;
      }
    }
    parsed.converted = true;
    if (end != nullptr) *end = const_cast<CharT*>(p);

    while (!digits.empty() && digits.back() == '0') {
      digits.pop_back();
      ++decimal_exponent;
    }
    if (digits.empty()) return parsed;
    if (sticky) {
      digits.push_back('1');
      --decimal_exponent;
    }
    const auto digit_count = static_cast<std::int64_t>(digits.size());
    if (digit_count + decimal_exponent > 5000) {
      parsed.value = Infinity(format);
      return parsed;
    }
    if (digit_count + decimal_exponent < -5000) {
      parsed.value = Zero(true);
      return parsed;
    }

    if (format.precision == kBinary64.precision && digit_count <= 15 && decimal_exponent >= -22 &&
        decimal_exponent <= 22 + (15 - digit_count)) {
      std::uint64_t integer = 0;
      for (char d : digits) integer = integer * 10 + static_cast<std::uint64_t>(d - '0');
      double value = static_cast<double>(integer);
      std::int64_t e = decimal_exponent;
      if (e > 22) {
        value *= kExactPow10[e - 22];
        e = 22;
      }
      value = e >= 0 ? value * kExactPow10[e] : value / kExactPow10[-e];
      parsed.value = EncodedFromDouble(value);
      return parsed;
    }
    if (format.precision == kBinary32.precision && digit_count <= 7 && decimal_exponent >= -10 &&
        decimal_exponent <= 10) {
      std::uint32_t integer = 0;
      for (char d : digits) integer = integer * 10 + static_cast<std::uint32_t>(d - '0');
      const float value = decimal_exponent >= 0 ? static_cast<float>(integer) * kExactPow10f[decimal_exponent]
                                                : static_cast<float>(integer) / kExactPow10f[-decimal_exponent];
      parsed.value = EncodedFromFloat(value);
      return parsed;
    }

    BigInt num;
    for (std::size_t i = 0; i < digits.size();) {
      std::uint32_t chunk = 0;
      std::uint32_t scale = 1;
      for (int k = 0; k < 9 && i < digits.size(); ++k, ++i) {
        chunk = chunk * 10 + static_cast<std::uint32_t>(digits[i] - '0');
        scale *= 10;
      }
      num.MulSmall(scale);
      if (num.IsZero()) {
        num = BigInt(chunk);
      } else {
        num.AddSmall(chunk);
      }
    }
    BigInt den(1);
    if (decimal_exponent >= 0) {
      num.MulPow5(decimal_exponent);
    } else {
      den.MulPow5(-decimal_exponent);
    }
    parsed.value = RoundToFormat(std::move(num), std::move(den), decimal_exponent, false, format);
    return parsed;
  } catch (...) {
    parsed.out_of_memory = true;
    parsed.converted = true;
    return parsed;
  }
}

void ReportErrors(const Parsed& parsed) noexcept {
  if (parsed.out_of_memory) {
    SetGuestErrno(guest::kENOMEM);
  } else if (parsed.special == Special::kNone && (parsed.value.overflow || parsed.value.underflow)) {
    SetGuestErrno(guest::kERANGE);
  }
}

double ToDouble(const Parsed& parsed) noexcept {
  ReportErrors(parsed);
  std::uint64_t bits;
  if (parsed.special == Special::kNan) {
    bits = 0x7ff8000000000000ull;
  } else if (parsed.special == Special::kInfinity) {
    bits = 0x7ff0000000000000ull;
  } else {
    bits = (static_cast<std::uint64_t>(parsed.value.biased_exponent) << 52) |
           static_cast<std::uint64_t>(parsed.value.fraction);
  }
  if (parsed.negative) bits |= 1ull << 63;
  double value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

float ToFloat(const Parsed& parsed) noexcept {
  ReportErrors(parsed);
  std::uint32_t bits;
  if (parsed.special == Special::kNan) {
    bits = 0x7fc00000u;
  } else if (parsed.special == Special::kInfinity) {
    bits = 0x7f800000u;
  } else {
    bits = (parsed.value.biased_exponent << 23) | static_cast<std::uint32_t>(parsed.value.fraction);
  }
  if (parsed.negative) bits |= 1u << 31;
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

__m128i ToLongDouble(const Parsed& parsed) noexcept {
  ReportErrors(parsed);
  abi::GuestLongDouble value;
  if (parsed.special == Special::kNan) {
    value.high = 0x7fff800000000000ull;
    value.low = 0;
  } else if (parsed.special == Special::kInfinity) {
    value.high = 0x7fff000000000000ull;
    value.low = 0;
  } else {
    value.high = (static_cast<std::uint64_t>(parsed.value.biased_exponent) << 48) |
                 static_cast<std::uint64_t>(parsed.value.fraction >> 64);
    value.low = static_cast<std::uint64_t>(parsed.value.fraction);
  }
  if (parsed.negative) value.high |= 1ull << 63;
  __m128i result;
  std::memcpy(&result, &value, sizeof(result));
  return result;
}

}
}

namespace ti = elixir::bionic::text_internal;

ELIXIR_BIONIC_FN(double, strtod)(const char* s, char** end) noexcept {
  return ti::ToDouble(ti::ParseFloat(s, end, ti::kBinary64));
}

ELIXIR_BIONIC_FN(float, strtof)(const char* s, char** end) noexcept {
  return ti::ToFloat(ti::ParseFloat(s, end, ti::kBinary32));
}

ELIXIR_BIONIC_FN(__m128i, strtold)(const char* s, char** end) noexcept {
  return ti::ToLongDouble(ti::ParseFloat(s, end, ti::kBinary128));
}

ELIXIR_BIONIC_FN(double, atof)(const char* s) noexcept {
  return ti::ToDouble(ti::ParseFloat(s, static_cast<char**>(nullptr), ti::kBinary64));
}

ELIXIR_BIONIC_FN(double, wcstod)(const ti::g_wchar* s, ti::g_wchar** end) noexcept {
  return ti::ToDouble(ti::ParseFloat(s, end, ti::kBinary64));
}

ELIXIR_BIONIC_FN(float, wcstof)(const ti::g_wchar* s, ti::g_wchar** end) noexcept {
  return ti::ToFloat(ti::ParseFloat(s, end, ti::kBinary32));
}

ELIXIR_BIONIC_FN(__m128i, wcstold)(const ti::g_wchar* s, ti::g_wchar** end) noexcept {
  return ti::ToLongDouble(ti::ParseFloat(s, end, ti::kBinary128));
}

namespace elixir::bionic::text_internal {

void RegisterFloatParseExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, strtod);
  ELIXIR_TEXT_ALIAS(table, kLibc, strtod_l, strtod);
  ELIXIR_TEXT_EXPORT(table, kLibc, strtof);
  ELIXIR_TEXT_ALIAS(table, kLibc, strtof_l, strtof);
  ELIXIR_TEXT_EXPORT(table, kLibc, strtold);
  ELIXIR_TEXT_ALIAS(table, kLibc, strtold_l, strtold);
  ELIXIR_TEXT_EXPORT(table, kLibc, atof);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstod);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcstod_l, wcstod);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstof);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcstof_l, wcstof);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstold);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcstold_l, wcstold);
}

}
