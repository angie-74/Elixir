// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <emmintrin.h>

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "abi/sysv_abi.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"

namespace elixir::bionic {
namespace math {
extern int g_signgam;
}

namespace ld {
namespace {

using u128 = unsigned __int128;
using abi::GuestLongDouble;

constexpr int kBias = 16383;
constexpr int kMaxExponentField = 0x7fff;
constexpr int kFractionBits = 112;
constexpr u128 kOne = 1;
constexpr u128 kFractionMask = (kOne << kFractionBits) - 1;
constexpr u128 kImplicitBit = kOne << kFractionBits;
constexpr u128 kSignBit = kOne << 127;
constexpr int kFpInfinite = 0x01, kFpNan = 0x02, kFpNormal = 0x04, kFpSubnormal = 0x08, kFpZero = 0x10;

u128 BitsOf(__m128i value) noexcept {
  GuestLongDouble v;
  std::memcpy(&v, &value, sizeof(v));
  return (static_cast<u128>(v.high) << 64) | v.low;
}
__m128i FromBits(u128 bits) noexcept {
  GuestLongDouble v;
  v.low = static_cast<std::uint64_t>(bits);
  v.high = static_cast<std::uint64_t>(bits >> 64);
  __m128i out;
  std::memcpy(&out, &v, sizeof(out));
  return out;
}
double ToDouble(__m128i value) noexcept {
  GuestLongDouble v;
  std::memcpy(&v, &value, sizeof(v));
  return abi::LongDoubleToDouble(v);
}
__m128i FromDouble(double value) noexcept {
  const GuestLongDouble v = abi::DoubleToLongDouble(value);
  __m128i out;
  std::memcpy(&out, &v, sizeof(out));
  return out;
}

int ExponentField(u128 bits) noexcept { return static_cast<int>((bits >> kFractionBits) & kMaxExponentField); }
u128 Fraction(u128 bits) noexcept { return bits & kFractionMask; }
bool Negative(u128 bits) noexcept { return (bits >> 127) != 0; }

int Classify(u128 bits) noexcept {
  const int exponent = ExponentField(bits);
  const u128 fraction = Fraction(bits);
  if (exponent == kMaxExponentField) return fraction != 0 ? kFpNan : kFpInfinite;
  if (exponent == 0) return fraction != 0 ? kFpSubnormal : kFpZero;
  return kFpNormal;
}

int LeadingZeros(u128 value) noexcept {
  const auto high = static_cast<std::uint64_t>(value >> 64);
  const auto low = static_cast<std::uint64_t>(value);
  if (high != 0) return __builtin_clzll(high);
  if (low != 0) return 64 + __builtin_clzll(low);
  return 128;
}

void Unpack(u128 bits, int* exponent, u128* significand) noexcept {
  const int field = ExponentField(bits);
  const u128 fraction = Fraction(bits);
  if (field == 0) {
    const int shift = LeadingZeros(fraction) - 15;
    *significand = fraction << shift;
    *exponent = 1 - kBias - shift;
  } else {
    *significand = fraction | kImplicitBit;
    *exponent = field - kBias;
  }
}

u128 Pack(bool negative, std::int64_t exponent, u128 significand) noexcept {
  const u128 sign = negative ? kSignBit : 0;
  if (exponent > kBias) return sign | (static_cast<u128>(kMaxExponentField) << kFractionBits);
  if (exponent >= 1 - kBias) {
    return sign | (static_cast<u128>(exponent + kBias) << kFractionBits) | (significand & kFractionMask);
  }
  const std::int64_t shift = (1 - kBias) - exponent;
  if (shift > 113) return sign;
  const u128 kept = significand >> shift;
  const u128 dropped = significand & ((kOne << shift) - 1);
  const u128 half = kOne << (shift - 1);
  u128 result = kept;
  if (dropped > half || (dropped == half && (kept & 1) != 0)) ++result;
  return sign | result;
}

constexpr u128 kQuietNan = static_cast<u128>(0x7fff800000000000ull) << 64;
constexpr u128 kInfinity = static_cast<u128>(kMaxExponentField) << kFractionBits;

u128 Quiet(u128 bits) noexcept { return bits | (kOne << 111); }

enum class Mode { kNearestEven, kDown, kUp, kTowardZero, kNearestAway };

Mode CurrentMode() noexcept {
  switch ((_mm_getcsr() >> 13) & 3) {
    case 1: return Mode::kDown;
    case 2: return Mode::kUp;
    case 3: return Mode::kTowardZero;
    default: return Mode::kNearestEven;
  }
}

u128 RoundIntegral(u128 bits, Mode mode) noexcept {
  const int field = ExponentField(bits);
  if (field == kMaxExponentField) return Classify(bits) == kFpNan ? Quiet(bits) : bits;
  if (Classify(bits) == kFpZero) return bits;
  const bool negative = Negative(bits);
  const u128 sign = negative ? kSignBit : 0;
  const int exponent = field - kBias;
  if (exponent >= kFractionBits) return bits;
  const u128 one = static_cast<u128>(kBias) << kFractionBits;
  if (exponent < 0) {
    bool to_one = false;
    switch (mode) {
      case Mode::kTowardZero: break;
      case Mode::kDown: to_one = negative; break;
      case Mode::kUp: to_one = !negative; break;
      case Mode::kNearestAway: to_one = exponent == -1; break;
      case Mode::kNearestEven: to_one = exponent == -1 && Fraction(bits) != 0; break;
    }
    return sign | (to_one ? one : 0);
  }
  const int fraction_bits = kFractionBits - exponent;
  const u128 significand = Fraction(bits) | kImplicitBit;
  const u128 mask = (kOne << fraction_bits) - 1;
  const u128 remainder = significand & mask;
  if (remainder == 0) return bits;
  u128 integral = significand & ~mask;
  const u128 half = kOne << (fraction_bits - 1);
  bool increment = false;
  switch (mode) {
    case Mode::kTowardZero: break;
    case Mode::kDown: increment = negative; break;
    case Mode::kUp: increment = !negative; break;
    case Mode::kNearestAway: increment = remainder >= half; break;
    case Mode::kNearestEven:
      increment = remainder > half || (remainder == half && ((integral >> fraction_bits) & 1) != 0);
      break;
  }
  int result_exponent = exponent;
  if (increment) {
    integral += kOne << fraction_bits;
    if ((integral >> (kFractionBits + 1)) != 0) {
      integral >>= 1;
      ++result_exponent;
    }
  }
  return sign | (static_cast<u128>(result_exponent + kBias) << kFractionBits) | (integral & kFractionMask);
}

std::int64_t ToInt64(u128 integral) noexcept {
  const int kind = Classify(integral);
  if (kind == kFpZero) return 0;
  if (kind == kFpNan || kind == kFpInfinite) return INT64_MIN;
  const int exponent = ExponentField(integral) - kBias;
  if (exponent < 0) return 0;
  if (exponent > 62) return INT64_MIN;
  const u128 significand = Fraction(integral) | kImplicitBit;
  const auto magnitude = static_cast<std::int64_t>(significand >> (kFractionBits - exponent));
  return Negative(integral) ? -magnitude : magnitude;
}

__int128 OrderKey(u128 bits) noexcept {
  const u128 magnitude = bits & ~kSignBit;
  return Negative(bits) ? -static_cast<__int128>(magnitude) : static_cast<__int128>(magnitude);
}
u128 FromOrderKey(__int128 key) noexcept {
  return key < 0 ? (kSignBit | static_cast<u128>(-key)) : static_cast<u128>(key);
}

u128 Scale(u128 bits, std::int64_t n) noexcept {
  const int kind = Classify(bits);
  if (kind == kFpZero || kind == kFpNan || kind == kFpInfinite) return kind == kFpNan ? Quiet(bits) : bits;
  if (n > 100000) n = 100000;
  if (n < -100000) n = -100000;
  int exponent;
  u128 significand;
  Unpack(bits, &exponent, &significand);
  return Pack(Negative(bits), exponent + n, significand);
}

void SplitDouble(u128 bits, double* mantissa, int* exponent) noexcept {
  u128 significand;
  Unpack(bits, exponent, &significand);
  const u128 one_bits = (static_cast<u128>(kBias) << kFractionBits) | (significand & kFractionMask);
  *mantissa = ToDouble(FromBits(one_bits));
}

__m128i Exp2Wide(double x) noexcept {
  if (std::isnan(x)) return FromDouble(x);
  if (x > 16384.0) return FromBits(kInfinity);
  if (x < -16500.0) return FromBits(0);
  const double whole = std::floor(x);
  const double fraction = std::exp2(x - whole);
  return FromBits(Scale(BitsOf(FromDouble(fraction)), static_cast<std::int64_t>(whole)));
}

constexpr double kLog2E = 1.4426950408889634073599246810019;
constexpr double kLn2 = 0.69314718055994530941723212145818;

__m128i ExpWide(double x) noexcept {
  if (x <= 709.0 && x >= -708.0) return FromDouble(std::exp(x));
  return Exp2Wide(x * kLog2E);
}

double LogWide(u128 bits) noexcept {
  double mantissa;
  int exponent;
  SplitDouble(bits, &mantissa, &exponent);
  return std::log(mantissa) + exponent * kLn2;
}

}
}

using ld::BitsOf;
using ld::FromBits;
using ld::FromDouble;
using ld::ToDouble;

ELIXIR_BIONIC_FN(int, __fpclassifyl)(__m128i x) noexcept { return ld::Classify(BitsOf(x)); }
ELIXIR_BIONIC_FN(int, __isfinitel)(__m128i x) noexcept {
  const int kind = ld::Classify(BitsOf(x));
  return kind != ld::kFpNan && kind != ld::kFpInfinite ? 1 : 0;
}
ELIXIR_BIONIC_FN(int, __isinfl)(__m128i x) noexcept { return ld::Classify(BitsOf(x)) == ld::kFpInfinite ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __isnanl)(__m128i x) noexcept { return ld::Classify(BitsOf(x)) == ld::kFpNan ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __isnormall)(__m128i x) noexcept { return ld::Classify(BitsOf(x)) == ld::kFpNormal ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __signbitl)(__m128i x) noexcept { return ld::Negative(BitsOf(x)) ? 1 : 0; }

ELIXIR_BIONIC_FN(__m128i, fabsl)(__m128i x) noexcept { return FromBits(BitsOf(x) & ~ld::kSignBit); }
ELIXIR_BIONIC_FN(__m128i, copysignl)(__m128i x, __m128i y) noexcept {
  return FromBits((BitsOf(x) & ~ld::kSignBit) | (BitsOf(y) & ld::kSignBit));
}
ELIXIR_BIONIC_FN(__m128i, nanl)(const char*) noexcept { return FromBits(ld::kQuietNan); }

ELIXIR_BIONIC_FN(__m128i, ceill)(__m128i x) noexcept { return FromBits(ld::RoundIntegral(BitsOf(x), ld::Mode::kUp)); }
ELIXIR_BIONIC_FN(__m128i, floorl)(__m128i x) noexcept {
  return FromBits(ld::RoundIntegral(BitsOf(x), ld::Mode::kDown));
}
ELIXIR_BIONIC_FN(__m128i, truncl)(__m128i x) noexcept {
  return FromBits(ld::RoundIntegral(BitsOf(x), ld::Mode::kTowardZero));
}
ELIXIR_BIONIC_FN(__m128i, roundl)(__m128i x) noexcept {
  return FromBits(ld::RoundIntegral(BitsOf(x), ld::Mode::kNearestAway));
}
ELIXIR_BIONIC_FN(__m128i, rintl)(__m128i x) noexcept { return FromBits(ld::RoundIntegral(BitsOf(x), ld::CurrentMode())); }
ELIXIR_BIONIC_FN(__m128i, nearbyintl)(__m128i x) noexcept {
  return FromBits(ld::RoundIntegral(BitsOf(x), ld::CurrentMode()));
}
ELIXIR_BIONIC_FN(abi::g_long, lroundl)(__m128i x) noexcept {
  return ld::ToInt64(ld::RoundIntegral(BitsOf(x), ld::Mode::kNearestAway));
}
ELIXIR_BIONIC_FN(long long, llroundl)(__m128i x) noexcept {
  return ld::ToInt64(ld::RoundIntegral(BitsOf(x), ld::Mode::kNearestAway));
}
ELIXIR_BIONIC_FN(abi::g_long, lrintl)(__m128i x) noexcept {
  return ld::ToInt64(ld::RoundIntegral(BitsOf(x), ld::CurrentMode()));
}
ELIXIR_BIONIC_FN(long long, llrintl)(__m128i x) noexcept {
  return ld::ToInt64(ld::RoundIntegral(BitsOf(x), ld::CurrentMode()));
}

ELIXIR_BIONIC_FN(__m128i, frexpl)(__m128i x, int* exponent_out) noexcept {
  const ld::u128 bits = BitsOf(x);
  const int kind = ld::Classify(bits);
  if (kind == ld::kFpZero || kind == ld::kFpNan || kind == ld::kFpInfinite) {
    if (exponent_out != nullptr) *exponent_out = 0;
    return x;
  }
  int exponent;
  ld::u128 significand;
  ld::Unpack(bits, &exponent, &significand);
  if (exponent_out != nullptr) *exponent_out = exponent + 1;
  return FromBits((bits & ld::kSignBit) | (static_cast<ld::u128>(ld::kBias - 1) << ld::kFractionBits) |
                  (significand & ld::kFractionMask));
}

ELIXIR_BIONIC_FN(__m128i, ldexpl)(__m128i x, int n) noexcept { return FromBits(ld::Scale(BitsOf(x), n)); }
ELIXIR_BIONIC_FN(__m128i, scalbnl)(__m128i x, int n) noexcept { return FromBits(ld::Scale(BitsOf(x), n)); }
ELIXIR_BIONIC_FN(__m128i, scalblnl)(__m128i x, abi::g_long n) noexcept { return FromBits(ld::Scale(BitsOf(x), n)); }

ELIXIR_BIONIC_FN(int, ilogbl)(__m128i x) noexcept {
  const ld::u128 bits = BitsOf(x);
  switch (ld::Classify(bits)) {
    case ld::kFpZero: return -INT_MAX;
    case ld::kFpNan:
    case ld::kFpInfinite: return INT_MAX;
    default: {
      int exponent;
      ld::u128 significand;
      ld::Unpack(bits, &exponent, &significand);
      return exponent;
    }
  }
}

ELIXIR_BIONIC_FN(__m128i, logbl)(__m128i x) noexcept {
  const ld::u128 bits = BitsOf(x);
  switch (ld::Classify(bits)) {
    case ld::kFpZero: return FromBits(ld::kSignBit | ld::kInfinity);
    case ld::kFpNan: return FromBits(ld::Quiet(bits));
    case ld::kFpInfinite: return FromBits(ld::kInfinity);
    default: return FromDouble(static_cast<double>(elixir_bionic_ilogbl(x)));
  }
}

ELIXIR_BIONIC_FN(__m128i, significandl)(__m128i x) noexcept {
  const ld::u128 bits = BitsOf(x);
  const int kind = ld::Classify(bits);
  if (kind == ld::kFpZero || kind == ld::kFpNan || kind == ld::kFpInfinite) return x;
  int exponent;
  ld::u128 significand;
  ld::Unpack(bits, &exponent, &significand);
  return FromBits((bits & ld::kSignBit) | (static_cast<ld::u128>(ld::kBias) << ld::kFractionBits) |
                  (significand & ld::kFractionMask));
}

ELIXIR_BIONIC_FN(__m128i, modfl)(__m128i x, __m128i* integral_out) noexcept {
  const ld::u128 bits = BitsOf(x);
  const ld::u128 sign = bits & ld::kSignBit;
  const int kind = ld::Classify(bits);
  if (kind == ld::kFpNan) {
    if (integral_out != nullptr) *integral_out = FromBits(ld::Quiet(bits));
    return FromBits(ld::Quiet(bits));
  }
  if (kind == ld::kFpInfinite || kind == ld::kFpZero) {
    if (integral_out != nullptr) *integral_out = x;
    return FromBits(sign);
  }
  const ld::u128 integral = ld::RoundIntegral(bits, ld::Mode::kTowardZero);
  if (integral_out != nullptr) *integral_out = FromBits(integral);
  const int exponent = ld::ExponentField(bits) - ld::kBias;
  if (exponent < 0) return x;
  if (exponent >= ld::kFractionBits) return FromBits(sign);
  const int fraction_bits = ld::kFractionBits - exponent;
  const ld::u128 remainder = (ld::Fraction(bits) | ld::kImplicitBit) & ((ld::kOne << fraction_bits) - 1);
  if (remainder == 0) return FromBits(sign);
  const int lead = 127 - ld::LeadingZeros(remainder);
  const int shift = ld::kFractionBits - lead;
  return FromBits(ld::Pack(sign != 0, exponent - shift, remainder << shift));
}

ELIXIR_BIONIC_FN(__m128i, nextafterl)(__m128i x, __m128i y) noexcept {
  const ld::u128 xb = BitsOf(x);
  const ld::u128 yb = BitsOf(y);
  if (ld::Classify(xb) == ld::kFpNan) return FromBits(ld::Quiet(xb));
  if (ld::Classify(yb) == ld::kFpNan) return FromBits(ld::Quiet(yb));
  const __int128 kx = ld::OrderKey(xb);
  const __int128 ky = ld::OrderKey(yb);
  if (kx == ky) return y;
  return FromBits(ld::FromOrderKey(kx < ky ? kx + 1 : kx - 1));
}

ELIXIR_BIONIC_FN(__m128i, fmaxl)(__m128i x, __m128i y) noexcept {
  const ld::u128 xb = BitsOf(x);
  const ld::u128 yb = BitsOf(y);
  if (ld::Classify(xb) == ld::kFpNan) return y;
  if (ld::Classify(yb) == ld::kFpNan) return x;
  const __int128 kx = ld::OrderKey(xb);
  const __int128 ky = ld::OrderKey(yb);
  if (kx == ky) return ld::Negative(xb) ? y : x;
  return kx > ky ? x : y;
}

ELIXIR_BIONIC_FN(__m128i, fminl)(__m128i x, __m128i y) noexcept {
  const ld::u128 xb = BitsOf(x);
  const ld::u128 yb = BitsOf(y);
  if (ld::Classify(xb) == ld::kFpNan) return y;
  if (ld::Classify(yb) == ld::kFpNan) return x;
  const __int128 kx = ld::OrderKey(xb);
  const __int128 ky = ld::OrderKey(yb);
  if (kx == ky) return ld::Negative(xb) ? x : y;
  return kx < ky ? x : y;
}

ELIXIR_BIONIC_FN(__m128i, exp2l)(__m128i x) noexcept { return ld::Exp2Wide(ToDouble(x)); }
ELIXIR_BIONIC_FN(__m128i, expl)(__m128i x) noexcept { return ld::ExpWide(ToDouble(x)); }
ELIXIR_BIONIC_FN(__m128i, exp10l)(__m128i x) noexcept {
  const double value = ToDouble(x);
  const double result = std::pow(10.0, value);
  if (std::isfinite(result) && result != 0 && std::fabs(result) >= 2.2250738585072014e-308) return FromDouble(result);
  return ld::Exp2Wide(value * 3.3219280948873623478703194294894);
}

ELIXIR_BIONIC_FN(__m128i, logl)(__m128i x) noexcept {
  const ld::u128 bits = BitsOf(x);
  const int kind = ld::Classify(bits);
  if (kind == ld::kFpNan || kind == ld::kFpZero || ld::Negative(bits) || kind == ld::kFpInfinite) {
    return FromDouble(std::log(ToDouble(x)));
  }
  return FromDouble(ld::LogWide(bits));
}
ELIXIR_BIONIC_FN(__m128i, log2l)(__m128i x) noexcept {
  const ld::u128 bits = BitsOf(x);
  const int kind = ld::Classify(bits);
  if (kind == ld::kFpNan || kind == ld::kFpZero || ld::Negative(bits) || kind == ld::kFpInfinite) {
    return FromDouble(std::log2(ToDouble(x)));
  }
  double mantissa;
  int exponent;
  ld::SplitDouble(bits, &mantissa, &exponent);
  return FromDouble(std::log2(mantissa) + exponent);
}
ELIXIR_BIONIC_FN(__m128i, log10l)(__m128i x) noexcept {
  const ld::u128 bits = BitsOf(x);
  const int kind = ld::Classify(bits);
  if (kind == ld::kFpNan || kind == ld::kFpZero || ld::Negative(bits) || kind == ld::kFpInfinite) {
    return FromDouble(std::log10(ToDouble(x)));
  }
  return FromDouble(ld::LogWide(bits) * 0.43429448190325182765112891891661);
}
ELIXIR_BIONIC_FN(__m128i, log1pl)(__m128i x) noexcept {
  const double value = ToDouble(x);
  if (std::isinf(value) && value > 0 && ld::Classify(BitsOf(x)) != ld::kFpInfinite) {
    return FromDouble(ld::LogWide(BitsOf(x)));
  }
  return FromDouble(std::log1p(value));
}

ELIXIR_BIONIC_FN(__m128i, powl)(__m128i x, __m128i y) noexcept {
  const double base = ToDouble(x);
  const double exponent = ToDouble(y);
  const double result = std::pow(base, exponent);
  const ld::u128 bits = BitsOf(x);
  const bool wide_base = ld::Classify(bits) == ld::kFpNormal && !ld::Negative(bits);
  if (wide_base && (!std::isfinite(result) || std::fabs(result) < 2.2250738585072014e-308) &&
      std::isfinite(exponent)) {
    double mantissa;
    int e;
    ld::SplitDouble(bits, &mantissa, &e);
    return ld::Exp2Wide(exponent * (std::log2(mantissa) + e));
  }
  return FromDouble(result);
}

ELIXIR_BIONIC_FN(__m128i, sqrtl)(__m128i x) noexcept {
  const ld::u128 bits = BitsOf(x);
  if (ld::Classify(bits) != ld::kFpNormal && ld::Classify(bits) != ld::kFpSubnormal) {
    return FromDouble(std::sqrt(ToDouble(x)));
  }
  if (ld::Negative(bits)) return FromBits(ld::kQuietNan);
  double mantissa;
  int exponent;
  ld::SplitDouble(bits, &mantissa, &exponent);
  if (exponent & 1) {
    mantissa *= 2;
    --exponent;
  }
  return FromBits(ld::Scale(BitsOf(FromDouble(std::sqrt(mantissa))), exponent / 2));
}

ELIXIR_BIONIC_FN(__m128i, cbrtl)(__m128i x) noexcept {
  const ld::u128 bits = BitsOf(x);
  if (ld::Classify(bits) != ld::kFpNormal && ld::Classify(bits) != ld::kFpSubnormal) {
    return FromDouble(std::cbrt(ToDouble(x)));
  }
  double mantissa;
  int exponent;
  ld::SplitDouble(bits, &mantissa, &exponent);
  int remainder = exponent % 3;
  if (remainder < 0) remainder += 3;
  mantissa = std::ldexp(mantissa, remainder);
  exponent -= remainder;
  const double root = std::cbrt(mantissa);
  const ld::u128 scaled = ld::Scale(BitsOf(FromDouble(root)), exponent / 3);
  return FromBits(ld::Negative(bits) ? (scaled | ld::kSignBit) : scaled);
}

ELIXIR_BIONIC_FN(__m128i, coshl)(__m128i x) noexcept {
  const double value = std::fabs(ToDouble(x));
  if (value < 710.0 || std::isnan(value)) return FromDouble(std::cosh(value));
  return ld::Exp2Wide(value * ld::kLog2E - 1.0);
}
ELIXIR_BIONIC_FN(__m128i, sinhl)(__m128i x) noexcept {
  const double value = ToDouble(x);
  if (std::fabs(value) < 710.0 || std::isnan(value)) return FromDouble(std::sinh(value));
  const ld::u128 magnitude = BitsOf(ld::Exp2Wide(std::fabs(value) * ld::kLog2E - 1.0));
  return FromBits(value < 0 ? (magnitude | ld::kSignBit) : magnitude);
}

ELIXIR_BIONIC_FN(__m128i, tgammal)(__m128i x) noexcept {
  const double value = ToDouble(x);
  const double result = std::tgamma(value);
  if (std::isinf(result) && value > 171.0 && value < 1756.0) {
    return ld::Exp2Wide(std::lgamma(value) * ld::kLog2E);
  }
  return FromDouble(result);
}

ELIXIR_BIONIC_FN(__m128i, hypotl)(__m128i x, __m128i y) noexcept {
  const ld::u128 xb = BitsOf(x) & ~ld::kSignBit;
  const ld::u128 yb = BitsOf(y) & ~ld::kSignBit;
  const int xk = ld::Classify(xb);
  const int yk = ld::Classify(yb);
  if (xk == ld::kFpInfinite || yk == ld::kFpInfinite) return FromBits(ld::kInfinity);
  if (xk == ld::kFpNan || yk == ld::kFpNan) return FromBits(ld::kQuietNan);
  const int xe = xk == ld::kFpZero ? INT_MIN : elixir_bionic_ilogbl(FromBits(xb));
  const int ye = yk == ld::kFpZero ? INT_MIN : elixir_bionic_ilogbl(FromBits(yb));
  const int scale = xe > ye ? xe : ye;
  if (scale == INT_MIN) return FromBits(0);
  const double a = ToDouble(FromBits(ld::Scale(xb, -scale)));
  const double b = ToDouble(FromBits(ld::Scale(yb, -scale)));
  return FromBits(ld::Scale(BitsOf(FromDouble(std::hypot(a, b))), scale));
}

ELIXIR_BIONIC_FN(__m128i, lgammal)(__m128i x) noexcept {
  const double value = ToDouble(x);
  int sign = 1;
  const double result = std::lgamma(value);
  if (value < 0 && std::isfinite(value)) {
    const double whole = std::floor(value);
    if (whole != value) sign = static_cast<std::int64_t>(std::fmod(whole, 2.0)) != 0 ? -1 : 1;
  } else if (value == 0 && std::signbit(value)) {
    sign = -1;
  }
  math::g_signgam = sign;
  return FromDouble(result);
}

ELIXIR_BIONIC_FN(__m128i, lgammal_r)(__m128i x, int* sign) noexcept {
  const int saved = math::g_signgam;
  const __m128i result = elixir_bionic_lgammal(x);
  if (sign != nullptr) *sign = math::g_signgam;
  math::g_signgam = saved;
  return result;
}

#define ELIXIR_LD_UNARY(name) \
  ELIXIR_BIONIC_FN(__m128i, name##l)(__m128i x) noexcept { return FromDouble(std::name(ToDouble(x))); }
#define ELIXIR_LD_BINARY(name)                                                    \
  ELIXIR_BIONIC_FN(__m128i, name##l)(__m128i x, __m128i y) noexcept {            \
    return FromDouble(std::name(ToDouble(x), ToDouble(y)));                       \
  }
#define ELIXIR_LD_UNARY_LIST(X)                                                                       \
  X(sin) X(cos) X(tan) X(asin) X(acos) X(atan) X(tanh) X(asinh) X(acosh) X(atanh) X(expm1) X(erf) \
  X(erfc)
#define ELIXIR_LD_BINARY_LIST(X) X(atan2) X(fmod) X(remainder)

ELIXIR_LD_UNARY_LIST(ELIXIR_LD_UNARY)
ELIXIR_LD_BINARY_LIST(ELIXIR_LD_BINARY)

ELIXIR_BIONIC_FN(__m128i, fdiml)(__m128i x, __m128i y) noexcept {
  const ld::u128 xb = BitsOf(x);
  const ld::u128 yb = BitsOf(y);
  if (ld::Classify(xb) == ld::kFpNan) return FromBits(ld::Quiet(xb));
  if (ld::Classify(yb) == ld::kFpNan) return FromBits(ld::Quiet(yb));
  if (ld::OrderKey(xb) <= ld::OrderKey(yb)) return FromBits(0);
  return FromDouble(ToDouble(x) - ToDouble(y));
}

ELIXIR_BIONIC_FN(__m128i, fmal)(__m128i x, __m128i y, __m128i z) noexcept {
  return FromDouble(std::fma(ToDouble(x), ToDouble(y), ToDouble(z)));
}

ELIXIR_BIONIC_FN(__m128i, remquol)(__m128i x, __m128i y, int* quotient) noexcept {
  int ignored = 0;
  return FromDouble(std::remquo(ToDouble(x), ToDouble(y), quotient != nullptr ? quotient : &ignored));
}

ELIXIR_BIONIC_FN(void, sincosl)(__m128i x, __m128i* s, __m128i* c) noexcept {
  const double value = ToDouble(x);
  if (s != nullptr) *s = FromDouble(std::sin(value));
  if (c != nullptr) *c = FromDouble(std::cos(value));
}

namespace core {

void RegisterLongDoubleMathExports(ExportTable& table) {
#define ELIXIR_ADD_M(name) table.Add(kLibm, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
#define ELIXIR_ADD_C(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
#define ELIXIR_ADD_L(name) ELIXIR_ADD_M(name##l);
  ELIXIR_LD_UNARY_LIST(ELIXIR_ADD_L)
  ELIXIR_LD_BINARY_LIST(ELIXIR_ADD_L)
  ELIXIR_ADD_M(fabsl);
  ELIXIR_ADD_M(copysignl);
  ELIXIR_ADD_M(nanl);
  ELIXIR_ADD_M(ceill);
  ELIXIR_ADD_M(floorl);
  ELIXIR_ADD_M(truncl);
  ELIXIR_ADD_M(roundl);
  ELIXIR_ADD_M(rintl);
  ELIXIR_ADD_M(nearbyintl);
  ELIXIR_ADD_M(lroundl);
  ELIXIR_ADD_M(llroundl);
  ELIXIR_ADD_M(lrintl);
  ELIXIR_ADD_M(llrintl);
  ELIXIR_ADD_M(frexpl);
  ELIXIR_ADD_M(ldexpl);
  ELIXIR_ADD_M(scalbnl);
  ELIXIR_ADD_M(scalblnl);
  ELIXIR_ADD_M(ilogbl);
  ELIXIR_ADD_M(logbl);
  ELIXIR_ADD_M(significandl);
  ELIXIR_ADD_M(modfl);
  ELIXIR_ADD_M(nextafterl);
  table.Add(kLibm, "nexttowardl", reinterpret_cast<void*>(&elixir_bionic_nextafterl));
  ELIXIR_ADD_M(fmaxl);
  ELIXIR_ADD_M(fminl);
  ELIXIR_ADD_M(fdiml);
  ELIXIR_ADD_M(fmal);
  ELIXIR_ADD_M(remquol);
  ELIXIR_ADD_M(sincosl);
  ELIXIR_ADD_M(exp2l);
  ELIXIR_ADD_M(expl);
  ELIXIR_ADD_M(exp10l);
  ELIXIR_ADD_M(logl);
  ELIXIR_ADD_M(log2l);
  ELIXIR_ADD_M(log10l);
  ELIXIR_ADD_M(log1pl);
  ELIXIR_ADD_M(powl);
  ELIXIR_ADD_M(sqrtl);
  ELIXIR_ADD_M(cbrtl);
  ELIXIR_ADD_M(coshl);
  ELIXIR_ADD_M(sinhl);
  ELIXIR_ADD_M(tgammal);
  ELIXIR_ADD_M(hypotl);
  ELIXIR_ADD_M(lgammal);
  ELIXIR_ADD_M(lgammal_r);
  ELIXIR_ADD_M(__signbitl);
  ELIXIR_ADD_C(__fpclassifyl);
  ELIXIR_ADD_C(__isfinitel);
  ELIXIR_ADD_C(__isinfl);
  ELIXIR_ADD_C(__isnanl);
  ELIXIR_ADD_C(__isnormall);
  table.Add(kLibc, "isfinitel", reinterpret_cast<void*>(&elixir_bionic___isfinitel));
  table.Add(kLibc, "isinfl", reinterpret_cast<void*>(&elixir_bionic___isinfl));
  table.Add(kLibc, "isnanl", reinterpret_cast<void*>(&elixir_bionic___isnanl));
  table.Add(kLibc, "isnormall", reinterpret_cast<void*>(&elixir_bionic___isnormall));
#undef ELIXIR_ADD_L
#undef ELIXIR_ADD_C
#undef ELIXIR_ADD_M
}

}
}
