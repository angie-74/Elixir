// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/pthread_once.cpp, semaphore.cpp
// (Copyright (C) 2008), pthread_barrier.cpp and pthread_spinlock.cpp
// (Copyright (C) 2015), The Android Open Source Project:
//
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

constexpr std::int32_t kOnceNotYetStarted = 0;
constexpr std::int32_t kOnceUnderway = 1;
constexpr std::int32_t kOnceComplete = 2;

enum BarrierState : std::int32_t { kBarrierWait = 0, kBarrierRelease = 1 };

struct BarrierInternal {
  std::uint32_t init_count;
  std::int32_t state;
  std::uint32_t wait_count;
  bool pshared;
  std::uint32_t reserved[4];
};
static_assert(sizeof(BarrierInternal) == sizeof(guest::PthreadBarrier), "pthread_barrier_t size");
static_assert(alignof(BarrierInternal) <= alignof(guest::PthreadBarrier), "pthread_barrier_t alignment");

inline BarrierInternal* Internal(guest::PthreadBarrier* barrier) noexcept {
  return reinterpret_cast<BarrierInternal*>(barrier);
}

static_assert(sizeof(GuestLock) == sizeof(guest::PthreadSpinlock), "pthread_spinlock_t holds a bionic Lock");
inline GuestLock* Internal(guest::PthreadSpinlock* lock) noexcept { return reinterpret_cast<GuestLock*>(lock); }

constexpr std::uint32_t kSemcountSharedMask = 0x00000001;
constexpr std::uint32_t kSemcountValueMask = 0xfffffffe;
constexpr int kSemcountValueShift = 1;

constexpr std::uint32_t SemcountFromValue(std::uint32_t value) noexcept {
  return (value << kSemcountValueShift) & kSemcountValueMask;
}
inline int SemcountToValue(std::uint32_t sval) noexcept { return static_cast<std::int32_t>(sval) >> kSemcountValueShift; }
constexpr std::uint32_t kSemcountOne = SemcountFromValue(1);
constexpr std::uint32_t kSemcountMinusOne = SemcountFromValue(~0u);
constexpr std::uint32_t SemcountDecrement(std::uint32_t sval) noexcept {
  return (sval - (1u << kSemcountValueShift)) & kSemcountValueMask;
}
constexpr std::uint32_t SemcountIncrement(std::uint32_t sval) noexcept {
  return (sval + (1u << kSemcountValueShift)) & kSemcountValueMask;
}

inline std::uint32_t* SemCount(guest::Sem* sem) noexcept { return &sem->count; }
inline std::uint32_t SemGetShared(std::uint32_t* count) noexcept {
  return __atomic_load_n(count, __ATOMIC_RELAXED) & kSemcountSharedMask;
}

