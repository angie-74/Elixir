// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from bionic's libm/fenv-x86_64.c, which carries this notice:
//
//   $OpenBSD: fenv.c,v 1.3 2012/12/05 23:20:02 deraadt Exp $
//   $NetBSD: fenv.c,v 1.1 2010/07/31 21:47:53 joerg Exp $
//   Copyright (c) 2004-2005 David Schultz <das (at) FreeBSD.ORG>
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
#include <cstdint>

#include "abi/sysv_abi.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"

namespace elixir::bionic {
namespace fenv {

struct GuestFenv {
  struct {
    std::uint32_t control;
    std::uint32_t status;
    std::uint32_t tag;
    std::uint32_t others[4];
  } x87;
  std::uint32_t mxcsr;
};
static_assert(sizeof(GuestFenv) == 32, "x86_64 fenv_t is 32 bytes");
using GuestFexcept = std::uint32_t;

constexpr int kInvalid = 0x01, kDenormal = 0x02, kDivByZero = 0x04, kOverflow = 0x08, kUnderflow = 0x10,
              kInexact = 0x20;
constexpr int kAllExcept = kInvalid | kDenormal | kDivByZero | kOverflow | kUnderflow | kInexact;
constexpr int kRoundMask = 0xc00;
constexpr int kSseMaskShift = 7;
constexpr int kSseRoundShift = 3;

const GuestFenv g_default_env = {
    {
        0xffff0000 | 0x037f,
        0xffff0000,
        0xffffffff,
        {0x00000000, 0x00000000, 0x00000000, 0xffff0000},
    },
    0x1f80,
};

}

using fenv::GuestFenv;
using fenv::GuestFexcept;

ELIXIR_BIONIC_FN(int, fesetexceptflag)(const GuestFexcept* flags, int excepts) noexcept;

ELIXIR_BIONIC_FN(int, feclearexcept)(int excepts) noexcept {
  excepts &= fenv::kAllExcept;
  GuestFenv env;
  __asm__ __volatile__("fnstenv %0" : "=m"(env));
  env.x87.status &= ~static_cast<std::uint32_t>(excepts);
  __asm__ __volatile__("fldenv %0" : : "m"(env));
  std::uint32_t mxcsr;
  __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
  mxcsr &= ~static_cast<std::uint32_t>(excepts);
  __asm__ __volatile__("ldmxcsr %0" : : "m"(mxcsr));
  return 0;
}

ELIXIR_BIONIC_FN(int, fegetexceptflag)(GuestFexcept* flags, int excepts) noexcept {
  excepts &= fenv::kAllExcept;
  std::uint16_t status;
  std::uint32_t mxcsr;
  __asm__ __volatile__("fnstsw %0" : "=am"(status));
  __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
  *flags = (status | mxcsr) & static_cast<std::uint32_t>(excepts);
  return 0;
}

ELIXIR_BIONIC_FN(int, feraiseexcept)(int excepts) noexcept {
  excepts &= fenv::kAllExcept;
  const GuestFexcept flags = static_cast<GuestFexcept>(excepts);
  elixir_bionic_fesetexceptflag(&flags, excepts);
  __asm__ __volatile__("fwait");
  return 0;
}

ELIXIR_BIONIC_FN(int, fesetexceptflag)(const GuestFexcept* flags, int excepts) noexcept {
  excepts &= fenv::kAllExcept;
  GuestFenv env;
  __asm__ __volatile__("fnstenv %0" : "=m"(env));
  env.x87.status &= ~static_cast<std::uint32_t>(excepts);
  env.x87.status |= *flags & static_cast<std::uint32_t>(excepts);
  __asm__ __volatile__("fldenv %0" : : "m"(env));
  std::uint32_t mxcsr;
  __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
  mxcsr &= ~static_cast<std::uint32_t>(excepts);
  mxcsr |= *flags & static_cast<std::uint32_t>(excepts);
  __asm__ __volatile__("ldmxcsr %0" : : "m"(mxcsr));
  return 0;
}

ELIXIR_BIONIC_FN(int, fetestexcept)(int excepts) noexcept {
  excepts &= fenv::kAllExcept;
  std::uint16_t status;
  std::uint32_t mxcsr;
  __asm__ __volatile__("fnstsw %0" : "=am"(status));
  __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
  return static_cast<int>((status | mxcsr) & static_cast<std::uint32_t>(excepts));
}

ELIXIR_BIONIC_FN(int, fegetround)() noexcept {
  std::uint32_t mxcsr;
  __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
  return static_cast<int>((mxcsr >> fenv::kSseRoundShift) & fenv::kRoundMask);
}

ELIXIR_BIONIC_FN(int, fesetround)(int round) noexcept {
  if ((round & ~fenv::kRoundMask) != 0) return -1;
  std::uint16_t control;
  __asm__ __volatile__("fnstcw %0" : "=m"(control));
  control = static_cast<std::uint16_t>((control & ~fenv::kRoundMask) | round);
  __asm__ __volatile__("fldcw %0" : : "m"(control));
  std::uint32_t mxcsr;
  __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
  mxcsr &= ~(static_cast<std::uint32_t>(fenv::kRoundMask) << fenv::kSseRoundShift);
  mxcsr |= static_cast<std::uint32_t>(round) << fenv::kSseRoundShift;
  __asm__ __volatile__("ldmxcsr %0" : : "m"(mxcsr));
  return 0;
}

ELIXIR_BIONIC_FN(int, fegetenv)(GuestFenv* env) noexcept {
  __asm__ __volatile__("fnstenv %0" : "=m"(*env));
  __asm__ __volatile__("stmxcsr %0" : "=m"(env->mxcsr));
  __asm__ __volatile__("fldcw %0" : : "m"(env->x87.control));
  return 0;
}

ELIXIR_BIONIC_FN(int, feholdexcept)(GuestFenv* env) noexcept {
  __asm__ __volatile__("fnstenv %0" : "=m"(*env));
  __asm__ __volatile__("fnclex");
  __asm__ __volatile__("stmxcsr %0" : "=m"(env->mxcsr));
  std::uint32_t mxcsr = env->mxcsr;
  mxcsr &= ~static_cast<std::uint32_t>(fenv::kAllExcept);
  mxcsr |= static_cast<std::uint32_t>(fenv::kAllExcept) << fenv::kSseMaskShift;
  __asm__ __volatile__("ldmxcsr %0" : : "m"(mxcsr));
  return 0;
}

ELIXIR_BIONIC_FN(int, fesetenv)(const GuestFenv* env) noexcept {
  __asm__ __volatile__("fldenv %0" : : "m"(*env));
  __asm__ __volatile__("ldmxcsr %0" : : "m"(env->mxcsr));
  return 0;
}

ELIXIR_BIONIC_FN(int, feupdateenv)(const GuestFenv* env) noexcept {
  std::uint16_t status;
  std::uint32_t mxcsr;
  __asm__ __volatile__("fnstsw %0" : "=am"(status));
  __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
  elixir_bionic_fesetenv(env);
  elixir_bionic_feraiseexcept(static_cast<int>(status | mxcsr));
  return 0;
}

ELIXIR_BIONIC_FN(int, feenableexcept)(int mask) noexcept {
  mask &= fenv::kAllExcept;
  std::uint16_t control;
  std::uint32_t mxcsr;
  __asm__ __volatile__("fnstcw %0" : "=m"(control));
  __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
  const int old_mask = ~(control | (mxcsr >> fenv::kSseMaskShift)) & fenv::kAllExcept;
  control = static_cast<std::uint16_t>(control & ~mask);
  __asm__ __volatile__("fldcw %0" : : "m"(control));
  mxcsr &= ~(static_cast<std::uint32_t>(mask) << fenv::kSseMaskShift);
  __asm__ __volatile__("ldmxcsr %0" : : "m"(mxcsr));
  return old_mask;
}

ELIXIR_BIONIC_FN(int, fedisableexcept)(int mask) noexcept {
  mask &= fenv::kAllExcept;
  std::uint16_t control;
  std::uint32_t mxcsr;
  __asm__ __volatile__("fnstcw %0" : "=m"(control));
  __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
  const int old_mask = ~(control | (mxcsr >> fenv::kSseMaskShift)) & fenv::kAllExcept;
  control = static_cast<std::uint16_t>(control | mask);
  __asm__ __volatile__("fldcw %0" : : "m"(control));
  mxcsr |= static_cast<std::uint32_t>(mask) << fenv::kSseMaskShift;
  __asm__ __volatile__("ldmxcsr %0" : : "m"(mxcsr));
  return old_mask;
}

ELIXIR_BIONIC_FN(int, fegetexcept)() noexcept {
  std::uint16_t control;
  __asm__ __volatile__("fnstcw %0" : "=m"(control));
  return ~control & fenv::kAllExcept;
}

namespace core {

void RegisterFenvExports(ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibm, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(feclearexcept);
  ELIXIR_ADD(fegetexceptflag);
  ELIXIR_ADD(feraiseexcept);
  ELIXIR_ADD(fesetexceptflag);
  ELIXIR_ADD(fetestexcept);
  ELIXIR_ADD(fegetround);
  ELIXIR_ADD(fesetround);
  ELIXIR_ADD(fegetenv);
  ELIXIR_ADD(feholdexcept);
  ELIXIR_ADD(fesetenv);
  ELIXIR_ADD(feupdateenv);
  ELIXIR_ADD(feenableexcept);
  ELIXIR_ADD(fedisableexcept);
  ELIXIR_ADD(fegetexcept);
#undef ELIXIR_ADD
  table.Add(kLibm, "__fe_dfl_env", const_cast<GuestFenv*>(&fenv::g_default_env));
}

}
}
