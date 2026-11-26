// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Upstream code adapted here (references/platform_bionic/libc):
//  * qsort/qsort_r: FreeBSD libc/stdlib/qsort.c (Bentley & McIlroy),
//    Copyright (c) 1992, 1993 The Regents of the University of California.
//  * random/srandom/initstate/setstate: NetBSD common/lib/libc/stdlib/random.c,
//    Copyright (c) 1983, 1993 The Regents of the University of California.
//  * rand48 family and rand_r: NetBSD libc/stdlib (_rand48.c, drand48.c, ...),
//    Copyright (c) 1993 Martin Birgmeier ("You may redistribute unmodified or
//    modified versions of this source code provided that the above copyright
//    notice and this condition are retained.").
//  * lfind/lsearch: OpenBSD libc/stdlib/lsearch.c, Copyright (c) 1989, 1993
//    The Regents of the University of California.
//  * arc4random_uniform: OpenBSD arc4random_uniform.c, Copyright (c) 2008,
//    Damien Miller <djm@openbsd.org> (ISC).
// The Regents' 3-clause BSD license:
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//   3. Neither the name of the University nor the names of its contributors
//      may be used to endorse or promote products derived from this software
//      without specific prior written permission.
//   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
//   ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
#include <windows.h>
#include <bcrypt.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "abi/sysv_abi.h"
#include "bionic/guest_types.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

#pragma comment(lib, "bcrypt.lib")

namespace {

namespace ti = elixir::bionic::text_internal;
namespace guest = elixir::bionic::guest;
using ti::g_long;
using ti::g_size;

using Comparator = elixir::abi::SysvFn<int(const void*, const void*)>;
using ComparatorWithContext = elixir::abi::SysvFn<int(const void*, const void*, void*)>;

void SwapBytes(char* a, char* b, g_size size) noexcept {
  while (size >= 8) {
    std::uint64_t t;
    std::memcpy(&t, a, 8);
    std::memcpy(a, b, 8);
    std::memcpy(b, &t, 8);
    a += 8;
    b += 8;
    size -= 8;
  }
  while (size-- != 0) {
    const char t = *a;
    *a++ = *b;
    *b++ = t;
  }
}

template <typename Compare>
char* Med3(char* a, char* b, char* c, Compare& cmp) noexcept {
  return cmp(a, b) < 0 ? (cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a))
                       : (cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c));
}

template <typename Compare>
void InsertionSort(char* a, g_size n, g_size es, Compare& cmp) noexcept {
  for (char* pm = a + es; pm < a + n * es; pm += es) {
    for (char* pl = pm; pl > a && cmp(pl - es, pl) > 0; pl -= es) SwapBytes(pl, pl - es, es);
  }
}

template <typename Compare>
void LocalQsort(char* a, g_size n, g_size es, Compare& cmp) noexcept {
  while (n >= 2) {
    if (n < 7) {
      InsertionSort(a, n, es, cmp);
      return;
    }
    char* pm = a + (n / 2) * es;
    if (n > 7) {
      char* pl = a;
      char* pn = a + (n - 1) * es;
      if (n > 40) {
        const g_size d = (n / 8) * es;
        pl = Med3(pl, pl + d, pl + 2 * d, cmp);
        pm = Med3(pm - d, pm, pm + d, cmp);
        pn = Med3(pn - 2 * d, pn - d, pn, cmp);
      }
      pm = Med3(pl, pm, pn, cmp);
    }
    SwapBytes(a, pm, es);
    char* pa = a + es;
    char* pb = pa;
    char* pc = a + (n - 1) * es;
    char* pd = pc;
    bool swapped = false;
    for (;;) {
      int r;
      while (pb <= pc && (r = cmp(pb, a)) <= 0) {
        if (r == 0) {
          swapped = true;
          SwapBytes(pa, pb, es);
          pa += es;
        }
        pb += es;
      }
      while (pb <= pc && (r = cmp(pc, a)) >= 0) {
        if (r == 0) {
          swapped = true;
          SwapBytes(pc, pd, es);
          pd -= es;
        }
        pc -= es;
      }
      if (pb > pc) break;
      SwapBytes(pb, pc, es);
      swapped = true;
      pb += es;
      pc -= es;
    }
    if (!swapped) {
      InsertionSort(a, n, es, cmp);
      return;
    }
    char* pn = a + n * es;
    g_size d1 = static_cast<g_size>(pa - a) < static_cast<g_size>(pb - pa) ? static_cast<g_size>(pa - a)
                                                                         : static_cast<g_size>(pb - pa);
    if (d1 > 0) SwapBytes(a, pb - d1, d1);
    const g_size right_gap = static_cast<g_size>(pn - pd) - es;
    d1 = static_cast<g_size>(pd - pc) < right_gap ? static_cast<g_size>(pd - pc) : right_gap;
    if (d1 > 0) SwapBytes(pb, pn - d1, d1);
    d1 = static_cast<g_size>(pb - pa);
    const g_size d2 = static_cast<g_size>(pd - pc);
    if (d1 <= d2) {
      if (d1 > es) LocalQsort(a, d1 / es, es, cmp);
      if (d2 <= es) return;
      a = pn - d2;
      n = d2 / es;
    } else {
      if (d2 > es) LocalQsort(pn - d2, d2 / es, es, cmp);
      if (d1 <= es) return;
      n = d1 / es;
    }
  }
}

