// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/pthread_rwlock.cpp:
//
//   Copyright (C) 2010 The Android Open Source Project
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
#include <cstring>

#include "bionic/threads/thread_internal.h"

namespace elixir::bionic::threads::internal {
namespace {

constexpr guest::PthreadRwlockattr kRwlockattrPsharedMask = 1;
constexpr int kRwlockattrKindShift = 1;
constexpr guest::PthreadRwlockattr kRwlockattrKindMask = 2;
constexpr guest::PthreadRwlockattr kRwlockattrReservedMask = ~static_cast<guest::PthreadRwlockattr>(3);

constexpr std::int32_t kStateHavePendingReadersFlag = 1 << 0;
constexpr std::int32_t kStateHavePendingWritersFlag = 1 << 1;
constexpr std::int32_t kStateReaderCountChangeStep = 1 << 2;
constexpr std::int32_t kStateOwnedByWriterFlag = static_cast<std::int32_t>(1u << 31);
constexpr std::int32_t kStateHavePendingReadersOrWritersFlag =
    kStateHavePendingReadersFlag | kStateHavePendingWritersFlag;

struct RwlockInternal {
  std::int32_t state;
  std::int32_t writer_tid;
  bool pshared;
  bool writer_nonrecursive_preferred;
  std::uint16_t pad;
  GuestLock pending_lock;
  std::uint32_t pending_reader_count;
  std::uint32_t pending_writer_count;
  std::uint32_t pending_reader_wakeup_serial;
  std::uint32_t pending_writer_wakeup_serial;
  char reserved[20];
};
static_assert(sizeof(RwlockInternal) == sizeof(guest::PthreadRwlock), "pthread_rwlock_t size");
static_assert(alignof(RwlockInternal) <= alignof(guest::PthreadRwlock), "pthread_rwlock_t alignment");
static_assert(offsetof(RwlockInternal, pending_lock) == 12, "pending_lock offset");
static_assert(offsetof(RwlockInternal, pending_writer_wakeup_serial) == 32, "wakeup serial offset");

inline RwlockInternal* Internal(guest::PthreadRwlock* rwlock) noexcept { return reinterpret_cast<RwlockInternal*>(rwlock); }

inline bool StateOwnedByWriter(std::int32_t state) noexcept { return state < 0; }
inline bool StateOwnedByReaders(std::int32_t state) noexcept { return state >= kStateReaderCountChangeStep; }
inline bool StateOwnedByReadersOrWriter(std::int32_t state) noexcept {
  return state < 0 || state >= kStateReaderCountChangeStep;
}
inline bool StateIsLastReader(std::int32_t state) noexcept { return (state >> 2) == 1; }
inline bool StateHavePendingWriters(std::int32_t state) noexcept { return (state & kStateHavePendingWritersFlag) != 0; }
inline bool StateHavePendingReadersOrWriters(std::int32_t state) noexcept {
  return (state & kStateHavePendingReadersOrWritersFlag) != 0;
}

inline bool CanAcquireReadLock(std::int32_t old_state, bool writer_nonrecursive_preferred) noexcept {
  const bool cannot_apply = StateOwnedByWriter(old_state) ||
                            (writer_nonrecursive_preferred && StateHavePendingWriters(old_state));
  return !cannot_apply;
}

inline bool CanAcquireWriteLock(std::int32_t old_state) noexcept { return !StateOwnedByReadersOrWriter(old_state); }

int TryRdlock(RwlockInternal* rwlock) noexcept {
  std::int32_t old_state = __atomic_load_n(&rwlock->state, __ATOMIC_RELAXED);
  while (CanAcquireReadLock(old_state, rwlock->writer_nonrecursive_preferred)) {
    const std::int32_t new_state =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(old_state) + kStateReaderCountChangeStep);
    if (!StateOwnedByReaders(new_state)) return guest::kEAGAIN;
    if (__atomic_compare_exchange_n(&rwlock->state, &old_state, new_state, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
      return 0;
    }
  }
  return guest::kEBUSY;
}

int TimedRdlock(RwlockInternal* rwlock, bool realtime, const guest::Timespec* deadline) noexcept {
  if (__atomic_load_n(&rwlock->writer_tid, __ATOMIC_RELAXED) == CurrentTid()) return guest::kEDEADLK;
  for (;;) {
    int result = TryRdlock(rwlock);
    if (result == 0 || result == guest::kEAGAIN) return result;
    result = CheckTimespec(deadline, true);
    if (result != 0) return result;
    std::int32_t old_state = __atomic_load_n(&rwlock->state, __ATOMIC_RELAXED);
    if (CanAcquireReadLock(old_state, rwlock->writer_nonrecursive_preferred)) continue;

    LockAcquire(&rwlock->pending_lock);
    rwlock->pending_reader_count++;
    old_state = __atomic_fetch_or(&rwlock->state, kStateHavePendingReadersFlag, __ATOMIC_RELAXED);
    const std::uint32_t old_serial = rwlock->pending_reader_wakeup_serial;
    LockRelease(&rwlock->pending_lock);

    int futex_result = 0;
    if (!CanAcquireReadLock(old_state, rwlock->writer_nonrecursive_preferred)) {
      futex_result = FutexWaitAbsolute(&rwlock->pending_reader_wakeup_serial, old_serial, realtime, deadline);
    }
    LockAcquire(&rwlock->pending_lock);
    rwlock->pending_reader_count--;
    if (rwlock->pending_reader_count == 0) {
      __atomic_fetch_and(&rwlock->state, ~kStateHavePendingReadersFlag, __ATOMIC_RELAXED);
    }
    LockRelease(&rwlock->pending_lock);
    if (futex_result == guest::kETIMEDOUT) return guest::kETIMEDOUT;
  }
}

int TryWrlock(RwlockInternal* rwlock) noexcept {
  std::int32_t old_state = __atomic_load_n(&rwlock->state, __ATOMIC_RELAXED);
  while (CanAcquireWriteLock(old_state)) {
    if (__atomic_compare_exchange_n(&rwlock->state, &old_state, old_state | kStateOwnedByWriterFlag, true,
                                    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
      __atomic_store_n(&rwlock->writer_tid, CurrentTid(), __ATOMIC_RELAXED);
      return 0;
    }
  }
  return guest::kEBUSY;
}

int TimedWrlock(RwlockInternal* rwlock, bool realtime, const guest::Timespec* deadline) noexcept {
  if (__atomic_load_n(&rwlock->writer_tid, __ATOMIC_RELAXED) == CurrentTid()) return guest::kEDEADLK;
  for (;;) {
    int result = TryWrlock(rwlock);
    if (result == 0) return result;
    result = CheckTimespec(deadline, true);
    if (result != 0) return result;
    std::int32_t old_state = __atomic_load_n(&rwlock->state, __ATOMIC_RELAXED);
    if (CanAcquireWriteLock(old_state)) continue;

    LockAcquire(&rwlock->pending_lock);
    rwlock->pending_writer_count++;
    old_state = __atomic_fetch_or(&rwlock->state, kStateHavePendingWritersFlag, __ATOMIC_RELAXED);
    const std::uint32_t old_serial = rwlock->pending_writer_wakeup_serial;
    LockRelease(&rwlock->pending_lock);

    int futex_result = 0;
    if (!CanAcquireWriteLock(old_state)) {
      futex_result = FutexWaitAbsolute(&rwlock->pending_writer_wakeup_serial, old_serial, realtime, deadline);
    }
    LockAcquire(&rwlock->pending_lock);
    rwlock->pending_writer_count--;
    if (rwlock->pending_writer_count == 0) {
      __atomic_fetch_and(&rwlock->state, ~kStateHavePendingWritersFlag, __ATOMIC_RELAXED);
    }
    LockRelease(&rwlock->pending_lock);
    if (futex_result == guest::kETIMEDOUT) return guest::kETIMEDOUT;
  }
}

}
}

namespace {
namespace internal = elixir::bionic::threads::internal;
namespace guest = elixir::bionic::guest;
using internal::RwlockInternal;
}

ELIXIR_BIONIC_FN(int, pthread_rwlockattr_init)(guest::PthreadRwlockattr* attr) noexcept {
  *attr = 0;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_rwlockattr_destroy)(guest::PthreadRwlockattr* attr) noexcept {
  *attr = -1;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_rwlockattr_getpshared)(const guest::PthreadRwlockattr* attr, int* pshared) noexcept {
  *pshared = (*attr & internal::kRwlockattrPsharedMask) != 0 ? guest::kPTHREAD_PROCESS_SHARED : guest::kPTHREAD_PROCESS_PRIVATE;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_rwlockattr_setpshared)(guest::PthreadRwlockattr* attr, int pshared) noexcept {
  switch (pshared) {
    case guest::kPTHREAD_PROCESS_PRIVATE: *attr &= ~internal::kRwlockattrPsharedMask; return 0;
    case guest::kPTHREAD_PROCESS_SHARED: *attr |= internal::kRwlockattrPsharedMask; return 0;
    default: return guest::kEINVAL;
  }
}

ELIXIR_BIONIC_FN(int, pthread_rwlockattr_getkind_np)(const guest::PthreadRwlockattr* attr, int* pref) noexcept {
  *pref = static_cast<int>((*attr & internal::kRwlockattrKindMask) >> internal::kRwlockattrKindShift);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_rwlockattr_setkind_np)(guest::PthreadRwlockattr* attr, int pref) noexcept {
  switch (pref) {
    case internal::kPthreadRwlockPreferReaderNp:
    case internal::kPthreadRwlockPreferWriterNonrecursiveNp:
      *attr = (*attr & ~internal::kRwlockattrKindMask) |
              (static_cast<guest::PthreadRwlockattr>(pref) << internal::kRwlockattrKindShift);
      return 0;
    default: return guest::kEINVAL;
  }
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_init)(guest::PthreadRwlock* rwlock_interface,
                                           const guest::PthreadRwlockattr* attr) noexcept {
  RwlockInternal* rwlock = internal::Internal(rwlock_interface);
  std::memset(rwlock, 0, sizeof(*rwlock));
  if (attr != nullptr) {
    rwlock->pshared = (*attr & internal::kRwlockattrPsharedMask) != 0;
    switch ((*attr & internal::kRwlockattrKindMask) >> internal::kRwlockattrKindShift) {
      case internal::kPthreadRwlockPreferReaderNp: rwlock->writer_nonrecursive_preferred = false; break;
      case internal::kPthreadRwlockPreferWriterNonrecursiveNp: rwlock->writer_nonrecursive_preferred = true; break;
      default: return guest::kEINVAL;
    }
    if ((*attr & internal::kRwlockattrReservedMask) != 0) return guest::kEINVAL;
  }
  __atomic_store_n(&rwlock->state, 0, __ATOMIC_RELAXED);
  internal::LockInit(&rwlock->pending_lock, rwlock->pshared);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_destroy)(guest::PthreadRwlock* rwlock_interface) noexcept {
  if (__atomic_load_n(&internal::Internal(rwlock_interface)->state, __ATOMIC_RELAXED) != 0) return guest::kEBUSY;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_rdlock)(guest::PthreadRwlock* rwlock_interface) noexcept {
  RwlockInternal* rwlock = internal::Internal(rwlock_interface);
  if (internal::TryRdlock(rwlock) == 0) return 0;
  return internal::TimedRdlock(rwlock, false, nullptr);
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_timedrdlock)(guest::PthreadRwlock* rwlock, const guest::Timespec* deadline) noexcept {
  return internal::TimedRdlock(internal::Internal(rwlock), true, deadline);
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_timedrdlock_monotonic_np)(guest::PthreadRwlock* rwlock,
                                                               const guest::Timespec* deadline) noexcept {
  return internal::TimedRdlock(internal::Internal(rwlock), false, deadline);
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_clockrdlock)(guest::PthreadRwlock* rwlock, elixir::abi::g_clockid clock,
                                                  const guest::Timespec* deadline) noexcept {
  switch (clock) {
    case guest::kCLOCK_MONOTONIC: return internal::TimedRdlock(internal::Internal(rwlock), false, deadline);
    case guest::kCLOCK_REALTIME: return internal::TimedRdlock(internal::Internal(rwlock), true, deadline);
    default: return guest::kEINVAL;
  }
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_tryrdlock)(guest::PthreadRwlock* rwlock) noexcept {
  return internal::TryRdlock(internal::Internal(rwlock));
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_wrlock)(guest::PthreadRwlock* rwlock_interface) noexcept {
  RwlockInternal* rwlock = internal::Internal(rwlock_interface);
  if (internal::TryWrlock(rwlock) == 0) return 0;
  return internal::TimedWrlock(rwlock, false, nullptr);
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_timedwrlock)(guest::PthreadRwlock* rwlock, const guest::Timespec* deadline) noexcept {
  return internal::TimedWrlock(internal::Internal(rwlock), true, deadline);
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_timedwrlock_monotonic_np)(guest::PthreadRwlock* rwlock,
                                                               const guest::Timespec* deadline) noexcept {
  return internal::TimedWrlock(internal::Internal(rwlock), false, deadline);
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_clockwrlock)(guest::PthreadRwlock* rwlock, elixir::abi::g_clockid clock,
                                                  const guest::Timespec* deadline) noexcept {
  switch (clock) {
    case guest::kCLOCK_MONOTONIC: return internal::TimedWrlock(internal::Internal(rwlock), false, deadline);
    case guest::kCLOCK_REALTIME: return internal::TimedWrlock(internal::Internal(rwlock), true, deadline);
    default: return guest::kEINVAL;
  }
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_trywrlock)(guest::PthreadRwlock* rwlock) noexcept {
  return internal::TryWrlock(internal::Internal(rwlock));
}

ELIXIR_BIONIC_FN(int, pthread_rwlock_unlock)(guest::PthreadRwlock* rwlock_interface) noexcept {
  RwlockInternal* rwlock = internal::Internal(rwlock_interface);
  std::int32_t old_state = __atomic_load_n(&rwlock->state, __ATOMIC_RELAXED);
  if (internal::StateOwnedByWriter(old_state)) {
    if (__atomic_load_n(&rwlock->writer_tid, __ATOMIC_RELAXED) != elixir::bionic::threads::CurrentTid()) {
      return guest::kEPERM;
    }
    __atomic_store_n(&rwlock->writer_tid, 0, __ATOMIC_RELAXED);
    old_state = __atomic_fetch_and(&rwlock->state, ~internal::kStateOwnedByWriterFlag, __ATOMIC_RELEASE);
    if (!internal::StateHavePendingReadersOrWriters(old_state)) return 0;
  } else if (internal::StateOwnedByReaders(old_state)) {
    old_state = __atomic_fetch_sub(&rwlock->state, internal::kStateReaderCountChangeStep, __ATOMIC_RELEASE);
    if (!internal::StateIsLastReader(old_state) || !internal::StateHavePendingReadersOrWriters(old_state)) return 0;
  } else {
    return guest::kEPERM;
  }

  internal::LockAcquire(&rwlock->pending_lock);
  if (rwlock->pending_writer_count != 0) {
    rwlock->pending_writer_wakeup_serial++;
    internal::LockRelease(&rwlock->pending_lock);
    internal::FutexWakeWord(&rwlock->pending_writer_wakeup_serial, false);
  } else if (rwlock->pending_reader_count != 0) {
    rwlock->pending_reader_wakeup_serial++;
    internal::LockRelease(&rwlock->pending_lock);
    internal::FutexWakeWord(&rwlock->pending_reader_wakeup_serial, true);
  } else {
    internal::LockRelease(&rwlock->pending_lock);
  }
  return 0;
}

namespace elixir::bionic::threads::internal {

void RegisterRwlockExports(ExportTable& table) {
#define ELIXIR_THREADS_EXPORT(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_THREADS_EXPORT(pthread_rwlockattr_init);
  ELIXIR_THREADS_EXPORT(pthread_rwlockattr_destroy);
  ELIXIR_THREADS_EXPORT(pthread_rwlockattr_getpshared);
  ELIXIR_THREADS_EXPORT(pthread_rwlockattr_setpshared);
  ELIXIR_THREADS_EXPORT(pthread_rwlockattr_getkind_np);
  ELIXIR_THREADS_EXPORT(pthread_rwlockattr_setkind_np);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_init);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_destroy);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_rdlock);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_tryrdlock);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_timedrdlock);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_timedrdlock_monotonic_np);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_clockrdlock);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_wrlock);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_trywrlock);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_timedwrlock);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_timedwrlock_monotonic_np);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_clockwrlock);
  ELIXIR_THREADS_EXPORT(pthread_rwlock_unlock);
#undef ELIXIR_THREADS_EXPORT
}

}
