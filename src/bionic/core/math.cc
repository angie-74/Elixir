// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <emmintrin.h>

#include <cfloat>
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
namespace {

constexpr int kFpInfinite = 0x01, kFpNan = 0x02, kFpNormal = 0x04, kFpSubnormal = 0x08, kFpZero = 0x10;
constexpr int kIlogb0 = -INT_MAX, kIlogbNan = INT_MAX;

std::uint64_t Bits(double x) noexcept {
  std::uint64_t bits;
  std::memcpy(&bits, &x, sizeof(bits));
  return bits;
}
std::uint32_t Bits(float x) noexcept {
  std::uint32_t bits;
  std::memcpy(&bits, &x, sizeof(bits));
  return bits;
}

int Classify(double x) noexcept {
  const std::uint64_t bits = Bits(x);
  const std::uint64_t exponent = (bits >> 52) & 0x7ff;
  const std::uint64_t mantissa = bits & 0x000fffffffffffffull;
  if (exponent == 0x7ff) return mantissa != 0 ? kFpNan : kFpInfinite;
  if (exponent == 0) return mantissa != 0 ? kFpSubnormal : kFpZero;
  return kFpNormal;
}
int Classify(float x) noexcept {
  const std::uint32_t bits = Bits(x);
  const std::uint32_t exponent = (bits >> 23) & 0xff;
  const std::uint32_t mantissa = bits & 0x7fffff;
  if (exponent == 0xff) return mantissa != 0 ? kFpNan : kFpInfinite;
  if (exponent == 0) return mantissa != 0 ? kFpSubnormal : kFpZero;
  return kFpNormal;
}

template <typename T>
int Ilogb(T x) noexcept {
  switch (Classify(x)) {
    case kFpZero: return kIlogb0;
    case kFpNan:
    case kFpInfinite: return kIlogbNan;
    default: {
      int exponent = 0;
      std::frexp(x, &exponent);
      return exponent - 1;
    }
  }
}

int ClampExponent(abi::g_long n) noexcept {
  if (n > 100000) return 100000;
  if (n < -100000) return -100000;
  return static_cast<int>(n);
}

template <typename T>
T Lgamma(T x, int* sign) noexcept {
  int s = 1;
  if (x < 0 && std::isfinite(x)) {
    const T whole = std::floor(x);
    if (whole != x) s = (static_cast<std::int64_t>(std::fmod(whole, T(2))) != 0) ? -1 : 1;
  } else if (x == 0 && std::signbit(x)) {
    s = -1;
  }
  *sign = s;
  return std::lgamma(x);
}

}

int g_signgam = 0;

}

using math::Classify;

