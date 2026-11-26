// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/pthread_mutex.cpp and
// libc/bionic/bionic_futex.cpp:
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
#include <cstdio>
#include <cstring>

#include "bionic/threads/thread_internal.h"

namespace elixir::bionic::threads::internal {
namespace {

constexpr guest::PthreadMutexattr kMutexattrTypeMask = 0x000f;
constexpr guest::PthreadMutexattr kMutexattrSharedMask = 0x0010;
constexpr guest::PthreadMutexattr kMutexattrProtocolMask = 0x0020;
constexpr int kMutexattrProtocolShift = 5;

constexpr std::uint16_t kStateMask = 0x3;
constexpr std::uint16_t kStateUnlocked = 0;
constexpr std::uint16_t kStateLockedUncontended = 1;
constexpr std::uint16_t kStateLockedContended = 2;
constexpr int kCounterShift = 2;
constexpr std::uint16_t kCounterMask = ((1u << 11) - 1) << kCounterShift;
constexpr std::uint16_t kCounterOne = 1u << kCounterShift;
constexpr std::uint16_t kSharedMask = 1u << 13;
constexpr int kTypeShift = 14;
constexpr std::uint16_t kTypeMask = 0x3u << kTypeShift;
constexpr std::uint16_t kTypeBitsNormal = kPthreadMutexNormal << kTypeShift;
constexpr std::uint16_t kTypeBitsRecursive = kPthreadMutexRecursive << kTypeShift;
constexpr std::uint16_t kTypeBitsErrorcheck = kPthreadMutexErrorcheck << kTypeShift;
constexpr std::uint16_t kPiMutexState = 3u << kTypeShift;
constexpr std::uint16_t kDestroyedState = 0xffff;

struct PiMutex {
  std::uint8_t type;
  bool shared;
  std::uint16_t counter;
  std::int32_t owner_tid;
};

struct MutexInternal {
  std::uint16_t state;
  std::uint16_t pad;
  union {
    std::int32_t owner_tid;
    PiMutex pi_mutex;
  };
  char reserved[28];
};
static_assert(sizeof(MutexInternal) == sizeof(guest::PthreadMutex), "pthread_mutex_t size");
static_assert(alignof(MutexInternal) <= alignof(guest::PthreadMutex), "pthread_mutex_t alignment");
static_assert(offsetof(MutexInternal, owner_tid) == 4, "owner_tid offset");

inline MutexInternal* Internal(guest::PthreadMutex* mutex) noexcept { return reinterpret_cast<MutexInternal*>(mutex); }

inline std::uint16_t LoadState(MutexInternal* mutex) noexcept { return __atomic_load_n(&mutex->state, __ATOMIC_RELAXED); }

inline int StateWait(MutexInternal* mutex, std::uint16_t expected, bool realtime, const guest::Timespec* deadline) noexcept {
  return FutexWaitAbsolute(&mutex->state, expected, realtime, deadline);
}
inline void StateWake(MutexInternal* mutex) noexcept { FutexWakeWord(&mutex->state, false); }

int PiMutexTryLock(PiMutex& mutex) noexcept {
  const std::int32_t tid = CurrentTid();
  std::int32_t old_owner = 0;
  if (__atomic_compare_exchange_n(&mutex.owner_tid, &old_owner, tid, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
    return 0;
  }
  if (tid == (old_owner & kFutexTidMask)) {
    if (mutex.type == kPthreadMutexNormal) return guest::kEBUSY;
    if (mutex.type == kPthreadMutexErrorcheck) return guest::kEDEADLK;
    if (mutex.counter == 0xffff) return guest::kEAGAIN;
    mutex.counter++;
    return 0;
  }
  return guest::kEBUSY;
}

int PiMutexLockSlow(PiMutex& mutex, bool realtime, const guest::Timespec* deadline) noexcept {
  const int check = CheckTimespec(deadline, true);
  if (check != 0) return check;
  const std::int32_t tid = CurrentTid();
  const std::int64_t timeout = RelativeTimeoutNs(deadline, realtime);
  const std::int64_t end = timeout < 0 ? -1 : MonotonicNowNs() + timeout;
  bool waited = false;
  for (;;) {
    std::int32_t value = __atomic_load_n(&mutex.owner_tid, __ATOMIC_RELAXED);
    if ((value & kFutexTidMask) == 0) {
      const std::int32_t desired = waited ? (tid | kFutexWaiters) : (tid | (value & kFutexWaiters));
      if (__atomic_compare_exchange_n(&mutex.owner_tid, &value, desired, false, __ATOMIC_ACQUIRE,
                                      __ATOMIC_RELAXED)) {
        return 0;
      }
      continue;
    }
    if ((value & kFutexTidMask) == tid) return guest::kEDEADLK;
    if ((value & kFutexWaiters) == 0) {
      if (!__atomic_compare_exchange_n(&mutex.owner_tid, &value, value | kFutexWaiters, false, __ATOMIC_RELAXED,
                                       __ATOMIC_RELAXED)) {
        continue;
      }
      value |= kFutexWaiters;
    }
    std::int64_t remaining = -1;
    if (end >= 0) {
      remaining = end - MonotonicNowNs();
      if (remaining <= 0) return guest::kETIMEDOUT;
    }
    waited = true;
    if (FutexWait(&mutex.owner_tid, value, remaining) == guest::kETIMEDOUT) return guest::kETIMEDOUT;
  }
}

int PiMutexTimedLock(PiMutex& mutex, bool realtime, const guest::Timespec* deadline) noexcept {
  int result = PiMutexTryLock(mutex);
  if (result == 0) return 0;
  if (result == guest::kEBUSY) result = PiMutexLockSlow(mutex, realtime, deadline);
  return result;
}

int PiMutexUnlock(PiMutex& mutex) noexcept {
  const std::int32_t tid = CurrentTid();
  std::int32_t old_owner = tid;
  if (mutex.type == kPthreadMutexNormal) {
    if (__atomic_compare_exchange_n(&mutex.owner_tid, &old_owner, 0, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
      return 0;
    }
  } else {
    old_owner = __atomic_load_n(&mutex.owner_tid, __ATOMIC_RELAXED);
  }
  if (tid != (old_owner & kFutexTidMask)) return guest::kEPERM;
  if (mutex.type == kPthreadMutexRecursive && mutex.counter != 0) {
    --mutex.counter;
    return 0;
  }
  if (old_owner == tid) {
    if (__atomic_compare_exchange_n(&mutex.owner_tid, &old_owner, 0, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
      return 0;
    }
  }
  __atomic_store_n(&mutex.owner_tid, 0, __ATOMIC_RELEASE);
  FutexWake(&mutex.owner_tid, false);
  return 0;
}

int PiMutexDestroy(PiMutex& mutex) noexcept {
  std::int32_t old_owner = 0;
  if (__atomic_compare_exchange_n(&mutex.owner_tid, &old_owner, static_cast<std::int32_t>(0xffffffffu), false,
                                  __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
    return 0;
  }
  return guest::kEBUSY;
}

inline int NormalMutexTryLock(MutexInternal* mutex, std::uint16_t shared) noexcept {
  const std::uint16_t unlocked = shared | kStateUnlocked;
  const std::uint16_t locked_uncontended = shared | kStateLockedUncontended;
  std::uint16_t old_state = unlocked;
  if (__atomic_compare_exchange_n(&mutex->state, &old_state, locked_uncontended, false, __ATOMIC_ACQUIRE,
                                  __ATOMIC_RELAXED)) {
    return 0;
  }
  return guest::kEBUSY;
}

int NormalMutexLock(MutexInternal* mutex, std::uint16_t shared, bool realtime, const guest::Timespec* deadline) noexcept {
  if (NormalMutexTryLock(mutex, shared) == 0) return 0;
  const int check = CheckTimespec(deadline, true);
  if (check != 0) return check;
  const std::uint16_t unlocked = shared | kStateUnlocked;
  const std::uint16_t locked_contended = shared | kStateLockedContended;
  while (__atomic_exchange_n(&mutex->state, locked_contended, __ATOMIC_ACQUIRE) != unlocked) {
    if (StateWait(mutex, locked_contended, realtime, deadline) == guest::kETIMEDOUT) return guest::kETIMEDOUT;
  }
  return 0;
}

inline void NormalMutexUnlock(MutexInternal* mutex, std::uint16_t shared) noexcept {
  const std::uint16_t unlocked = shared | kStateUnlocked;
  const std::uint16_t locked_contended = shared | kStateLockedContended;
  if (__atomic_exchange_n(&mutex->state, unlocked, __ATOMIC_RELEASE) == locked_contended) {
    StateWake(mutex);
  }
}

inline int RecursiveIncrement(MutexInternal* mutex, std::uint16_t old_state) noexcept {
  if ((old_state & kCounterMask) == kCounterMask) return guest::kEAGAIN;
  __atomic_fetch_add(&mutex->state, kCounterOne, __ATOMIC_RELAXED);
  return 0;
}

int MutexLockWithTimeout(MutexInternal* mutex, bool realtime, const guest::Timespec* deadline) noexcept {
  std::uint16_t old_state = LoadState(mutex);
  const std::uint16_t mtype = old_state & kTypeMask;
  const std::uint16_t shared = old_state & kSharedMask;
  if (mtype == kTypeBitsNormal) return NormalMutexLock(mutex, shared, realtime, deadline);

  const std::int32_t tid = CurrentTid();
  if (tid == __atomic_load_n(&mutex->owner_tid, __ATOMIC_RELAXED)) {
    if (mtype == kTypeBitsErrorcheck) return guest::kEDEADLK;
    return RecursiveIncrement(mutex, old_state);
  }

  const std::uint16_t unlocked = mtype | shared | kStateUnlocked;
  const std::uint16_t locked_uncontended = mtype | shared | kStateLockedUncontended;
  const std::uint16_t locked_contended = mtype | shared | kStateLockedContended;

  if (old_state == unlocked) {
    if (__atomic_compare_exchange_n(&mutex->state, &old_state, locked_uncontended, false, __ATOMIC_ACQUIRE,
                                    __ATOMIC_RELAXED)) {
      __atomic_store_n(&mutex->owner_tid, tid, __ATOMIC_RELAXED);
      return 0;
    }
  }

  for (;;) {
    if (old_state == unlocked) {
      if (__atomic_compare_exchange_n(&mutex->state, &old_state, locked_contended, true, __ATOMIC_ACQUIRE,
                                      __ATOMIC_RELAXED)) {
        __atomic_store_n(&mutex->owner_tid, tid, __ATOMIC_RELAXED);
        return 0;
      }
      continue;
    } else if ((old_state & kStateMask) == kStateLockedUncontended) {
      const std::uint16_t new_state = old_state ^ (kStateLockedContended ^ kStateLockedUncontended);
      if (!__atomic_compare_exchange_n(&mutex->state, &old_state, new_state, true, __ATOMIC_RELAXED,
                                       __ATOMIC_RELAXED)) {
        continue;
      }
      old_state = new_state;
    }
    const int check = CheckTimespec(deadline, true);
    if (check != 0) return check;
    if (StateWait(mutex, old_state, realtime, deadline) == guest::kETIMEDOUT) return guest::kETIMEDOUT;
    old_state = LoadState(mutex);
  }
}

int HandleUsingDestroyedMutex(guest::PthreadMutex* mutex, const char* function) noexcept {
  if (Options().sdk_version >= 28) {
    char message[128];
    std::snprintf(message, sizeof(message), "%s called on a destroyed mutex (%p)", function,
                  static_cast<void*>(mutex));
    FatalError(message);
  }
  return guest::kEBUSY;
}

int MutexTimedLock(guest::PthreadMutex* mutex_interface, bool realtime, const guest::Timespec* deadline,
                   const char* function) noexcept {
  MutexInternal* mutex = Internal(mutex_interface);
  const std::uint16_t old_state = LoadState(mutex);
  const std::uint16_t mtype = old_state & kTypeMask;
  if (mtype == kTypeBitsNormal) {
    if (NormalMutexTryLock(mutex, old_state & kSharedMask) == 0) return 0;
  }
  if (old_state == kPiMutexState) return PiMutexTimedLock(mutex->pi_mutex, realtime, deadline);
  if (old_state == kDestroyedState) return HandleUsingDestroyedMutex(mutex_interface, function);
  return MutexLockWithTimeout(mutex, realtime, deadline);
}

}

int MutexLock(guest::PthreadMutex* mutex_interface) noexcept {
  MutexInternal* mutex = Internal(mutex_interface);
  const std::uint16_t old_state = LoadState(mutex);
  const std::uint16_t mtype = old_state & kTypeMask;
  if (mtype == kTypeBitsNormal) {
    if (NormalMutexTryLock(mutex, old_state & kSharedMask) == 0) return 0;
  }
  if (old_state == kPiMutexState) {
    if (PiMutexTryLock(mutex->pi_mutex) == 0) return 0;
    return PiMutexTimedLock(mutex->pi_mutex, false, nullptr);
  }
  if (old_state == kDestroyedState) return HandleUsingDestroyedMutex(mutex_interface, "pthread_mutex_lock");
  return MutexLockWithTimeout(mutex, false, nullptr);
}

int MutexUnlock(guest::PthreadMutex* mutex_interface) noexcept {
  MutexInternal* mutex = Internal(mutex_interface);
  std::uint16_t old_state = LoadState(mutex);
  const std::uint16_t mtype = old_state & kTypeMask;
  const std::uint16_t shared = old_state & kSharedMask;
  if (mtype == kTypeBitsNormal) {
    NormalMutexUnlock(mutex, shared);
    return 0;
  }
  if (old_state == kPiMutexState) return PiMutexUnlock(mutex->pi_mutex);
  if (old_state == kDestroyedState) return HandleUsingDestroyedMutex(mutex_interface, "pthread_mutex_unlock");

  const std::int32_t tid = CurrentTid();
  if (tid != __atomic_load_n(&mutex->owner_tid, __ATOMIC_RELAXED)) return guest::kEPERM;
  if ((old_state & kCounterMask) != 0) {
    __atomic_fetch_sub(&mutex->state, kCounterOne, __ATOMIC_RELAXED);
    return 0;
  }
  __atomic_store_n(&mutex->owner_tid, 0, __ATOMIC_RELAXED);
  const std::uint16_t unlocked = mtype | shared | kStateUnlocked;
  old_state = __atomic_exchange_n(&mutex->state, unlocked, __ATOMIC_RELEASE);
  if ((old_state & kStateMask) == kStateLockedContended) StateWake(mutex);
  return 0;
}

}

namespace {
namespace internal = elixir::bionic::threads::internal;
namespace guest = elixir::bionic::guest;
using internal::MutexInternal;
}

ELIXIR_BIONIC_FN(int, pthread_mutexattr_init)(guest::PthreadMutexattr* attr) noexcept {
  *attr = internal::kPthreadMutexNormal;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_mutexattr_destroy)(guest::PthreadMutexattr* attr) noexcept {
  *attr = -1;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_mutexattr_gettype)(const guest::PthreadMutexattr* attr, int* type_out) noexcept {
  const int type = static_cast<int>(*attr & internal::kMutexattrTypeMask);
  if (type < internal::kPthreadMutexNormal || type > internal::kPthreadMutexErrorcheck) return guest::kEINVAL;
  *type_out = type;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_mutexattr_settype)(guest::PthreadMutexattr* attr, int type) noexcept {
  if (type < internal::kPthreadMutexNormal || type > internal::kPthreadMutexErrorcheck) return guest::kEINVAL;
  *attr = (*attr & ~internal::kMutexattrTypeMask) | type;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_mutexattr_setpshared)(guest::PthreadMutexattr* attr, int pshared) noexcept {
  switch (pshared) {
    case guest::kPTHREAD_PROCESS_PRIVATE:
      *attr &= ~internal::kMutexattrSharedMask;
      return 0;
    case guest::kPTHREAD_PROCESS_SHARED:
      *attr |= internal::kMutexattrSharedMask;
      return 0;
  }
  return guest::kEINVAL;
}

ELIXIR_BIONIC_FN(int, pthread_mutexattr_getpshared)(const guest::PthreadMutexattr* attr, int* pshared) noexcept {
  *pshared = (*attr & internal::kMutexattrSharedMask) != 0 ? guest::kPTHREAD_PROCESS_SHARED : guest::kPTHREAD_PROCESS_PRIVATE;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_mutexattr_setprotocol)(guest::PthreadMutexattr* attr, int protocol) noexcept {
  if (protocol != guest::kPTHREAD_PRIO_NONE && protocol != guest::kPTHREAD_PRIO_INHERIT) return guest::kEINVAL;
  *attr = (*attr & ~internal::kMutexattrProtocolMask) |
          (static_cast<guest::PthreadMutexattr>(protocol) << internal::kMutexattrProtocolShift);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_mutexattr_getprotocol)(const guest::PthreadMutexattr* attr, int* protocol) noexcept {
  *protocol = static_cast<int>((*attr & internal::kMutexattrProtocolMask) >> internal::kMutexattrProtocolShift);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_mutex_init)(guest::PthreadMutex* mutex_interface,
                                          const guest::PthreadMutexattr* attr) noexcept {
  MutexInternal* mutex = internal::Internal(mutex_interface);
  std::memset(mutex, 0, sizeof(*mutex));
  if (attr == nullptr) {
    __atomic_store_n(&mutex->state, internal::kTypeBitsNormal, __ATOMIC_RELAXED);
    return 0;
  }
  std::uint16_t state = 0;
  if ((*attr & internal::kMutexattrSharedMask) != 0) state |= internal::kSharedMask;
  switch (*attr & internal::kMutexattrTypeMask) {
    case internal::kPthreadMutexNormal: state |= internal::kTypeBitsNormal; break;
    case internal::kPthreadMutexRecursive: state |= internal::kTypeBitsRecursive; break;
    case internal::kPthreadMutexErrorcheck: state |= internal::kTypeBitsErrorcheck; break;
    default: return guest::kEINVAL;
  }
  if (((*attr & internal::kMutexattrProtocolMask) >> internal::kMutexattrProtocolShift) == guest::kPTHREAD_PRIO_INHERIT) {
    __atomic_store_n(&mutex->state, internal::kPiMutexState, __ATOMIC_RELAXED);
    mutex->pi_mutex.type = static_cast<std::uint8_t>(*attr & internal::kMutexattrTypeMask);
    mutex->pi_mutex.shared = (*attr & internal::kMutexattrSharedMask) != 0;
  } else {
    __atomic_store_n(&mutex->state, state, __ATOMIC_RELAXED);
    __atomic_store_n(&mutex->owner_tid, 0, __ATOMIC_RELAXED);
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_mutex_lock)(guest::PthreadMutex* mutex) noexcept { return internal::MutexLock(mutex); }

ELIXIR_BIONIC_FN(int, pthread_mutex_unlock)(guest::PthreadMutex* mutex) noexcept { return internal::MutexUnlock(mutex); }

ELIXIR_BIONIC_FN(int, pthread_mutex_trylock)(guest::PthreadMutex* mutex_interface) noexcept {
  MutexInternal* mutex = internal::Internal(mutex_interface);
  std::uint16_t old_state = internal::LoadState(mutex);
  const std::uint16_t mtype = old_state & internal::kTypeMask;
  if (mtype == internal::kTypeBitsNormal) return internal::NormalMutexTryLock(mutex, old_state & internal::kSharedMask);
  if (old_state == internal::kPiMutexState) return internal::PiMutexTryLock(mutex->pi_mutex);
  if (old_state == internal::kDestroyedState) return internal::HandleUsingDestroyedMutex(mutex_interface, "pthread_mutex_trylock");

  const std::int32_t tid = elixir::bionic::threads::CurrentTid();
  if (tid == __atomic_load_n(&mutex->owner_tid, __ATOMIC_RELAXED)) {
    if (mtype == internal::kTypeBitsErrorcheck) return guest::kEBUSY;
    return internal::RecursiveIncrement(mutex, old_state);
  }
  const std::uint16_t shared = old_state & internal::kSharedMask;
  const std::uint16_t unlocked = mtype | shared | internal::kStateUnlocked;
  const std::uint16_t locked_uncontended = mtype | shared | internal::kStateLockedUncontended;
  old_state = unlocked;
  if (__atomic_compare_exchange_n(&mutex->state, &old_state, locked_uncontended, false, __ATOMIC_ACQUIRE,
                                  __ATOMIC_RELAXED)) {
    __atomic_store_n(&mutex->owner_tid, tid, __ATOMIC_RELAXED);
    return 0;
  }
  return guest::kEBUSY;
}

ELIXIR_BIONIC_FN(int, pthread_mutex_timedlock)(guest::PthreadMutex* mutex, const guest::Timespec* deadline) noexcept {
  return internal::MutexTimedLock(mutex, true, deadline, "pthread_mutex_timedlock");
}

ELIXIR_BIONIC_FN(int, pthread_mutex_timedlock_monotonic_np)(guest::PthreadMutex* mutex,
                                                            const guest::Timespec* deadline) noexcept {
  return internal::MutexTimedLock(mutex, false, deadline, "pthread_mutex_timedlock_monotonic_np");
}

ELIXIR_BIONIC_FN(int, pthread_mutex_clocklock)(guest::PthreadMutex* mutex, elixir::abi::g_clockid clock,
                                               const guest::Timespec* deadline) noexcept {
  switch (clock) {
    case guest::kCLOCK_MONOTONIC: return internal::MutexTimedLock(mutex, false, deadline, "pthread_mutex_clocklock");
    case guest::kCLOCK_REALTIME: return internal::MutexTimedLock(mutex, true, deadline, "pthread_mutex_clocklock");
    default:
      if (internal::LoadState(internal::Internal(mutex)) == internal::kDestroyedState) {
        return internal::HandleUsingDestroyedMutex(mutex, "pthread_mutex_clocklock");
      }
      return guest::kEINVAL;
  }
}

ELIXIR_BIONIC_FN(int, pthread_mutex_destroy)(guest::PthreadMutex* mutex_interface) noexcept {
  MutexInternal* mutex = internal::Internal(mutex_interface);
  std::uint16_t old_state = internal::LoadState(mutex);
  if (old_state == internal::kDestroyedState) return internal::HandleUsingDestroyedMutex(mutex_interface, "pthread_mutex_destroy");
  if (old_state == internal::kPiMutexState) {
    const int result = internal::PiMutexDestroy(mutex->pi_mutex);
    if (result == 0) __atomic_store_n(&mutex->state, internal::kDestroyedState, __ATOMIC_SEQ_CST);
    return result;
  }
  if ((old_state & internal::kStateMask) == internal::kStateUnlocked &&
      __atomic_compare_exchange_n(&mutex->state, &old_state, internal::kDestroyedState, false, __ATOMIC_RELAXED,
                                  __ATOMIC_RELAXED)) {
    return 0;
  }
  return guest::kEBUSY;
}

namespace elixir::bionic::threads::internal {

void RegisterMutexExports(ExportTable& table) {
#define ELIXIR_THREADS_EXPORT(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_THREADS_EXPORT(pthread_mutexattr_init);
  ELIXIR_THREADS_EXPORT(pthread_mutexattr_destroy);
  ELIXIR_THREADS_EXPORT(pthread_mutexattr_gettype);
  ELIXIR_THREADS_EXPORT(pthread_mutexattr_settype);
  ELIXIR_THREADS_EXPORT(pthread_mutexattr_setpshared);
  ELIXIR_THREADS_EXPORT(pthread_mutexattr_getpshared);
  ELIXIR_THREADS_EXPORT(pthread_mutexattr_setprotocol);
  ELIXIR_THREADS_EXPORT(pthread_mutexattr_getprotocol);
  ELIXIR_THREADS_EXPORT(pthread_mutex_init);
  ELIXIR_THREADS_EXPORT(pthread_mutex_destroy);
  ELIXIR_THREADS_EXPORT(pthread_mutex_lock);
  ELIXIR_THREADS_EXPORT(pthread_mutex_trylock);
  ELIXIR_THREADS_EXPORT(pthread_mutex_unlock);
  ELIXIR_THREADS_EXPORT(pthread_mutex_timedlock);
  ELIXIR_THREADS_EXPORT(pthread_mutex_timedlock_monotonic_np);
  ELIXIR_THREADS_EXPORT(pthread_mutex_clocklock);
#undef ELIXIR_THREADS_EXPORT
}

}
