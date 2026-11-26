// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The hdtoa behaviour is adapted from AOSP bionic
// libc/upstream-openbsd/lib/libc/gdtoa/hdtoa.c:
//
//   Copyright (c) 2004-2008 David Schultz <das@FreeBSD.ORG>
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
//   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
//   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
//   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
//   SUCH DAMAGE.
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "bionic/format/format_internal.h"

namespace elixir::bionic::format::internal {
namespace {

using u128 = unsigned __int128;

int BitLength64(std::uint64_t value) noexcept { return value == 0 ? 0 : 64 - __builtin_clzll(value); }

int BitLength128(u128 value) noexcept {
  const auto high = static_cast<std::uint64_t>(value >> 64);
  if (high != 0) return 64 + BitLength64(high);
  return BitLength64(static_cast<std::uint64_t>(value));
}

constexpr std::uint32_t kDecimalBase = 1000000000u;

class DecimalBig {
 public:
  explicit DecimalBig(u128 value) {
    std::uint32_t words[4] = {static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32),
                              static_cast<std::uint32_t>(value >> 64), static_cast<std::uint32_t>(value >> 96)};
    for (;;) {
      bool nonzero = false;
      std::uint64_t remainder = 0;
      for (int i = 3; i >= 0; --i) {
        const std::uint64_t current = (remainder << 32) | words[i];
        words[i] = static_cast<std::uint32_t>(current / kDecimalBase);
        remainder = current % kDecimalBase;
        nonzero |= words[i] != 0;
      }
      limbs_.push_back(static_cast<std::uint32_t>(remainder));
      if (!nonzero) break;
    }
  }

  void Multiply(std::uint64_t factor) {
    std::uint64_t carry = 0;
    for (std::uint32_t& limb : limbs_) {
      const std::uint64_t product = static_cast<std::uint64_t>(limb) * factor + carry;
      limb = static_cast<std::uint32_t>(product % kDecimalBase);
      carry = product / kDecimalBase;
    }
    while (carry != 0) {
      limbs_.push_back(static_cast<std::uint32_t>(carry % kDecimalBase));
      carry /= kDecimalBase;
    }
  }

  std::string ToString() const {
    std::string out;
    out.reserve(limbs_.size() * 9);
    char chunk[16];
    std::size_t top = limbs_.size();
    while (top > 1 && limbs_[top - 1] == 0) --top;
    for (std::size_t index = top; index-- > 0;) {
      std::uint32_t limb = limbs_[index];
      for (int i = 8; i >= 0; --i) {
        chunk[i] = static_cast<char>('0' + limb % 10);
        limb /= 10;
      }
      if (index == top - 1) {
        int first = 0;
        while (first < 8 && chunk[first] == '0') ++first;
        out.append(chunk + first, 9 - first);
      } else {
        out.append(chunk, 9);
      }
    }
    return out;
  }

 private:
  std::vector<std::uint32_t> limbs_;
};

void ExactDecimal(u128 significand, int exponent2, std::string* digits, int* decimal_point) {
  while (exponent2 < 0 && (significand & 1) == 0) {
    significand >>= 1;
    ++exponent2;
  }
  DecimalBig number(significand);
  int decimal_exponent = 0;
  if (exponent2 > 0) {
    int remaining = exponent2;
    while (remaining >= 32) {
      number.Multiply(std::uint64_t{1} << 32);
      remaining -= 32;
    }
    if (remaining > 0) number.Multiply(std::uint64_t{1} << remaining);
  } else if (exponent2 < 0) {
    static constexpr std::uint32_t kPow5[14] = {1,       5,        25,        125,       625,
                                                3125,    15625,    78125,     390625,    1953125,
                                                9765625, 48828125, 244140625, 1220703125};
    int remaining = -exponent2;
    while (remaining >= 13) {
      number.Multiply(kPow5[13]);
      remaining -= 13;
    }
    if (remaining > 0) number.Multiply(kPow5[remaining]);
    decimal_exponent = exponent2;
  }
  *digits = number.ToString();
  *decimal_point = static_cast<int>(digits->size()) + decimal_exponent;
}

void RoundDigits(std::string* digits, std::size_t keep, int* decimal_point) {
  if (digits->size() <= keep) return;
  const char next = (*digits)[keep];
  bool round_up = next > '5';
  if (next == '5') {
    bool rest_nonzero = false;
    for (std::size_t i = keep + 1; i < digits->size(); ++i) {
      if ((*digits)[i] != '0') {
        rest_nonzero = true;
        break;
      }
    }
    round_up = rest_nonzero || (((*digits)[keep - 1] - '0') & 1) != 0;
  }
  digits->resize(keep);
  if (!round_up) return;
  for (std::size_t i = keep; i-- > 0;) {
    if ((*digits)[i] != '9') {
      ++(*digits)[i];
      return;
    }
    (*digits)[i] = '0';
  }
  *digits = "1";
  ++*decimal_point;
}

void StripTrailingZeros(std::string* digits) {
  std::size_t end = digits->size();
  while (end > 1 && (*digits)[end - 1] == '0') --end;
  digits->resize(end);
}

class BinaryBig {
 public:
  BinaryBig() = default;

