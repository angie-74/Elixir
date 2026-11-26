// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/pthread_cond.cpp:
//
//   Copyright (C) 2008 The Android Open Source Project
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in
//      the documentation and/or other materials provided with the
//      distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
//   OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
//   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
//   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
//   SUCH DAMAGE.
#include <cstddef>
#include <cstdint>

#include "bionic/threads/thread_internal.h"

namespace elixir::bionic::threads::internal {
namespace {

constexpr std::uint32_t kCondSharedMask = 0x0001;
constexpr std::uint32_t kCondClockMask = 0x0002;
constexpr std::uint32_t kCondCounterStep = 0x0004;
constexpr std::uint32_t kCondFlagsMask = kCondSharedMask | kCondClockMask;
constexpr std::uint32_t kCondDestroyed = 0xdeadc04du;
constexpr guest::PthreadCondattr kCondattrDestroyed = 0xdeada11d;

struct CondInternal {
  std::uint32_t state;
  std::uint32_t waiters;
  char reserved[40];
};
static_assert(sizeof(CondInternal) == sizeof(guest::PthreadCond), "pthread_cond_t size");
static_assert(alignof(CondInternal) <= alignof(guest::PthreadCond), "pthread_cond_t alignment");

inline CondInternal* Internal(guest::PthreadCond* cond) noexcept { return reinterpret_cast<CondInternal*>(cond); }

inline bool UseRealtimeClock(CondInternal* cond) noexcept {
  return ((__atomic_load_n(&cond->state, __ATOMIC_RELAXED) & kCondClockMask) >> 1) ==
         static_cast<std::uint32_t>(guest::kCLOCK_REALTIME);
}

int CondPulse(CondInternal* cond, bool all) noexcept {
  if (__atomic_load_n(&cond->waiters, __ATOMIC_RELAXED) == 0) return 0;
  __atomic_fetch_add(&cond->state, kCondCounterStep, __ATOMIC_RELAXED);
  FutexWakeWord(&cond->state, all);
  return 0;
}

int CondTimedWait(CondInternal* cond, guest::PthreadMutex* mutex, bool realtime,
                  const guest::Timespec* deadline) noexcept {
  const int check = CheckTimespec(deadline, true);
  if (check != 0) return check;
  const std::uint32_t old_state = __atomic_load_n(&cond->state, __ATOMIC_RELAXED);
  __atomic_fetch_add(&cond->waiters, 1, __ATOMIC_RELAXED);
  MutexUnlock(mutex);
  const int status = FutexWaitAbsolute(&cond->state, old_state, realtime, deadline);
  __atomic_fetch_sub(&cond->waiters, 1, __ATOMIC_RELAXED);
  MutexLock(mutex);
  return status == guest::kETIMEDOUT ? guest::kETIMEDOUT : 0;
}

}
}

namespace {
namespace internal = elixir::bionic::threads::internal;
namespace guest = elixir::bionic::guest;
}

ELIXIR_BIONIC_FN(int, pthread_condattr_init)(guest::PthreadCondattr* attr) noexcept {
  *attr = 0;
  *attr |= guest::kPTHREAD_PROCESS_PRIVATE;
  *attr |= (guest::kCLOCK_REALTIME << 1);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_condattr_getpshared)(const guest::PthreadCondattr* attr, int* pshared) noexcept {
  *pshared = static_cast<int>((*attr & internal::kCondSharedMask) != 0);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_condattr_setpshared)(guest::PthreadCondattr* attr, int pshared) noexcept {
  if (pshared != guest::kPTHREAD_PROCESS_SHARED && pshared != guest::kPTHREAD_PROCESS_PRIVATE) return guest::kEINVAL;
  *attr |= pshared;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_condattr_getclock)(const guest::PthreadCondattr* attr, elixir::abi::g_clockid* clock) noexcept {
  *clock = static_cast<elixir::abi::g_clockid>((*attr & internal::kCondClockMask) >> 1);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_condattr_setclock)(guest::PthreadCondattr* attr, elixir::abi::g_clockid clock) noexcept {
  if (clock != guest::kCLOCK_MONOTONIC && clock != guest::kCLOCK_REALTIME) return guest::kEINVAL;
  *attr = *attr | (static_cast<guest::PthreadCondattr>(clock) << 1);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_condattr_destroy)(guest::PthreadCondattr* attr) noexcept {
  *attr = internal::kCondattrDestroyed;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_cond_init)(guest::PthreadCond* cond_interface, const guest::PthreadCondattr* attr) noexcept {
  internal::CondInternal* cond = internal::Internal(cond_interface);
  std::uint32_t init_state = 0;
  if (attr != nullptr) init_state = static_cast<std::uint32_t>(*attr) & internal::kCondFlagsMask;
  __atomic_store_n(&cond->state, init_state, __ATOMIC_RELAXED);
  __atomic_store_n(&cond->waiters, 0u, __ATOMIC_RELAXED);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_cond_destroy)(guest::PthreadCond* cond_interface) noexcept {
  __atomic_store_n(&internal::Internal(cond_interface)->state, internal::kCondDestroyed, __ATOMIC_RELAXED);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_cond_broadcast)(guest::PthreadCond* cond) noexcept {
  return internal::CondPulse(internal::Internal(cond), true);
}