struct PlainCompare {
  Comparator fn;
  int operator()(const void* a, const void* b) const noexcept { return fn(a, b); }
};

struct ContextCompare {
  ComparatorWithContext fn;
  void* context;
  int operator()(const void* a, const void* b) const noexcept { return fn(a, b, context); }
};

constexpr int kMaxTypes = 5;
constexpr int kDegrees[kMaxTypes] = {0, 7, 15, 31, 63};
constexpr int kSeps[kMaxTypes] = {0, 3, 1, 3, 1};
constexpr g_size kBreaks[kMaxTypes] = {8, 32, 64, 128, 256};

struct RandomState {
  SRWLOCK lock = SRWLOCK_INIT;
  std::uint32_t default_table[32] = {
      3,
      0x991539b1, 0x16a5bce3, 0x6774a4cd, 0x3e01511e, 0x4e508aaa, 0x61048c05, 0xf5500617, 0x846b7115,
      0x6a19892c, 0x896a97af, 0xdb48f936, 0x14898454, 0x37ffd106, 0xb58bff9c, 0x59e17104, 0xcf918a49,
      0x09378c83, 0x52c7a471, 0x8d293ea9, 0x1f4fc301, 0xc3db71be, 0x39b44e1c, 0xf8a44ef9, 0x4c8b80b1,
      0x19edc328, 0x87bf4bdd, 0xc9b240e5, 0xe9ee4b1b, 0x4382aee7, 0x535b6b41, 0xf3bec5da,
  };
  std::uint32_t* fptr = &default_table[3 + 1];
  std::uint32_t* rptr = &default_table[1];
  std::uint32_t* state = &default_table[1];
  int rand_type = 3;
  int rand_deg = 31;
  int rand_sep = 3;
  std::uint32_t* end_ptr = &default_table[32];
};

RandomState g_random;

std::int32_t RandomUnlocked() noexcept {
  RandomState& r = g_random;
  std::uint32_t i;
  if (r.rand_type == 0) {
    i = r.state[0];
    r.state[0] = i = (i * 1103515245u + 12345u) & 0x7fffffff;
  } else {
    std::uint32_t* f = r.fptr;
    std::uint32_t* b = r.rptr;
    *f += *b;
    i = (*f >> 1) & 0x7fffffff;
    if (++f >= r.end_ptr) {
      f = r.state;
      ++b;
    } else if (++b >= r.end_ptr) {
      b = r.state;
    }
    r.fptr = f;
    r.rptr = b;
  }
  return static_cast<std::int32_t>(i);
}