  void SetSmall(std::uint32_t value) {
    limbs_.assign(1, value);
    Trim();
  }

  void Multiply(std::uint32_t factor) {
    std::uint64_t carry = 0;
    for (std::uint32_t& limb : limbs_) {
      const std::uint64_t product = static_cast<std::uint64_t>(limb) * factor + carry;
      limb = static_cast<std::uint32_t>(product);
      carry = product >> 32;
    }
    if (carry != 0) limbs_.push_back(static_cast<std::uint32_t>(carry));
  }

  void Add(std::uint32_t addend) {
    std::uint64_t carry = addend;
    for (std::size_t i = 0; i < limbs_.size() && carry != 0; ++i) {
      const std::uint64_t sum = static_cast<std::uint64_t>(limbs_[i]) + carry;
      limbs_[i] = static_cast<std::uint32_t>(sum);
      carry = sum >> 32;
    }
    if (carry != 0) limbs_.push_back(static_cast<std::uint32_t>(carry));
  }

  void SetDecimal(const std::string& digits) {
    limbs_.clear();
    std::size_t position = 0;
    std::size_t chunk = digits.size() % 9;
    if (chunk == 0) chunk = 9;
    while (position < digits.size()) {
      std::uint32_t value = 0;
      for (std::size_t i = 0; i < chunk; ++i) value = value * 10 + static_cast<std::uint32_t>(digits[position + i] - '0');
      Multiply(1000000000u);
      Add(value);
      position += chunk;
      chunk = 9;
    }
    Trim();
  }

  void MultiplyPow10(int power) {
    static constexpr std::uint32_t kPow10[10] = {1,      10,      100,      1000,      10000,
                                                 100000, 1000000, 10000000, 100000000, 1000000000};
    while (power >= 9) {
      Multiply(kPow10[9]);
      power -= 9;
    }
    if (power > 0) Multiply(kPow10[power]);
  }

  void ShiftLeft(int bits) {
    if (bits <= 0 || limbs_.empty()) return;
    const std::size_t words = static_cast<std::size_t>(bits / 32);
    const int rest = bits % 32;
    if (rest != 0) {
      std::uint32_t carry = 0;
      for (std::uint32_t& limb : limbs_) {
        const std::uint32_t next_carry = limb >> (32 - rest);
        limb = (limb << rest) | carry;
        carry = next_carry;
      }
      if (carry != 0) limbs_.push_back(carry);
    }
    limbs_.insert(limbs_.begin(), words, 0u);
  }

  void ShiftRightOne() {
    for (std::size_t i = 0; i < limbs_.size(); ++i) {
      std::uint32_t value = limbs_[i] >> 1;
      if (i + 1 < limbs_.size()) value |= limbs_[i + 1] << 31;
      limbs_[i] = value;
    }
    Trim();
  }

  int BitLength() const {
    if (limbs_.empty()) return 0;
    return static_cast<int>((limbs_.size() - 1) * 32) + BitLength64(limbs_.back());
  }

  bool IsZero() const { return limbs_.empty(); }

