// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_ABI_SYSV_ABI_H_
#define ELIXIR_ABI_SYSV_ABI_H_

#include <cstddef>
#include <cstdint>

#if !defined(__clang__)
#error "Elixir's guest ABI layer requires clang (sysv_abi calling convention)"
#endif
#if !defined(__x86_64__) && !defined(_M_X64)
#error "Elixir currently hosts x86_64 guests on x86_64 Windows only"
#endif

#define ELIXIR_SYSV __attribute__((sysv_abi))
#define ELIXIR_NAKED __attribute__((naked, noinline))
#define ELIXIR_ASM_CALLED __attribute__((used))
#define ELIXIR_GUEST_API extern "C" ELIXIR_SYSV

namespace elixir::abi {

using g_long = std::int64_t;
using g_ulong = std::uint64_t;
using g_size = std::uint64_t;
using g_ssize = std::int64_t;
using g_off = std::int64_t;
using g_time = std::int64_t;
using g_suseconds = std::int64_t;
using g_clock = std::int64_t;
using g_clockid = std::int32_t;
using g_pid = std::int32_t;
using g_uid = std::uint32_t;
using g_gid = std::uint32_t;
using g_mode = std::uint32_t;
using g_dev = std::uint64_t;
using g_ino = std::uint64_t;
using g_nlink = std::uint32_t;
using g_socklen = std::uint32_t;
using g_wchar = std::uint32_t;
using g_wint = std::uint32_t;

static_assert(sizeof(void*) == 8, "guest pointers are 64-bit");

struct alignas(16) GuestLongDouble {
  std::uint64_t low;
  std::uint64_t high;
};
static_assert(sizeof(GuestLongDouble) == 16, "binary128 storage");

double LongDoubleToDouble(const GuestLongDouble& value) noexcept;
GuestLongDouble DoubleToLongDouble(double value) noexcept;

template <typename Signature>
struct SysvFunctionPointer;
template <typename R, typename... Args>
struct SysvFunctionPointer<R(Args...)> {
  using type = R(ELIXIR_SYSV*)(Args...);
};
template <typename Signature>
using SysvFn = typename SysvFunctionPointer<Signature>::type;

template <typename Signature, typename... Args>
inline auto CallGuest(void* address, Args... args) {
  return reinterpret_cast<SysvFn<Signature>>(address)(args...);
}

}

#endif
