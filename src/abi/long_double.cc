// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <cstring>

#include "abi/sysv_abi.h"

namespace elixir::abi {
namespace {

constexpr int kQuadBias = 16383;
constexpr int kDoubleBias = 1023;

std::uint64_t DoubleBits(double value) {
  std::uint64_t bits;
  std::memcpy(&bits, &value, 8);
  return bits;
}

double FromBits(std::uint64_t bits) {
  double value;
  std::memcpy(&value, &bits, 8);
  return value;
}

}

double LongDoubleToDouble(const GuestLongDouble& value) noexcept {
  const std::uint64_t sign = value.high >> 63;
  const int exponent = static_cast<int>((value.high >> 48) & 0x7fff);
  const std::uint64_t fraction_high = value.high & 0x0000ffffffffffffull;
  const std::uint64_t fraction_low = value.low;

  if (exponent == 0x7fff) {
    if ((fraction_high | fraction_low) == 0) return FromBits((sign << 63) | 0x7ff0000000000000ull);
    return FromBits((sign << 63) | 0x7ff8000000000000ull | (fraction_high << 4) | (fraction_low >> 60));
  }
  if (exponent == 0 && (fraction_high | fraction_low) == 0) return FromBits(sign << 63);

  int unbiased = exponent - kQuadBias;
  std::uint64_t sig_high = fraction_high;
  std::uint64_t sig_low = fraction_low;
  if (exponent == 0) {
    return FromBits(sign << 63);
  }
  sig_high |= 1ull << 48;

  int double_exponent = unbiased + kDoubleBias;
  std::uint64_t kept;
  std::uint64_t dropped;
  if (double_exponent >= 1) {
    kept = (sig_high << 4) | (sig_low >> 60);
    dropped = sig_low & 0x0fffffffffffffffull;
    const std::uint64_t half = 1ull << 59;
    if (dropped > half || (dropped == half && (kept & 1))) {
      ++kept;
      if (kept == (1ull << 53)) {
        kept >>= 1;
        ++double_exponent;
      }
    }
    if (double_exponent >= 0x7ff) return FromBits((sign << 63) | 0x7ff0000000000000ull);
    return FromBits((sign << 63) | (static_cast<std::uint64_t>(double_exponent) << 52) |
                    (kept & 0x000fffffffffffffull));
  }
  const int shift = 1 - double_exponent;
  if (shift > 54) return FromBits(sign << 63);
  std::uint64_t window = (sig_high << 15) | (sig_low >> 49);
  const bool sticky_low = (sig_low & ((1ull << 49) - 1)) != 0;
  const int total_shift = shift + 11;
  std::uint64_t result = total_shift >= 64 ? 0 : window >> total_shift;
  const std::uint64_t remainder_mask = total_shift >= 64 ? ~0ull : ((1ull << total_shift) - 1);
  const std::uint64_t remainder = window & remainder_mask;
  const std::uint64_t half = total_shift >= 64 ? (1ull << 63) : (1ull << (total_shift - 1));
  const bool above_half = remainder > half || (remainder == half && sticky_low);
  if (above_half || (remainder == half && !sticky_low && (result & 1))) ++result;
  return FromBits((sign << 63) | result);
}

GuestLongDouble DoubleToLongDouble(double value) noexcept {
  const std::uint64_t bits = DoubleBits(value);
  const std::uint64_t sign = bits >> 63;
  const int exponent = static_cast<int>((bits >> 52) & 0x7ff);
  std::uint64_t fraction = bits & 0x000fffffffffffffull;
  GuestLongDouble out{};
  if (exponent == 0x7ff) {
    out.high = (sign << 63) | (0x7fffull << 48) | (fraction >> 4);
    out.low = fraction << 60;
    return out;
  }
  if (exponent == 0) {
    if (fraction == 0) {
      out.high = sign << 63;
      return out;
    }
    int shift = 0;
    while ((fraction & (1ull << 52)) == 0) {
      fraction <<= 1;
      ++shift;
    }
    fraction &= 0x000fffffffffffffull;
    const int quad_exponent = 1 - kDoubleBias - shift + kQuadBias;
    out.high = (sign << 63) | (static_cast<std::uint64_t>(quad_exponent) << 48) | (fraction >> 4);
    out.low = fraction << 60;
    return out;
  }
  const int quad_exponent = exponent - kDoubleBias + kQuadBias;
  out.high = (sign << 63) | (static_cast<std::uint64_t>(quad_exponent) << 48) | (fraction >> 4);
  out.low = fraction << 60;
  return out;
}

}