  int Compare(const BinaryBig& other) const {
    if (limbs_.size() != other.limbs_.size()) return limbs_.size() < other.limbs_.size() ? -1 : 1;
    for (std::size_t i = limbs_.size(); i-- > 0;) {
      if (limbs_[i] != other.limbs_[i]) return limbs_[i] < other.limbs_[i] ? -1 : 1;
    }
    return 0;
  }

  void Subtract(const BinaryBig& other) {
    std::int64_t borrow = 0;
    for (std::size_t i = 0; i < limbs_.size(); ++i) {
      std::int64_t value = static_cast<std::int64_t>(limbs_[i]) - borrow -
                           (i < other.limbs_.size() ? static_cast<std::int64_t>(other.limbs_[i]) : 0);
      borrow = value < 0 ? 1 : 0;
      if (value < 0) value += std::int64_t{1} << 32;
      limbs_[i] = static_cast<std::uint32_t>(value);
    }
    Trim();
  }

  u128 Low128() const {
    u128 value = 0;
    for (std::size_t i = std::min<std::size_t>(limbs_.size(), 4); i-- > 0;) value = (value << 32) | limbs_[i];
    return value;
  }

  u128 Top128(bool* sticky) const {
    const int length = BitLength();
    const int drop = length - 128;
    u128 value = 0;
    for (int bit = length - 1; bit >= drop; --bit) value = (value << 1) | Bit(bit);
    bool any = false;
    const std::size_t full_words = static_cast<std::size_t>(drop / 32);
    for (std::size_t i = 0; i < full_words && !any; ++i) any = limbs_[i] != 0;
    for (int bit = static_cast<int>(full_words * 32); bit < drop && !any; ++bit) any = Bit(bit) != 0;
    *sticky = any;
    return value;
  }

 private:
  unsigned Bit(int index) const {
    return (limbs_[static_cast<std::size_t>(index / 32)] >> (index % 32)) & 1u;
  }
  void Trim() {
    while (!limbs_.empty() && limbs_.back() == 0) limbs_.pop_back();
  }