#define ELIXIR_MATH_UNARY(name)                                                              \
  ELIXIR_BIONIC_FN(double, name)(double x) noexcept { return std::name(x); }                \
  ELIXIR_BIONIC_FN(float, name##f)(float x) noexcept { return std::name(x); }
#define ELIXIR_MATH_BINARY(name)                                                             \
  ELIXIR_BIONIC_FN(double, name)(double x, double y) noexcept { return std::name(x, y); }   \
  ELIXIR_BIONIC_FN(float, name##f)(float x, float y) noexcept { return std::name(x, y); }

#define ELIXIR_MATH_UNARY_LIST(X)                                                             \
  X(sin) X(cos) X(tan) X(asin) X(acos) X(atan) X(sinh) X(cosh) X(tanh) X(asinh) X(acosh)     \
  X(atanh) X(exp) X(exp2) X(expm1) X(log) X(log2) X(log10) X(log1p) X(logb) X(sqrt) X(cbrt) \
  X(fabs) X(ceil) X(floor) X(trunc) X(round) X(rint) X(nearbyint) X(erf) X(erfc) X(tgamma)
#define ELIXIR_MATH_BINARY_LIST(X) \
  X(atan2) X(pow) X(fmod) X(remainder) X(hypot) X(copysign) X(fmax) X(fmin) X(fdim) X(nextafter)

ELIXIR_MATH_UNARY_LIST(ELIXIR_MATH_UNARY)
ELIXIR_MATH_BINARY_LIST(ELIXIR_MATH_BINARY)

ELIXIR_BIONIC_FN(double, fma)(double x, double y, double z) noexcept { return std::fma(x, y, z); }
ELIXIR_BIONIC_FN(float, fmaf)(float x, float y, float z) noexcept { return std::fma(x, y, z); }

ELIXIR_BIONIC_FN(double, remquo)(double x, double y, int* quo) noexcept { return std::remquo(x, y, quo); }
ELIXIR_BIONIC_FN(float, remquof)(float x, float y, int* quo) noexcept { return std::remquo(x, y, quo); }

ELIXIR_BIONIC_FN(double, modf)(double x, double* integral) noexcept { return std::modf(x, integral); }
ELIXIR_BIONIC_FN(float, modff)(float x, float* integral) noexcept { return std::modf(x, integral); }

ELIXIR_BIONIC_FN(double, frexp)(double x, int* exponent) noexcept { return std::frexp(x, exponent); }
ELIXIR_BIONIC_FN(float, frexpf)(float x, int* exponent) noexcept { return std::frexp(x, exponent); }

ELIXIR_BIONIC_FN(double, ldexp)(double x, int exponent) noexcept { return std::ldexp(x, exponent); }
ELIXIR_BIONIC_FN(float, ldexpf)(float x, int exponent) noexcept { return std::ldexp(x, exponent); }
ELIXIR_BIONIC_FN(double, scalbn)(double x, int exponent) noexcept { return std::scalbn(x, exponent); }
ELIXIR_BIONIC_FN(float, scalbnf)(float x, int exponent) noexcept { return std::scalbn(x, exponent); }
ELIXIR_BIONIC_FN(double, scalbln)(double x, abi::g_long exponent) noexcept {
  return std::scalbn(x, math::ClampExponent(exponent));
}
ELIXIR_BIONIC_FN(float, scalblnf)(float x, abi::g_long exponent) noexcept {
  return std::scalbn(x, math::ClampExponent(exponent));
}

ELIXIR_BIONIC_FN(double, scalb)(double x, double n) noexcept {
  if (std::isnan(x) || std::isnan(n)) return x * n;
  if (std::isinf(n)) return n > 0 ? x * n : x / -n;
  if (std::rint(n) != n) return (n - n) / (n - n);
  return std::scalbn(x, n > 100000 ? 100000 : (n < -100000 ? -100000 : static_cast<int>(n)));
}
ELIXIR_BIONIC_FN(float, scalbf)(float x, float n) noexcept {
  return static_cast<float>(elixir_bionic_scalb(static_cast<double>(x), static_cast<double>(n)));
}

ELIXIR_BIONIC_FN(int, ilogb)(double x) noexcept { return math::Ilogb(x); }
ELIXIR_BIONIC_FN(int, ilogbf)(float x) noexcept { return math::Ilogb(x); }

ELIXIR_BIONIC_FN(abi::g_long, lround)(double x) noexcept { return std::llround(x); }
ELIXIR_BIONIC_FN(abi::g_long, lroundf)(float x) noexcept { return std::llround(x); }
ELIXIR_BIONIC_FN(long long, llround)(double x) noexcept { return std::llround(x); }
ELIXIR_BIONIC_FN(long long, llroundf)(float x) noexcept { return std::llround(x); }
ELIXIR_BIONIC_FN(abi::g_long, lrint)(double x) noexcept { return std::llrint(x); }
ELIXIR_BIONIC_FN(abi::g_long, lrintf)(float x) noexcept { return std::llrint(x); }
ELIXIR_BIONIC_FN(long long, llrint)(double x) noexcept { return std::llrint(x); }
ELIXIR_BIONIC_FN(long long, llrintf)(float x) noexcept { return std::llrint(x); }

ELIXIR_BIONIC_FN(double, nan)(const char*) noexcept { return std::numeric_limits<double>::quiet_NaN(); }
ELIXIR_BIONIC_FN(float, nanf)(const char*) noexcept { return std::numeric_limits<float>::quiet_NaN(); }

ELIXIR_BIONIC_FN(double, exp10)(double x) noexcept { return std::pow(10.0, x); }
ELIXIR_BIONIC_FN(float, exp10f)(float x) noexcept { return static_cast<float>(std::pow(10.0, static_cast<double>(x))); }

ELIXIR_BIONIC_FN(double, lgamma)(double x) noexcept { return math::Lgamma(x, &math::g_signgam); }
ELIXIR_BIONIC_FN(float, lgammaf)(float x) noexcept { return math::Lgamma(x, &math::g_signgam); }
ELIXIR_BIONIC_FN(double, lgamma_r)(double x, int* sign) noexcept {
  int ignored;
  return math::Lgamma(x, sign != nullptr ? sign : &ignored);
}
ELIXIR_BIONIC_FN(float, lgammaf_r)(float x, int* sign) noexcept {
  int ignored;
  return math::Lgamma(x, sign != nullptr ? sign : &ignored);
}
ELIXIR_BIONIC_FN(double, gamma)(double x) noexcept { return elixir_bionic_lgamma(x); }
ELIXIR_BIONIC_FN(float, gammaf)(float x) noexcept { return elixir_bionic_lgammaf(x); }
ELIXIR_BIONIC_FN(double, gamma_r)(double x, int* sign) noexcept { return elixir_bionic_lgamma_r(x, sign); }
ELIXIR_BIONIC_FN(float, gammaf_r)(float x, int* sign) noexcept { return elixir_bionic_lgammaf_r(x, sign); }

ELIXIR_BIONIC_FN(void, sincos)(double x, double* s, double* c) noexcept {
  *s = std::sin(x);
  *c = std::cos(x);
}
ELIXIR_BIONIC_FN(void, sincosf)(float x, float* s, float* c) noexcept {
  *s = std::sin(x);
  *c = std::cos(x);
}

ELIXIR_BIONIC_FN(double, significand)(double x) noexcept {
  const int kind = Classify(x);
  if (kind == math::kFpZero || kind == math::kFpNan || kind == math::kFpInfinite) return x;
  return std::scalbn(x, -math::Ilogb(x));
}
ELIXIR_BIONIC_FN(float, significandf)(float x) noexcept {
  const int kind = Classify(x);
  if (kind == math::kFpZero || kind == math::kFpNan || kind == math::kFpInfinite) return x;
  return std::scalbn(x, -math::Ilogb(x));
}

ELIXIR_BIONIC_FN(double, drem)(double x, double y) noexcept { return std::remainder(x, y); }
ELIXIR_BIONIC_FN(float, dremf)(float x, float y) noexcept { return std::remainder(x, y); }

ELIXIR_BIONIC_FN(int, finite)(double x) noexcept { return std::isfinite(x) ? 1 : 0; }
ELIXIR_BIONIC_FN(int, finitef)(float x) noexcept { return std::isfinite(x) ? 1 : 0; }

ELIXIR_BIONIC_FN(double, j0)(double x) noexcept { return _j0(x); }
ELIXIR_BIONIC_FN(double, j1)(double x) noexcept { return _j1(x); }
ELIXIR_BIONIC_FN(double, jn)(int n, double x) noexcept { return _jn(n, x); }
ELIXIR_BIONIC_FN(double, y0)(double x) noexcept {
  if (x < 0) return std::numeric_limits<double>::quiet_NaN();
  if (x == 0) return -HUGE_VAL;
  return _y0(x);
}
ELIXIR_BIONIC_FN(double, y1)(double x) noexcept {
  if (x < 0) return std::numeric_limits<double>::quiet_NaN();
  if (x == 0) return -HUGE_VAL;
  return _y1(x);
}
ELIXIR_BIONIC_FN(double, yn)(int n, double x) noexcept {
  if (x < 0) return std::numeric_limits<double>::quiet_NaN();
  if (x == 0) return -HUGE_VAL;
  return _yn(n, x);
}
ELIXIR_BIONIC_FN(float, j0f)(float x) noexcept { return static_cast<float>(_j0(x)); }
ELIXIR_BIONIC_FN(float, j1f)(float x) noexcept { return static_cast<float>(_j1(x)); }
ELIXIR_BIONIC_FN(float, jnf)(int n, float x) noexcept { return static_cast<float>(_jn(n, x)); }
ELIXIR_BIONIC_FN(float, y0f)(float x) noexcept { return static_cast<float>(elixir_bionic_y0(x)); }
ELIXIR_BIONIC_FN(float, y1f)(float x) noexcept { return static_cast<float>(elixir_bionic_y1(x)); }
ELIXIR_BIONIC_FN(float, ynf)(int n, float x) noexcept { return static_cast<float>(elixir_bionic_yn(n, x)); }

ELIXIR_BIONIC_FN(int, __fpclassifyd)(double x) noexcept { return Classify(x); }
ELIXIR_BIONIC_FN(int, __fpclassifyf)(float x) noexcept { return Classify(x); }
ELIXIR_BIONIC_FN(int, __isfinite)(double x) noexcept { return std::isfinite(x) ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __isfinitef)(float x) noexcept { return std::isfinite(x) ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __isinf)(double x) noexcept { return Classify(x) == math::kFpInfinite ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __isinff)(float x) noexcept { return Classify(x) == math::kFpInfinite ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __isnan)(double x) noexcept { return Classify(x) == math::kFpNan ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __isnanf)(float x) noexcept { return Classify(x) == math::kFpNan ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __isnormal)(double x) noexcept { return Classify(x) == math::kFpNormal ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __isnormalf)(float x) noexcept { return Classify(x) == math::kFpNormal ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __signbit)(double x) noexcept { return std::signbit(x) ? 1 : 0; }
ELIXIR_BIONIC_FN(int, __signbitf)(float x) noexcept { return std::signbit(x) ? 1 : 0; }

namespace math {
namespace {
__int128 OrderKey(const abi::GuestLongDouble& value) noexcept {
  const unsigned __int128 bits = (static_cast<unsigned __int128>(value.high) << 64) | value.low;
  const unsigned __int128 magnitude = bits & ~(static_cast<unsigned __int128>(1) << 127);
  return (value.high >> 63) != 0 ? -static_cast<__int128>(magnitude) : static_cast<__int128>(magnitude);
}
bool IsNanLd(const abi::GuestLongDouble& value) noexcept {
  return ((value.high >> 48) & 0x7fff) == 0x7fff && ((value.high & 0xffffffffffffull) | value.low) != 0;
}
abi::GuestLongDouble FromVector(__m128i v) noexcept {
  abi::GuestLongDouble value;
  std::memcpy(&value, &v, sizeof(value));
  return value;
}
}
}

ELIXIR_BIONIC_FN(double, nexttoward)(double x, __m128i y_bits) noexcept {
  const abi::GuestLongDouble y = math::FromVector(y_bits);
  if (std::isnan(x)) return x;
  if (math::IsNanLd(y)) return abi::LongDoubleToDouble(y);
  const __int128 kx = math::OrderKey(abi::DoubleToLongDouble(x));
  const __int128 ky = math::OrderKey(y);
  if (kx == ky) return abi::LongDoubleToDouble(y);
  return std::nextafter(x, kx < ky ? HUGE_VAL : -HUGE_VAL);
}
ELIXIR_BIONIC_FN(float, nexttowardf)(float x, __m128i y_bits) noexcept {
  const abi::GuestLongDouble y = math::FromVector(y_bits);
  if (std::isnan(x)) return x;
  if (math::IsNanLd(y)) return static_cast<float>(abi::LongDoubleToDouble(y));
  const __int128 kx = math::OrderKey(abi::DoubleToLongDouble(x));
  const __int128 ky = math::OrderKey(y);
  if (kx == ky) return static_cast<float>(abi::LongDoubleToDouble(y));
  return std::nextafter(x, kx < ky ? HUGE_VALF : -HUGE_VALF);
}

void RegisterMathExports(ExportTable& table) {
#define ELIXIR_ADD_M(name) table.Add(kLibm, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
#define ELIXIR_ADD_C(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
#define ELIXIR_ADD_PAIR(name) ELIXIR_ADD_M(name); ELIXIR_ADD_M(name##f);
  ELIXIR_MATH_UNARY_LIST(ELIXIR_ADD_PAIR)
  ELIXIR_MATH_BINARY_LIST(ELIXIR_ADD_PAIR)
  ELIXIR_ADD_PAIR(fma)
  ELIXIR_ADD_PAIR(remquo)
  ELIXIR_ADD_PAIR(lround)
  ELIXIR_ADD_PAIR(llround)
  ELIXIR_ADD_PAIR(lrint)
  ELIXIR_ADD_PAIR(llrint)
  ELIXIR_ADD_PAIR(nan)
  ELIXIR_ADD_PAIR(exp10)
  ELIXIR_ADD_PAIR(lgamma)
  ELIXIR_ADD_PAIR(gamma)
  ELIXIR_ADD_PAIR(sincos)
  ELIXIR_ADD_PAIR(significand)
  ELIXIR_ADD_PAIR(drem)
  ELIXIR_ADD_PAIR(finite)
  ELIXIR_ADD_PAIR(scalbn)
  ELIXIR_ADD_PAIR(scalbln)
  ELIXIR_ADD_PAIR(scalb)
  ELIXIR_ADD_PAIR(ilogb)
  ELIXIR_ADD_PAIR(modf)
  ELIXIR_ADD_PAIR(frexp)
  ELIXIR_ADD_PAIR(j0)
  ELIXIR_ADD_PAIR(j1)
  ELIXIR_ADD_PAIR(jn)
  ELIXIR_ADD_PAIR(y0)
  ELIXIR_ADD_PAIR(y1)
  ELIXIR_ADD_PAIR(yn)
  ELIXIR_ADD_PAIR(nexttoward)
  ELIXIR_ADD_M(ldexpf);
  ELIXIR_ADD_M(lgamma_r);
  ELIXIR_ADD_M(lgammaf_r);
  ELIXIR_ADD_M(gamma_r);
  ELIXIR_ADD_M(gammaf_r);
  ELIXIR_ADD_M(__signbit);
  ELIXIR_ADD_M(__signbitf);
  table.Add(kLibm, "signgam", &math::g_signgam);
  ELIXIR_ADD_C(ldexp);
  table.Add(kLibc, "__fpclassify", reinterpret_cast<void*>(&elixir_bionic___fpclassifyd));
  ELIXIR_ADD_C(__fpclassifyd);
  ELIXIR_ADD_C(__fpclassifyf);
  ELIXIR_ADD_C(__isfinite);
  ELIXIR_ADD_C(__isfinitef);
  ELIXIR_ADD_C(__isinf);
  ELIXIR_ADD_C(__isinff);
  ELIXIR_ADD_C(__isnan);
  ELIXIR_ADD_C(__isnanf);
  ELIXIR_ADD_C(__isnormal);
  ELIXIR_ADD_C(__isnormalf);
  table.Add(kLibc, "isfinite", reinterpret_cast<void*>(&elixir_bionic___isfinite));
  table.Add(kLibc, "isfinitef", reinterpret_cast<void*>(&elixir_bionic___isfinitef));
  table.Add(kLibc, "isinf", reinterpret_cast<void*>(&elixir_bionic___isinf));
  table.Add(kLibc, "isinff", reinterpret_cast<void*>(&elixir_bionic___isinff));
  table.Add(kLibc, "isnan", reinterpret_cast<void*>(&elixir_bionic___isnan));
  table.Add(kLibc, "isnanf", reinterpret_cast<void*>(&elixir_bionic___isnanf));
  table.Add(kLibc, "isnormal", reinterpret_cast<void*>(&elixir_bionic___isnormal));
  table.Add(kLibc, "isnormalf", reinterpret_cast<void*>(&elixir_bionic___isnormalf));
#undef ELIXIR_ADD_PAIR
#undef ELIXIR_ADD_C
#undef ELIXIR_ADD_M
  core::RegisterLongDoubleMathExports(table);
  core::RegisterFenvExports(table);
  core::RegisterComplexExports(table);
}

}