void SrandomUnlocked(std::uint32_t seed) noexcept {
  RandomState& r = g_random;
  r.state[0] = seed;
  if (r.rand_type == 0) return;
  for (int i = 1; i < r.rand_deg; ++i) {
    const auto x1 = static_cast<std::int32_t>(r.state[i - 1]);
    const std::int32_t hi = x1 / 127773;
    const std::int32_t lo = x1 % 127773;
    std::int32_t t = 16807 * lo - 2836 * hi;
    if (t <= 0) t += 0x7fffffff;
    r.state[i] = static_cast<std::uint32_t>(t);
  }
  r.fptr = &r.state[r.rand_sep];
  r.rptr = &r.state[0];
  for (int i = 0; i < 10 * r.rand_deg; ++i) RandomUnlocked();
}

void SaveRandomPosition() noexcept {
  RandomState& r = g_random;
  if (r.rand_type == 0) {
    r.state[-1] = static_cast<std::uint32_t>(r.rand_type);
  } else {
    r.state[-1] = static_cast<std::uint32_t>(kMaxTypes * static_cast<int>(r.rptr - r.state) + r.rand_type);
  }
}

class RandomLock {
 public:
  RandomLock() noexcept { AcquireSRWLockExclusive(&g_random.lock); }
  ~RandomLock() { ReleaseSRWLockExclusive(&g_random.lock); }
  RandomLock(const RandomLock&) = delete;
  RandomLock& operator=(const RandomLock&) = delete;
};

std::uint16_t g_rand48_seed[3] = {0x330e, 0xabcd, 0x1234};
std::uint16_t g_rand48_mult[3] = {0xe66d, 0xdeec, 0x0005};
std::uint16_t g_rand48_add = 0x000b;
std::uint16_t g_seed48_previous[3];

void Dorand48(std::uint16_t xseed[3]) noexcept {
  std::uint64_t accu = static_cast<std::uint64_t>(g_rand48_mult[0]) * xseed[0] + g_rand48_add;
  const auto temp0 = static_cast<std::uint16_t>(accu);
  accu >>= 16;
  accu += static_cast<std::uint64_t>(g_rand48_mult[0]) * xseed[1] + static_cast<std::uint64_t>(g_rand48_mult[1]) * xseed[0];
  const auto temp1 = static_cast<std::uint16_t>(accu);
  accu >>= 16;
  accu += static_cast<std::uint64_t>(g_rand48_mult[0]) * xseed[2] + static_cast<std::uint64_t>(g_rand48_mult[1]) * xseed[1] +
          static_cast<std::uint64_t>(g_rand48_mult[2]) * xseed[0];
  xseed[0] = temp0;
  xseed[1] = temp1;
  xseed[2] = static_cast<std::uint16_t>(accu);
}

double Erand48(std::uint16_t xseed[3]) noexcept {
  Dorand48(xseed);
  return std::ldexp(static_cast<double>(xseed[0]), -48) + std::ldexp(static_cast<double>(xseed[1]), -32) +
         std::ldexp(static_cast<double>(xseed[2]), -16);
}

bool FillRandom(void* buffer, g_size size) noexcept {
  auto* bytes = static_cast<unsigned char*>(buffer);
  while (size != 0) {
    const ULONG chunk = size > 0x40000000u ? 0x40000000u : static_cast<ULONG>(size);
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, bytes, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) return false;
    bytes += chunk;
    size -= chunk;
  }
  return true;
}

struct RandomPool {
  unsigned char bytes[256];
  std::size_t available = 0;
};
thread_local RandomPool t_random_pool;

void PoolRandom(void* out, std::size_t size) noexcept {
  RandomPool& pool = t_random_pool;
  if (pool.available < size) {
    if (!FillRandom(pool.bytes, sizeof(pool.bytes))) {
      elixir::bionic::HostLog(7, "libc", "arc4random: BCryptGenRandom failed");
      RaiseFailFastException(nullptr, nullptr, 0);
    }
    pool.available = sizeof(pool.bytes);
  }
  pool.available -= size;
  std::memcpy(out, pool.bytes + pool.available, size);
  std::memset(pool.bytes + pool.available, 0, size);
}

}