  std::vector<std::uint32_t> limbs_;
};

u128 Divide(BinaryBig numerator, BinaryBig denominator, bool* inexact) {
  const int quotient_bits = numerator.BitLength() - denominator.BitLength() + 1;
  if (quotient_bits <= 0) {
    *inexact = !numerator.IsZero();
    return 0;
  }
  denominator.ShiftLeft(quotient_bits - 1);
  u128 quotient = 0;
  for (int bit = quotient_bits - 1; bit >= 0; --bit) {
    if (numerator.Compare(denominator) >= 0) {
      numerator.Subtract(denominator);
      quotient |= static_cast<u128>(1) << bit;
    }
    denominator.ShiftRightOne();
  }
  *inexact = !numerator.IsZero();
  return quotient;
}

struct FormatSpec {
  int precision;
  int min_exponent;
  int max_exponent;
};

FormatSpec SpecFor(FloatKind kind) {
  switch (kind) {
    case FloatKind::kFloat:
      return {24, -126, 127};
    case FloatKind::kDouble:
      return {53, -1022, 1023};
    case FloatKind::kQuad:
    default:
      return {113, -16382, 16383};
  }
}

void EncodeSpecial(FloatKind kind, bool negative, bool nan, std::uint64_t* high, std::uint64_t* low) {
  switch (kind) {
    case FloatKind::kFloat:
      *high = 0;
      *low = (negative ? 0x80000000ull : 0) | (nan ? 0x7fc00000ull : 0x7f800000ull);
      break;
    case FloatKind::kDouble:
      *high = 0;
      *low = (negative ? 0x8000000000000000ull : 0) | (nan ? 0x7ff8000000000000ull : 0x7ff0000000000000ull);
      break;
    case FloatKind::kQuad:
      *high = (negative ? 0x8000000000000000ull : 0) | (nan ? 0x7fff800000000000ull : 0x7fff000000000000ull);
      *low = 0;
      break;
  }
}

void EncodeFinite(FloatKind kind, bool negative, int biased_exponent, u128 fraction, std::uint64_t* high,
                  std::uint64_t* low) {
  switch (kind) {
    case FloatKind::kFloat:
      *high = 0;
      *low = (negative ? 0x80000000ull : 0) | (static_cast<std::uint64_t>(biased_exponent) << 23) |
             (static_cast<std::uint64_t>(fraction) & 0x7fffffull);
      break;
    case FloatKind::kDouble:
      *high = 0;
      *low = (negative ? 0x8000000000000000ull : 0) | (static_cast<std::uint64_t>(biased_exponent) << 52) |
             (static_cast<std::uint64_t>(fraction) & 0x000fffffffffffffull);
      break;
    case FloatKind::kQuad:
      *high = (negative ? 0x8000000000000000ull : 0) | (static_cast<std::uint64_t>(biased_exponent) << 48) |
              (static_cast<std::uint64_t>(fraction >> 64) & 0x0000ffffffffffffull);
      *low = static_cast<std::uint64_t>(fraction);
      break;
  }
}

void MakeFloat(u128 mantissa, bool sticky, int exponent2, FloatKind kind, bool negative, bool decimal_double,
               std::uint64_t* high, std::uint64_t* low, bool* range_error) {
  const FormatSpec spec = SpecFor(kind);
  if (mantissa == 0) {
    EncodeFinite(kind, negative, 0, 0, high, low);
    if (sticky) *range_error = true;
    return;
  }
  int length = BitLength128(mantissa);
  if (length < 125) {
    mantissa <<= (125 - length);
    exponent2 -= 125 - length;
    length = 125;
  }
  int exponent = exponent2 + length - 1;
  if (exponent > spec.max_exponent) {
    EncodeSpecial(kind, negative, false, high, low);
    *range_error = true;
    return;
  }
  const bool normal = exponent >= spec.min_exponent;
  const int lsb_exponent = (normal ? exponent : spec.min_exponent) - (spec.precision - 1);
  const int shift = lsb_exponent - exponent2;
  u128 kept;
  bool round_up;
  bool inexact;
  if (shift > 128) {
    kept = 0;
    round_up = false;
    inexact = true;
  } else if (shift == 128) {
    kept = 0;
    const u128 half = static_cast<u128>(1) << 127;
    round_up = mantissa > half || (mantissa == half && sticky);
    inexact = true;
  } else {
    kept = mantissa >> shift;
    const u128 remainder = mantissa & ((static_cast<u128>(1) << shift) - 1);
    const u128 half = static_cast<u128>(1) << (shift - 1);
    round_up = remainder > half || (remainder == half && (sticky || (kept & 1) != 0));
    inexact = remainder != 0 || sticky;
  }
  if (round_up) ++kept;
  const u128 hidden = static_cast<u128>(1) << (spec.precision - 1);
  const int bias = spec.max_exponent;
  if (normal) {
    if (kept == (hidden << 1)) {
      kept >>= 1;
      ++exponent;
      if (exponent > spec.max_exponent) {
        EncodeSpecial(kind, negative, false, high, low);
        *range_error = true;
        return;
      }
    }
    EncodeFinite(kind, negative, exponent + bias, kept - hidden, high, low);
    return;
  }
  if (kept == hidden) {
    EncodeFinite(kind, negative, 1, 0, high, low);
  } else {
    EncodeFinite(kind, negative, 0, kept, high, low);
  }
  if (kept < hidden && (inexact || decimal_double)) *range_error = true;
}

bool MatchesIgnoreCase(const char* text, std::size_t length, std::size_t position, const char* word) {
  const std::size_t word_length = std::strlen(word);
  if (position + word_length > length) return false;
  for (std::size_t i = 0; i < word_length; ++i) {
    if ((text[position + i] | 0x20) != word[i]) return false;
  }
  return true;
}

int HexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  return (c | 0x20) - 'a' + 10;
}

bool ParseExponent(const char* text, std::size_t length, std::size_t* position, char marker, int* exponent) {
  std::size_t i = *position;
  if (i >= length || (text[i] | 0x20) != marker) return false;
  ++i;
  bool negative = false;
  if (i < length && (text[i] == '+' || text[i] == '-')) {
    negative = text[i] == '-';
    ++i;
  }
  if (i >= length || !IsDigit(text[i])) return false;
  int value = 0;
  while (i < length && IsDigit(text[i])) {
    if (value < 100000000) value = value * 10 + (text[i] - '0');
    ++i;
  }
  *exponent = negative ? -value : value;
  *position = i;
  return true;
}

}