int SemDec(std::uint32_t* count) noexcept {
  std::uint32_t old_value = __atomic_load_n(count, __ATOMIC_RELAXED);
  const std::uint32_t shared = old_value & kSemcountSharedMask;
  do {
    if (SemcountToValue(old_value) < 0) break;
  } while (!__atomic_compare_exchange_n(count, &old_value, SemcountDecrement(old_value) | shared, true,
                                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  return SemcountToValue(old_value);
}

int SemTryDec(std::uint32_t* count) noexcept {
  std::uint32_t old_value = __atomic_load_n(count, __ATOMIC_RELAXED);
  const std::uint32_t shared = old_value & kSemcountSharedMask;
  do {
    if (SemcountToValue(old_value) <= 0) break;
  } while (!__atomic_compare_exchange_n(count, &old_value, SemcountDecrement(old_value) | shared, true,
                                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  return SemcountToValue(old_value);
}

int SemInc(std::uint32_t* count) noexcept {
  std::uint32_t old_value = __atomic_load_n(count, __ATOMIC_RELAXED);
  const std::uint32_t shared = old_value & kSemcountSharedMask;
  std::uint32_t new_value;
  do {
    if (SemcountToValue(old_value) == static_cast<int>(kSemValueMax)) break;
    if (SemcountToValue(old_value) < 0) {
      new_value = kSemcountOne | shared;
    } else {
      new_value = SemcountIncrement(old_value) | shared;
    }
  } while (!__atomic_compare_exchange_n(count, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  return SemcountToValue(old_value);
}

int SemTimedWait(guest::Sem* sem, const guest::Timespec* deadline, bool realtime) noexcept {
  std::uint32_t* count = SemCount(sem);
  if (SemTryDec(count) > 0) return 0;
  const int check = CheckTimespec(deadline, false);
  if (check != 0) return FailWith(check, -1);
  const std::uint32_t shared = SemGetShared(count);
  for (;;) {
    if (SemDec(count) > 0) return 0;
    const int result = FutexWaitAbsolute(count, shared | kSemcountMinusOne, realtime, deadline);
    if (result == guest::kETIMEDOUT) return FailWith(guest::kETIMEDOUT, -1);
  }
}

}
}

namespace {
namespace internal = elixir::bionic::threads::internal;
namespace guest = elixir::bionic::guest;
using elixir::bionic::FailWith;
}

ELIXIR_BIONIC_FN(int, pthread_once)(guest::PthreadOnce* once_control,
                                    elixir::abi::SysvFn<void()> init_routine) noexcept {
  std::int32_t* control = once_control;
  std::int32_t old_value = __atomic_load_n(control, __ATOMIC_ACQUIRE);
  for (;;) {
    if (old_value == internal::kOnceComplete) return 0;
    if (!__atomic_compare_exchange_n(control, &old_value, internal::kOnceUnderway, true, __ATOMIC_ACQUIRE,
                                     __ATOMIC_ACQUIRE)) {
      continue;
    }
    if (old_value == internal::kOnceNotYetStarted) {
      init_routine();
      __atomic_store_n(control, internal::kOnceComplete, __ATOMIC_RELEASE);
      internal::FutexWakeWord(control, true);
      return 0;
    }
    internal::FutexWaitWord(control, static_cast<std::uint32_t>(old_value));
    old_value = __atomic_load_n(control, __ATOMIC_ACQUIRE);
  }
}

ELIXIR_BIONIC_FN(int, pthread_barrierattr_init)(std::int32_t* attr) noexcept {
  *attr = 0;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_barrierattr_destroy)(std::int32_t* attr) noexcept {
  *attr = 0;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_barrierattr_getpshared)(const std::int32_t* attr, int* pshared) noexcept {
  *pshared = (*attr & 1) != 0 ? guest::kPTHREAD_PROCESS_SHARED : guest::kPTHREAD_PROCESS_PRIVATE;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_barrierattr_setpshared)(std::int32_t* attr, int pshared) noexcept {
  if (pshared == guest::kPTHREAD_PROCESS_SHARED) {
    *attr |= 1;
  } else {
    *attr &= ~1;
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_barrier_init)(guest::PthreadBarrier* barrier_interface, const std::int32_t* attr,
                                            unsigned count) noexcept {
  internal::BarrierInternal* barrier = internal::Internal(barrier_interface);
  if (count == 0) return guest::kEINVAL;
  barrier->init_count = count;
  __atomic_store_n(&barrier->state, internal::kBarrierWait, __ATOMIC_RELAXED);
  __atomic_store_n(&barrier->wait_count, 0u, __ATOMIC_RELAXED);
  barrier->pshared = attr != nullptr && (*attr & 1) != 0;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_barrier_wait)(guest::PthreadBarrier* barrier_interface) noexcept {
  internal::BarrierInternal* barrier = internal::Internal(barrier_interface);
  while (__atomic_load_n(&barrier->state, __ATOMIC_ACQUIRE) == internal::kBarrierRelease) {
    internal::FutexWaitWord(&barrier->state, internal::kBarrierRelease);
  }
  std::uint32_t prev_wait_count = __atomic_load_n(&barrier->wait_count, __ATOMIC_RELAXED);
  for (;;) {
    if (prev_wait_count >= barrier->init_count) return guest::kEINVAL;
    if (__atomic_compare_exchange_n(&barrier->wait_count, &prev_wait_count, prev_wait_count + 1u, true,
                                    __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
      break;
    }
  }
  int result = 0;
  if (prev_wait_count + 1 == barrier->init_count) {
    result = guest::kPTHREAD_BARRIER_SERIAL_THREAD;
    if (prev_wait_count != 0) {
      __atomic_store_n(&barrier->state, internal::kBarrierRelease, __ATOMIC_RELEASE);
      internal::FutexWakeWord(&barrier->state, true);
    }
  } else {
    while (__atomic_load_n(&barrier->state, __ATOMIC_ACQUIRE) == internal::kBarrierWait) {
      internal::FutexWaitWord(&barrier->state, internal::kBarrierWait);
    }
  }
  if (__atomic_fetch_sub(&barrier->wait_count, 1u, __ATOMIC_RELEASE) == 1) {
    __atomic_store_n(&barrier->state, internal::kBarrierWait, __ATOMIC_RELEASE);
    internal::FutexWakeWord(&barrier->state, true);
  }
  return result;
}

ELIXIR_BIONIC_FN(int, pthread_barrier_destroy)(guest::PthreadBarrier* barrier_interface) noexcept {
  internal::BarrierInternal* barrier = internal::Internal(barrier_interface);
  if (barrier->init_count == 0) return guest::kEINVAL;
  while (__atomic_load_n(&barrier->state, __ATOMIC_ACQUIRE) == internal::kBarrierRelease) {
    internal::FutexWaitWord(&barrier->state, internal::kBarrierRelease);
  }
  if (__atomic_load_n(&barrier->wait_count, __ATOMIC_RELAXED) != 0) return guest::kEBUSY;
  barrier->init_count = 0;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_spin_init)(guest::PthreadSpinlock* lock, int pshared) noexcept {
  internal::LockInit(internal::Internal(lock), pshared != 0);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_spin_destroy)(guest::PthreadSpinlock* lock) noexcept {
  return internal::LockTry(internal::Internal(lock)) ? 0 : guest::kEBUSY;
}

ELIXIR_BIONIC_FN(int, pthread_spin_trylock)(guest::PthreadSpinlock* lock) noexcept {
  return internal::LockTry(internal::Internal(lock)) ? 0 : guest::kEBUSY;
}

ELIXIR_BIONIC_FN(int, pthread_spin_lock)(guest::PthreadSpinlock* lock) noexcept {
  for (int i = 0; i < 10000; ++i) {
    if (internal::LockTry(internal::Internal(lock))) return 0;
  }
  internal::LockAcquire(internal::Internal(lock));
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_spin_unlock)(guest::PthreadSpinlock* lock) noexcept {
  internal::LockRelease(internal::Internal(lock));
  return 0;
}

ELIXIR_BIONIC_FN(int, sem_init)(guest::Sem* sem, int pshared, unsigned value) noexcept {
  if (value > internal::kSemValueMax) return FailWith(guest::kEINVAL, -1);
  std::uint32_t count = internal::SemcountFromValue(value);
  if (pshared != 0) count |= internal::kSemcountSharedMask;
  __atomic_store_n(internal::SemCount(sem), count, __ATOMIC_RELAXED);
  return 0;
}

ELIXIR_BIONIC_FN(int, sem_destroy)(guest::Sem*) noexcept { return 0; }

ELIXIR_BIONIC_FN(guest::Sem*, sem_open)(const char*, int, unsigned, unsigned) noexcept {
  return FailWith<guest::Sem*>(guest::kENOSYS, nullptr);
}

ELIXIR_BIONIC_FN(int, sem_close)(guest::Sem*) noexcept { return FailWith(guest::kENOSYS, -1); }

ELIXIR_BIONIC_FN(int, sem_unlink)(const char*) noexcept { return FailWith(guest::kENOSYS, -1); }

ELIXIR_BIONIC_FN(int, sem_wait)(guest::Sem* sem) noexcept {
  std::uint32_t* count = internal::SemCount(sem);
  const std::uint32_t shared = internal::SemGetShared(count);
  for (;;) {
    if (internal::SemDec(count) > 0) return 0;
    internal::FutexWaitWord(count, shared | internal::kSemcountMinusOne);
  }
}

ELIXIR_BIONIC_FN(int, sem_timedwait)(guest::Sem* sem, const guest::Timespec* deadline) noexcept {
  return internal::SemTimedWait(sem, deadline, true);
}

ELIXIR_BIONIC_FN(int, sem_timedwait_monotonic_np)(guest::Sem* sem, const guest::Timespec* deadline) noexcept {
  return internal::SemTimedWait(sem, deadline, false);
}

ELIXIR_BIONIC_FN(int, sem_clockwait)(guest::Sem* sem, elixir::abi::g_clockid clock,
                                     const guest::Timespec* deadline) noexcept {
  switch (clock) {
    case guest::kCLOCK_MONOTONIC: return internal::SemTimedWait(sem, deadline, false);
    case guest::kCLOCK_REALTIME: return internal::SemTimedWait(sem, deadline, true);
    default: return guest::kEINVAL;
  }
}

ELIXIR_BIONIC_FN(int, sem_post)(guest::Sem* sem) noexcept {
  std::uint32_t* count = internal::SemCount(sem);
  const int old_value = internal::SemInc(count);
  if (old_value < 0) {
    internal::FutexWakeWord(count, true);
  } else if (old_value == static_cast<int>(internal::kSemValueMax)) {
    return FailWith(guest::kEOVERFLOW, -1);
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, sem_trywait)(guest::Sem* sem) noexcept {
  if (internal::SemTryDec(internal::SemCount(sem)) > 0) return 0;
  return FailWith(guest::kEAGAIN, -1);
}

ELIXIR_BIONIC_FN(int, sem_getvalue)(guest::Sem* sem, int* sval) noexcept {
  int value = internal::SemcountToValue(__atomic_load_n(internal::SemCount(sem), __ATOMIC_SEQ_CST));
  if (value < 0) value = 0;
  *sval = value;
  return 0;
}

namespace elixir::bionic::threads::internal {

void RegisterSyncMiscExports(ExportTable& table) {
#define ELIXIR_THREADS_EXPORT(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_THREADS_EXPORT(pthread_once);
  ELIXIR_THREADS_EXPORT(pthread_barrierattr_init);
  ELIXIR_THREADS_EXPORT(pthread_barrierattr_destroy);
  ELIXIR_THREADS_EXPORT(pthread_barrierattr_getpshared);
  ELIXIR_THREADS_EXPORT(pthread_barrierattr_setpshared);
  ELIXIR_THREADS_EXPORT(pthread_barrier_init);
  ELIXIR_THREADS_EXPORT(pthread_barrier_wait);
  ELIXIR_THREADS_EXPORT(pthread_barrier_destroy);
  ELIXIR_THREADS_EXPORT(pthread_spin_init);
  ELIXIR_THREADS_EXPORT(pthread_spin_destroy);
  ELIXIR_THREADS_EXPORT(pthread_spin_trylock);
  ELIXIR_THREADS_EXPORT(pthread_spin_lock);
  ELIXIR_THREADS_EXPORT(pthread_spin_unlock);
  ELIXIR_THREADS_EXPORT(sem_init);
  ELIXIR_THREADS_EXPORT(sem_destroy);
  ELIXIR_THREADS_EXPORT(sem_open);
  ELIXIR_THREADS_EXPORT(sem_close);
  ELIXIR_THREADS_EXPORT(sem_unlink);
  ELIXIR_THREADS_EXPORT(sem_wait);
  ELIXIR_THREADS_EXPORT(sem_timedwait);
  ELIXIR_THREADS_EXPORT(sem_timedwait_monotonic_np);
  ELIXIR_THREADS_EXPORT(sem_clockwait);
  ELIXIR_THREADS_EXPORT(sem_post);
  ELIXIR_THREADS_EXPORT(sem_trywait);
  ELIXIR_THREADS_EXPORT(sem_getvalue);
#undef ELIXIR_THREADS_EXPORT
}

}