ELIXIR_BIONIC_FN(int, abs)(int value) noexcept {
  return value < 0 ? static_cast<int>(0u - static_cast<unsigned>(value)) : value;
}
ELIXIR_BIONIC_FN(g_long, labs)(g_long value) noexcept {
  return value < 0 ? static_cast<g_long>(0ull - static_cast<std::uint64_t>(value)) : value;
}
ELIXIR_BIONIC_FN(long long, llabs)(long long value) noexcept {
  return value < 0 ? static_cast<long long>(0ull - static_cast<std::uint64_t>(value)) : value;
}
ELIXIR_BIONIC_FN(std::int64_t, imaxabs)(std::int64_t value) noexcept {
  return value < 0 ? static_cast<std::int64_t>(0ull - static_cast<std::uint64_t>(value)) : value;
}

ELIXIR_BIONIC_FN(guest::DivT, div)(int numerator, int denominator) noexcept {
  return {numerator / denominator, numerator % denominator};
}
ELIXIR_BIONIC_FN(guest::LdivT, ldiv)(g_long numerator, g_long denominator) noexcept {
  return {numerator / denominator, numerator % denominator};
}
ELIXIR_BIONIC_FN(guest::LldivT, lldiv)(long long numerator, long long denominator) noexcept {
  return {numerator / denominator, numerator % denominator};
}
ELIXIR_BIONIC_FN(guest::LldivT, imaxdiv)(std::int64_t numerator, std::int64_t denominator) noexcept {
  return {numerator / denominator, numerator % denominator};
}

ELIXIR_BIONIC_FN(void, qsort)(void* base, g_size count, g_size size, Comparator compare) noexcept {
  if (count < 2 || size == 0) return;
  PlainCompare cmp{compare};
  LocalQsort(static_cast<char*>(base), count, size, cmp);
}

ELIXIR_BIONIC_FN(void, qsort_r)(void* base, g_size count, g_size size, ComparatorWithContext compare,
                                void* context) noexcept {
  if (count < 2 || size == 0) return;
  ContextCompare cmp{compare, context};
  LocalQsort(static_cast<char*>(base), count, size, cmp);
}

ELIXIR_BIONIC_FN(void*, bsearch)(const void* key, const void* base, g_size count, g_size size,
                                 Comparator compare) noexcept {
  const char* lo = static_cast<const char*>(base);
  while (count != 0) {
    const char* middle = lo + (count / 2) * size;
    const int r = compare(key, middle);
    if (r == 0) return const_cast<char*>(middle);
    if (r > 0) {
      lo = middle + size;
      count = count - count / 2 - 1;
    } else {
      count /= 2;
    }
  }
  return nullptr;
}

ELIXIR_BIONIC_FN(void*, lfind)(const void* key, const void* base, g_size* count, g_size size,
                               Comparator compare) noexcept {
  const char* element = static_cast<const char*>(base);
  for (g_size i = 0; i < *count; ++i, element += size) {
    if (compare(key, element) == 0) return const_cast<char*>(element);
  }
  return nullptr;
}

ELIXIR_BIONIC_FN(void*, lsearch)(const void* key, void* base, g_size* count, g_size size,
                                 Comparator compare) noexcept {
  if (void* found = elixir_bionic_lfind(key, base, count, size, compare)) return found;
  char* slot = static_cast<char*>(base) + *count * size;
  std::memcpy(slot, key, size);
  ++*count;
  return slot;
}

ELIXIR_BIONIC_FN(g_long, random)() noexcept {
  RandomLock lock;
  return RandomUnlocked();
}

ELIXIR_BIONIC_FN(void, srandom)(unsigned seed) noexcept {
  RandomLock lock;
  SrandomUnlocked(seed);
}

ELIXIR_BIONIC_FN(int, rand)() noexcept { return static_cast<int>(elixir_bionic_random()); }
ELIXIR_BIONIC_FN(void, srand)(unsigned seed) noexcept { elixir_bionic_srandom(seed); }

ELIXIR_BIONIC_FN(int, rand_r)(unsigned* seed) noexcept {
  *seed = *seed * 1103515245u + 12345u;
  return static_cast<int>(*seed & 0x7fffffff);
}