ELIXIR_BIONIC_FN(int, pthread_cond_signal)(guest::PthreadCond* cond) noexcept {
  return internal::CondPulse(internal::Internal(cond), false);
}

ELIXIR_BIONIC_FN(int, pthread_cond_wait)(guest::PthreadCond* cond, guest::PthreadMutex* mutex) noexcept {
  return internal::CondTimedWait(internal::Internal(cond), mutex, false, nullptr);
}

ELIXIR_BIONIC_FN(int, pthread_cond_timedwait)(guest::PthreadCond* cond_interface, guest::PthreadMutex* mutex,
                                              const guest::Timespec* deadline) noexcept {
  internal::CondInternal* cond = internal::Internal(cond_interface);
  return internal::CondTimedWait(cond, mutex, internal::UseRealtimeClock(cond), deadline);
}

ELIXIR_BIONIC_FN(int, pthread_cond_timedwait_monotonic_np)(guest::PthreadCond* cond, guest::PthreadMutex* mutex,
                                                           const guest::Timespec* deadline) noexcept {
  return internal::CondTimedWait(internal::Internal(cond), mutex, false, deadline);
}

ELIXIR_BIONIC_FN(int, pthread_cond_timedwait_monotonic)(guest::PthreadCond* cond, guest::PthreadMutex* mutex,
                                                        const guest::Timespec* deadline) noexcept {
  return internal::CondTimedWait(internal::Internal(cond), mutex, false, deadline);
}

ELIXIR_BIONIC_FN(int, pthread_cond_timedwait_relative_np)(guest::PthreadCond* cond, guest::PthreadMutex* mutex,
                                                          const guest::Timespec* relative) noexcept {
  guest::Timespec deadline;
  const guest::Timespec* deadline_ptr = nullptr;
  if (relative != nullptr) {
    const std::int64_t now = internal::MonotonicNowNs();
    deadline.tv_sec = now / 1000000000 + relative->tv_sec;
    deadline.tv_nsec = now % 1000000000 + relative->tv_nsec;
    if (deadline.tv_nsec >= 1000000000) {
      deadline.tv_sec++;
      deadline.tv_nsec -= 1000000000;
    }
    deadline_ptr = &deadline;
  }
  return internal::CondTimedWait(internal::Internal(cond), mutex, false, deadline_ptr);
}

ELIXIR_BIONIC_FN(int, pthread_cond_clockwait)(guest::PthreadCond* cond, guest::PthreadMutex* mutex,
                                              elixir::abi::g_clockid clock, const guest::Timespec* deadline) noexcept {
  switch (clock) {
    case guest::kCLOCK_MONOTONIC: return internal::CondTimedWait(internal::Internal(cond), mutex, false, deadline);
    case guest::kCLOCK_REALTIME: return internal::CondTimedWait(internal::Internal(cond), mutex, true, deadline);
    default: return guest::kEINVAL;
  }
}

namespace elixir::bionic::threads::internal {

void RegisterCondExports(ExportTable& table) {
#define ELIXIR_THREADS_EXPORT(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_THREADS_EXPORT(pthread_condattr_init);
  ELIXIR_THREADS_EXPORT(pthread_condattr_destroy);
  ELIXIR_THREADS_EXPORT(pthread_condattr_getpshared);
  ELIXIR_THREADS_EXPORT(pthread_condattr_setpshared);
  ELIXIR_THREADS_EXPORT(pthread_condattr_getclock);
  ELIXIR_THREADS_EXPORT(pthread_condattr_setclock);
  ELIXIR_THREADS_EXPORT(pthread_cond_init);
  ELIXIR_THREADS_EXPORT(pthread_cond_destroy);
  ELIXIR_THREADS_EXPORT(pthread_cond_broadcast);
  ELIXIR_THREADS_EXPORT(pthread_cond_signal);
  ELIXIR_THREADS_EXPORT(pthread_cond_wait);
  ELIXIR_THREADS_EXPORT(pthread_cond_timedwait);
  ELIXIR_THREADS_EXPORT(pthread_cond_timedwait_monotonic_np);
  ELIXIR_THREADS_EXPORT(pthread_cond_timedwait_monotonic);
  ELIXIR_THREADS_EXPORT(pthread_cond_timedwait_relative_np);
  ELIXIR_THREADS_EXPORT(pthread_cond_clockwait);
#undef ELIXIR_THREADS_EXPORT
}

}