DecodedFloat DecodeDouble(double value) noexcept {
  std::uint64_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  DecodedFloat out;
  out.kind = FloatKind::kDouble;
  out.negative = (bits >> 63) != 0;
  const int biased = static_cast<int>((bits >> 52) & 0x7ff);
  const std::uint64_t fraction = bits & 0x000fffffffffffffull;
  out.biased_exponent = biased;
  out.fraction_low = fraction;
  if (biased == 0x7ff) {
    out.cls = fraction != 0 ? DecodedFloat::kNan : DecodedFloat::kInfinite;
    return out;
  }
  if (biased == 0 && fraction == 0) {
    out.cls = DecodedFloat::kZero;
    return out;
  }
  out.cls = DecodedFloat::kFinite;
  out.sig_low = biased != 0 ? (fraction | (1ull << 52)) : fraction;
  out.exponent2 = (biased != 0 ? biased : 1) - 1075;
  return out;
}

DecodedFloat DecodeQuad(const abi::GuestLongDouble& value) noexcept {
  DecodedFloat out;
  out.kind = FloatKind::kQuad;
  out.negative = (value.high >> 63) != 0;
  const int biased = static_cast<int>((value.high >> 48) & 0x7fff);
  const std::uint64_t fraction_high = value.high & 0x0000ffffffffffffull;
  out.biased_exponent = biased;
  out.fraction_high = fraction_high;
  out.fraction_low = value.low;
  if (biased == 0x7fff) {
    out.cls = (fraction_high | value.low) != 0 ? DecodedFloat::kNan : DecodedFloat::kInfinite;
    return out;
  }
  if (biased == 0 && (fraction_high | value.low) == 0) {
    out.cls = DecodedFloat::kZero;
    return out;
  }
  out.cls = DecodedFloat::kFinite;
  out.sig_high = biased != 0 ? (fraction_high | (1ull << 48)) : fraction_high;
  out.sig_low = value.low;
  out.exponent2 = (biased != 0 ? biased : 1) - 16495;
  return out;
}

void DecimalDigits(const DecodedFloat& value, int mode, int ndigits, std::string* digits, int* decimal_point) {
  if (value.cls == DecodedFloat::kZero) {
    *digits = "0";
    *decimal_point = 1;
    return;
  }
  const u128 significand = (static_cast<u128>(value.sig_high) << 64) | value.sig_low;
  ExactDecimal(significand, value.exponent2, digits, decimal_point);
  if (mode == 2) {
    RoundDigits(digits, static_cast<std::size_t>(ndigits < 1 ? 1 : ndigits), decimal_point);
  } else {
    const long long keep = static_cast<long long>(*decimal_point) + ndigits;
    if (keep < 0) {
      digits->clear();
      *decimal_point = -ndigits;
      return;
    }
    if (keep == 0) {
      bool above_half = (*digits)[0] > '5';
      if ((*digits)[0] == '5') {
        for (std::size_t i = 1; i < digits->size(); ++i) {
          if ((*digits)[i] != '0') {
            above_half = true;
            break;
          }
        }
      }
      if (above_half) {
        *digits = "1";
        ++*decimal_point;
      } else {
        digits->clear();
        *decimal_point = -ndigits;
      }
      return;
    }
    RoundDigits(digits, static_cast<std::size_t>(keep), decimal_point);
  }
  StripTrailingZeros(digits);
}