ELIXIR_BIONIC_FN(char*, initstate)(unsigned seed, char* arg_state, g_size n) noexcept {
  RandomLock lock;
  RandomState& r = g_random;
  char* old_state = reinterpret_cast<char*>(&r.state[-1]);
  SaveRandomPosition();
  if (n < kBreaks[0]) return nullptr;
  int type = kMaxTypes - 1;
  for (int t = 1; t < kMaxTypes; ++t) {
    if (n < kBreaks[t]) {
      type = t - 1;
      break;
    }
  }
  r.rand_type = type;
  r.rand_deg = kDegrees[type];
  r.rand_sep = kSeps[type];
  auto* int_state = reinterpret_cast<std::uint32_t*>(arg_state);
  r.state = int_state + 1;
  r.end_ptr = &r.state[r.rand_deg];
  SrandomUnlocked(seed);
  if (r.rand_type == 0) {
    int_state[0] = static_cast<std::uint32_t>(r.rand_type);
  } else {
    int_state[0] = static_cast<std::uint32_t>(kMaxTypes * static_cast<int>(r.rptr - r.state) + r.rand_type);
  }
  return old_state;
}

ELIXIR_BIONIC_FN(char*, setstate)(char* arg_state) noexcept {
  RandomLock lock;
  RandomState& r = g_random;
  auto* new_state = reinterpret_cast<std::uint32_t*>(arg_state);
  const int type = static_cast<int>(new_state[0] % kMaxTypes);
  const int rear = static_cast<int>(new_state[0] / kMaxTypes);
  char* old_state = reinterpret_cast<char*>(&r.state[-1]);
  SaveRandomPosition();
  if (type < 0 || type >= kMaxTypes) return nullptr;
  r.rand_type = type;
  r.rand_deg = kDegrees[type];
  r.rand_sep = kSeps[type];
  r.state = new_state + 1;
  if (r.rand_type != 0) {
    r.rptr = &r.state[rear];
    r.fptr = &r.state[(rear + r.rand_sep) % r.rand_deg];
  }
  r.end_ptr = &r.state[r.rand_deg];
  return old_state;
}

ELIXIR_BIONIC_FN(double, erand48)(std::uint16_t xseed[3]) noexcept { return Erand48(xseed); }
ELIXIR_BIONIC_FN(double, drand48)() noexcept { return Erand48(g_rand48_seed); }

ELIXIR_BIONIC_FN(g_long, nrand48)(std::uint16_t xseed[3]) noexcept {
  Dorand48(xseed);
  return static_cast<g_long>(xseed[2]) * 32768 + (xseed[1] >> 1);
}
ELIXIR_BIONIC_FN(g_long, lrand48)() noexcept {
  Dorand48(g_rand48_seed);
  return static_cast<g_long>(g_rand48_seed[2]) * 32768 + (g_rand48_seed[1] >> 1);
}
ELIXIR_BIONIC_FN(g_long, jrand48)(std::uint16_t xseed[3]) noexcept {
  Dorand48(xseed);
  return static_cast<g_long>(static_cast<std::int16_t>(xseed[2])) * 65536 + xseed[1];
}
ELIXIR_BIONIC_FN(g_long, mrand48)() noexcept {
  Dorand48(g_rand48_seed);
  return static_cast<g_long>(static_cast<std::int16_t>(g_rand48_seed[2])) * 65536 + g_rand48_seed[1];
}

ELIXIR_BIONIC_FN(void, srand48)(g_long seed) noexcept {
  g_rand48_seed[0] = 0x330e;
  g_rand48_seed[1] = static_cast<std::uint16_t>(seed);
  g_rand48_seed[2] = static_cast<std::uint16_t>(static_cast<std::uint64_t>(seed) >> 16);
  g_rand48_mult[0] = 0xe66d;
  g_rand48_mult[1] = 0xdeec;
  g_rand48_mult[2] = 0x0005;
  g_rand48_add = 0x000b;
}

ELIXIR_BIONIC_FN(std::uint16_t*, seed48)(std::uint16_t xseed[3]) noexcept {
  std::memcpy(g_seed48_previous, g_rand48_seed, sizeof(g_seed48_previous));
  std::memcpy(g_rand48_seed, xseed, sizeof(g_rand48_seed));
  g_rand48_mult[0] = 0xe66d;
  g_rand48_mult[1] = 0xdeec;
  g_rand48_mult[2] = 0x0005;
  g_rand48_add = 0x000b;
  return g_seed48_previous;
}

