// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <complex>
#include <cstring>

#include <emmintrin.h>

#include "abi/sysv_abi.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"

namespace elixir::bionic {
namespace cplx {

struct ComplexD {
  double re, im;
};
struct ComplexF {
  float re, im;
};
struct ComplexL {
  abi::GuestLongDouble re, im;
};
static_assert(sizeof(ComplexD) == 16 && sizeof(ComplexF) == 8 && sizeof(ComplexL) == 32, "complex layouts");

inline std::complex<double> Std(ComplexD z) noexcept { return {z.re, z.im}; }
inline std::complex<float> Std(ComplexF z) noexcept { return {z.re, z.im}; }
inline std::complex<double> Std(const ComplexL& z) noexcept {
  return {abi::LongDoubleToDouble(z.re), abi::LongDoubleToDouble(z.im)};
}
inline ComplexD ToD(std::complex<double> z) noexcept { return {z.real(), z.imag()}; }
inline ComplexF ToF(std::complex<float> z) noexcept { return {z.real(), z.imag()}; }
inline ComplexL ToL(std::complex<double> z) noexcept {
  return {abi::DoubleToLongDouble(z.real()), abi::DoubleToLongDouble(z.imag())};
}
inline __m128i Ld(double value) noexcept {
  const abi::GuestLongDouble ld = abi::DoubleToLongDouble(value);
  __m128i out;
  std::memcpy(&out, &ld, sizeof(out));
  return out;
}
inline __m128i LdBits(const abi::GuestLongDouble& value) noexcept {
  __m128i out;
  std::memcpy(&out, &value, sizeof(out));
  return out;
}

}

using cplx::ComplexD;
using cplx::ComplexF;
using cplx::ComplexL;

#define ELIXIR_COMPLEX_UNARY(cname, op)                                                                      \
  ELIXIR_BIONIC_FN(ComplexD, cname)(ComplexD z) noexcept { return cplx::ToD(std::op(cplx::Std(z))); }      \
  ELIXIR_BIONIC_FN(ComplexF, cname##f)(ComplexF z) noexcept { return cplx::ToF(std::op(cplx::Std(z))); }   \
  ELIXIR_BIONIC_FN(ComplexL, cname##l)(ComplexL z) noexcept { return cplx::ToL(std::op(cplx::Std(z))); }

#define ELIXIR_COMPLEX_LIST(X)                                                                                \
  X(cacos, acos) X(cacosh, acosh) X(casin, asin) X(casinh, asinh) X(catan, atan) X(catanh, atanh)          \
  X(ccos, cos) X(ccosh, cosh) X(cexp, exp) X(clog, log) X(csin, sin) X(csinh, sinh) X(csqrt, sqrt)         \
  X(ctan, tan) X(ctanh, tanh) X(cproj, proj) X(conj, conj)

ELIXIR_COMPLEX_LIST(ELIXIR_COMPLEX_UNARY)

ELIXIR_BIONIC_FN(ComplexD, cpow)(ComplexD x, ComplexD y) noexcept {
  return cplx::ToD(std::pow(cplx::Std(x), cplx::Std(y)));
}
ELIXIR_BIONIC_FN(ComplexF, cpowf)(ComplexF x, ComplexF y) noexcept {
  return cplx::ToF(std::pow(cplx::Std(x), cplx::Std(y)));
}
ELIXIR_BIONIC_FN(ComplexL, cpowl)(ComplexL x, ComplexL y) noexcept {
  return cplx::ToL(std::pow(cplx::Std(x), cplx::Std(y)));
}

ELIXIR_BIONIC_FN(double, cabs)(ComplexD z) noexcept { return std::abs(cplx::Std(z)); }
ELIXIR_BIONIC_FN(float, cabsf)(ComplexF z) noexcept { return std::abs(cplx::Std(z)); }
ELIXIR_BIONIC_FN(__m128i, cabsl)(ComplexL z) noexcept { return cplx::Ld(std::abs(cplx::Std(z))); }
ELIXIR_BIONIC_FN(double, carg)(ComplexD z) noexcept { return std::arg(cplx::Std(z)); }
ELIXIR_BIONIC_FN(float, cargf)(ComplexF z) noexcept { return std::arg(cplx::Std(z)); }
ELIXIR_BIONIC_FN(__m128i, cargl)(ComplexL z) noexcept { return cplx::Ld(std::arg(cplx::Std(z))); }
ELIXIR_BIONIC_FN(double, creal)(ComplexD z) noexcept { return z.re; }
ELIXIR_BIONIC_FN(float, crealf)(ComplexF z) noexcept { return z.re; }
ELIXIR_BIONIC_FN(__m128i, creall)(ComplexL z) noexcept { return cplx::LdBits(z.re); }
ELIXIR_BIONIC_FN(double, cimag)(ComplexD z) noexcept { return z.im; }
ELIXIR_BIONIC_FN(float, cimagf)(ComplexF z) noexcept { return z.im; }
ELIXIR_BIONIC_FN(__m128i, cimagl)(ComplexL z) noexcept { return cplx::LdBits(z.im); }

namespace core {

void RegisterComplexExports(ExportTable& table) {
#define ELIXIR_ADD_M(name) table.Add(kLibm, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
#define ELIXIR_ADD_ALL(cname, op) ELIXIR_ADD_M(cname); ELIXIR_ADD_M(cname##f); ELIXIR_ADD_M(cname##l);
  ELIXIR_COMPLEX_LIST(ELIXIR_ADD_ALL)
  ELIXIR_ADD_ALL(cpow, pow)
  ELIXIR_ADD_ALL(cabs, abs)
  ELIXIR_ADD_ALL(carg, arg)
  ELIXIR_ADD_ALL(creal, real)
  ELIXIR_ADD_ALL(cimag, imag)
#undef ELIXIR_ADD_ALL
#undef ELIXIR_ADD_M
}

}
}