void HexDigits(const DecodedFloat& value, int ndigits, std::string* digits, int* exponent) {
  const bool quad = value.kind == FloatKind::kQuad;
  const int sigfigs = quad ? 29 : 14;
  if (value.cls == DecodedFloat::kZero) {
    digits->assign(1, '\0');
    *exponent = 1;
    return;
  }
  u128 fraction;
  int fraction_bits;
  int decpt;
  if (quad) {
    fraction = (static_cast<u128>(value.fraction_high) << 64) | value.fraction_low;
    fraction_bits = 112;
    if (value.biased_exponent == 0) {
      const int msb = BitLength128(fraction) - 1;
      fraction = (fraction << (112 - msb)) & ((static_cast<u128>(1) << 112) - 1);
      decpt = msb - 16493;
    } else {
      decpt = value.biased_exponent - 16382;
    }
  } else {
    fraction = value.fraction_low;
    fraction_bits = 52;
    if (value.biased_exponent == 0) {
      const int msb = BitLength64(value.fraction_low) - 1;
      fraction = (fraction << (52 - msb)) & ((static_cast<u128>(1) << 52) - 1);
      decpt = msb - 1073;
    } else {
      decpt = value.biased_exponent - 1022;
    }
  }
  if (ndigits == 0) ndigits = 1;
  if (ndigits > sigfigs) ndigits = sigfigs;
  std::vector<std::uint8_t> nibbles(static_cast<std::size_t>(sigfigs) + 2, 0);
  nibbles[0] = 1;
  for (int i = 1; i < sigfigs; ++i) {
    nibbles[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((fraction >> (fraction_bits - 4 * i)) & 0xf);
  }
  if (ndigits < 0) {
    ndigits = sigfigs;
    while (nibbles[static_cast<std::size_t>(ndigits - 1)] == 0) --ndigits;
  }
  const auto n = static_cast<std::size_t>(ndigits);
  if (sigfigs > ndigits && nibbles[n] != 0) {
    if (nibbles[n] > 8 || (nibbles[n] == 8 && (nibbles[n + 1] & 1) != 0)) {
      std::size_t i = n - 1;
      bool overflow = false;
      for (;;) {
        if (nibbles[i] != 0xf) {
          ++nibbles[i];
          break;
        }
        if (i == 0) {
          nibbles[0] = 1;
          overflow = true;
          break;
        }
        nibbles[i] = 0;
        --i;
      }
      if (overflow) decpt += 4;
    }
  }
  digits->assign(nibbles.begin(), nibbles.begin() + static_cast<std::ptrdiff_t>(n));
  *exponent = decpt;
}

std::size_t ParseFloat(const char* text, std::size_t length, FloatKind kind, std::uint64_t* bits_high,
                       std::uint64_t* bits_low, bool* range_error) {
  *range_error = false;
  *bits_high = 0;
  *bits_low = 0;
  std::size_t i = 0;
  bool negative = false;
  if (i < length && (text[i] == '+' || text[i] == '-')) {
    negative = text[i] == '-';
    ++i;
  }
  if (i >= length) return 0;
  if ((text[i] | 0x20) == 'i') {
    if (!MatchesIgnoreCase(text, length, i, "inf")) return 0;
    i += 3;
    if (MatchesIgnoreCase(text, length, i, "inity")) i += 5;
    EncodeSpecial(kind, negative, false, bits_high, bits_low);
    return i;
  }
  if ((text[i] | 0x20) == 'n') {
    if (!MatchesIgnoreCase(text, length, i, "nan")) return 0;
    i += 3;
    if (i < length && text[i] == '(') {
      std::size_t j = i + 1;
      while (j < length && (IsAlnum(text[j]) || text[j] == '_')) ++j;
      if (j < length && text[j] == ')') i = j + 1;
    }
    EncodeSpecial(kind, negative, true, bits_high, bits_low);
    return i;
  }

  if (text[i] == '0' && i + 1 < length && (text[i + 1] | 0x20) == 'x' &&
      ((i + 2 < length && IsXdigit(text[i + 2])) ||
       (i + 3 < length && text[i + 2] == '.' && IsXdigit(text[i + 3])))) {
    i += 2;
    u128 mantissa = 0;
    int significant = 0;
    int exponent2 = 0;
    bool sticky = false;
    while (i < length && IsXdigit(text[i])) {
      const int digit = HexValue(text[i]);
      if (significant == 0 && digit == 0) {
      } else if (significant < 30) {
        mantissa = (mantissa << 4) | static_cast<u128>(digit);
        ++significant;
      } else {
        sticky |= digit != 0;
        exponent2 += 4;
      }
      ++i;
    }
    if (i < length && text[i] == '.') {
      ++i;
      while (i < length && IsXdigit(text[i])) {
        const int digit = HexValue(text[i]);
        if (significant == 0 && digit == 0) {
          exponent2 -= 4;
        } else if (significant < 30) {
          mantissa = (mantissa << 4) | static_cast<u128>(digit);
          ++significant;
          exponent2 -= 4;
        } else {
          sticky |= digit != 0;
        }
        ++i;
      }
    }
    int binary_exponent = 0;
    if (ParseExponent(text, length, &i, 'p', &binary_exponent)) exponent2 += binary_exponent;
    if (mantissa == 0) {
      EncodeFinite(kind, negative, 0, 0, bits_high, bits_low);
    } else {
      MakeFloat(mantissa, sticky, exponent2, kind, negative, false, bits_high, bits_low, range_error);
    }
    return i;
  }

  std::string digits;
  bool any_digit = false;
  int decimal_exponent = 0;
  while (i < length && IsDigit(text[i])) {
    any_digit = true;
    if (!(digits.empty() && text[i] == '0')) digits.push_back(text[i]);
    ++i;
  }
  if (i < length && text[i] == '.') {
    const std::size_t point = i;
    ++i;
    bool fraction_digit = false;
    while (i < length && IsDigit(text[i])) {
      fraction_digit = true;
      if (!(digits.empty() && text[i] == '0')) digits.push_back(text[i]);
      --decimal_exponent;
      ++i;
    }
    if (!any_digit && !fraction_digit) return 0;
    if (!fraction_digit) i = point + 1;
    any_digit = true;
  }
  if (!any_digit) return 0;
  int written_exponent = 0;
  if (ParseExponent(text, length, &i, 'e', &written_exponent)) {
    decimal_exponent = std::max(-200000000, std::min(200000000, decimal_exponent + written_exponent));
  }
  while (!digits.empty() && digits.back() == '0') {
    digits.pop_back();
    ++decimal_exponent;
  }
  if (digits.empty()) {
    EncodeFinite(kind, negative, 0, 0, bits_high, bits_low);
    return i;
  }
  const long long magnitude = static_cast<long long>(digits.size()) + decimal_exponent;
  int overflow_limit;
  int zero_limit;
  switch (kind) {
    case FloatKind::kFloat:
      overflow_limit = 39;
      zero_limit = -45;
      break;
    case FloatKind::kDouble:
      overflow_limit = 309;
      zero_limit = -323;
      break;
    case FloatKind::kQuad:
    default:
      overflow_limit = 4933;
      zero_limit = -4965;
      break;
  }
  if (magnitude > overflow_limit) {
    EncodeSpecial(kind, negative, false, bits_high, bits_low);
    *range_error = true;
    return i;
  }
  if (magnitude < zero_limit) {
    EncodeFinite(kind, negative, 0, 0, bits_high, bits_low);
    *range_error = true;
    return i;
  }
  BinaryBig numerator;
  numerator.SetDecimal(digits);
  u128 mantissa;
  bool sticky = false;
  int exponent2;
  if (decimal_exponent >= 0) {
    numerator.MultiplyPow10(decimal_exponent);
    const int bits = numerator.BitLength();
    if (bits <= 128) {
      mantissa = numerator.Low128();
      exponent2 = 0;
    } else {
      mantissa = numerator.Top128(&sticky);
      exponent2 = bits - 128;
    }
  } else {
    BinaryBig denominator;
    denominator.SetSmall(1);
    denominator.MultiplyPow10(-decimal_exponent);
    const int scale = 125 - (numerator.BitLength() - denominator.BitLength());
    if (scale >= 0) {
      numerator.ShiftLeft(scale);
    } else {
      denominator.ShiftLeft(-scale);
    }
    mantissa = Divide(numerator, denominator, &sticky);
    exponent2 = -scale;
  }
  MakeFloat(mantissa, sticky, exponent2, kind, negative, kind == FloatKind::kDouble, bits_high, bits_low,
            range_error);
  return i;
}

}