ELIXIR_BIONIC_FN(void, lcong48)(std::uint16_t p[7]) noexcept {
  std::memcpy(g_rand48_seed, p, 3 * sizeof(std::uint16_t));
  std::memcpy(g_rand48_mult, p + 3, 3 * sizeof(std::uint16_t));
  g_rand48_add = p[6];
}

ELIXIR_BIONIC_FN(std::uint32_t, arc4random)() noexcept {
  std::uint32_t value;
  PoolRandom(&value, sizeof(value));
  return value;
}

ELIXIR_BIONIC_FN(void, arc4random_buf)(void* buffer, g_size size) noexcept {
  if (size <= 64) {
    PoolRandom(buffer, static_cast<std::size_t>(size));
    return;
  }
  if (!FillRandom(buffer, size)) {
    elixir::bionic::HostLog(7, "libc", "arc4random_buf: BCryptGenRandom failed");
    RaiseFailFastException(nullptr, nullptr, 0);
  }
}

ELIXIR_BIONIC_FN(std::uint32_t, arc4random_uniform)(std::uint32_t upper_bound) noexcept {
  if (upper_bound < 2) return 0;
  const std::uint32_t min = (0u - upper_bound) % upper_bound;
  for (;;) {
    const std::uint32_t r = elixir_bionic_arc4random();
    if (r >= min) return r % upper_bound;
  }
}

namespace elixir::bionic::text_internal {

void RegisterStdlibMiscExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, abs);
  ELIXIR_TEXT_EXPORT(table, kLibc, labs);
  ELIXIR_TEXT_EXPORT(table, kLibc, llabs);
  ELIXIR_TEXT_EXPORT(table, kLibc, imaxabs);
  ELIXIR_TEXT_EXPORT(table, kLibc, div);
  ELIXIR_TEXT_EXPORT(table, kLibc, ldiv);
  ELIXIR_TEXT_EXPORT(table, kLibc, lldiv);
  ELIXIR_TEXT_EXPORT(table, kLibc, imaxdiv);
  ELIXIR_TEXT_EXPORT(table, kLibc, qsort);
  ELIXIR_TEXT_EXPORT(table, kLibc, qsort_r);
  ELIXIR_TEXT_EXPORT(table, kLibc, bsearch);
  ELIXIR_TEXT_EXPORT(table, kLibc, lfind);
  ELIXIR_TEXT_EXPORT(table, kLibc, lsearch);
  ELIXIR_TEXT_EXPORT(table, kLibc, random);
  ELIXIR_TEXT_EXPORT(table, kLibc, srandom);
  ELIXIR_TEXT_EXPORT(table, kLibc, rand);
  ELIXIR_TEXT_EXPORT(table, kLibc, srand);
  ELIXIR_TEXT_EXPORT(table, kLibc, rand_r);
  ELIXIR_TEXT_EXPORT(table, kLibc, initstate);
  ELIXIR_TEXT_EXPORT(table, kLibc, setstate);
  ELIXIR_TEXT_EXPORT(table, kLibc, drand48);
  ELIXIR_TEXT_EXPORT(table, kLibc, erand48);
  ELIXIR_TEXT_EXPORT(table, kLibc, lrand48);
  ELIXIR_TEXT_EXPORT(table, kLibc, nrand48);
  ELIXIR_TEXT_EXPORT(table, kLibc, mrand48);
  ELIXIR_TEXT_EXPORT(table, kLibc, jrand48);
  ELIXIR_TEXT_EXPORT(table, kLibc, srand48);
  ELIXIR_TEXT_EXPORT(table, kLibc, seed48);
  ELIXIR_TEXT_EXPORT(table, kLibc, lcong48);
  ELIXIR_TEXT_EXPORT(table, kLibc, arc4random);
  ELIXIR_TEXT_EXPORT(table, kLibc, arc4random_buf);
  ELIXIR_TEXT_EXPORT(table, kLibc, arc4random_uniform);
}

}
