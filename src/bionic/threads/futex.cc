// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// GuestLock is a port of bionic's libc/private/bionic_lock.h (Copyright (C)
// 2015 The Android Open Source Project, BSD-2-Clause; see the notice in
// pthread_mutex.cc).
#include <windows.h>

#include <cstdio>

#include "bionic/threads/thread_internal.h"
#include "bionic/wake_trace.h"

namespace elixir::bionic::threads {
namespace {

const DWORD g_main_windows_tid = GetCurrentThreadId();
const DWORD g_process_id = GetCurrentProcessId();

std::int64_t QpcFrequency() noexcept {
  static const std::int64_t frequency = [] {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    return static_cast<std::int64_t>(value.QuadPart);
  }();
  return frequency;
}

constexpr std::int64_t kNsPerSecond = 1000000000;
constexpr std::int64_t kUnixEpochIn100ns = 116444736000000000;
constexpr std::int64_t kMaxTimeoutNs = INT64_MAX / 4;

}

std::int32_t CurrentTid() noexcept {
  const DWORD id = GetCurrentThreadId();
  return static_cast<std::int32_t>(id == g_main_windows_tid ? g_process_id : id);
}

std::uint32_t WindowsThreadIdFromTid(std::int32_t tid) noexcept { return internal::WindowsTidFromGuestTid(tid); }

std::int32_t TidFromWindowsThreadId(std::uint32_t windows_thread_id) noexcept {
  return static_cast<std::int32_t>(windows_thread_id == g_main_windows_tid ? g_process_id : windows_thread_id);
}

int FutexWait(volatile std::int32_t* word, std::int32_t expected, std::int64_t timeout_ns) noexcept {
  if (__atomic_load_n(word, __ATOMIC_SEQ_CST) != expected) return guest::kEAGAIN;
  const std::int64_t traced_from = wake_trace::Enabled() ? wake_trace::NowNs() : 0;
  if (timeout_ns < 0) {
    WaitOnAddress(word, &expected, sizeof(expected), INFINITE);
    if (traced_from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kWoken, word, traced_from);
    return 0;
  }
  if (timeout_ns == 0) return guest::kETIMEDOUT;
  if (timeout_ns > kMaxTimeoutNs) timeout_ns = kMaxTimeoutNs;
  const std::int64_t deadline = internal::MonotonicNowNs() + timeout_ns;
  for (;;) {
    const std::int64_t remaining = deadline - internal::MonotonicNowNs();
    if (remaining <= 0) {
      if (traced_from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kTimedOut, word, traced_from);
      return guest::kETIMEDOUT;
    }
    const std::uint64_t ms = (static_cast<std::uint64_t>(remaining) + 999999) / 1000000;
    const DWORD wait_ms = ms >= INFINITE ? INFINITE - 1 : static_cast<DWORD>(ms);
    if (WaitOnAddress(word, &expected, sizeof(expected), wait_ms) || GetLastError() != ERROR_TIMEOUT) {
      if (traced_from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kWoken, word, traced_from);
      return 0;
    }
  }
}

void FutexWake(volatile std::int32_t* word, bool all) noexcept {
  wake_trace::RecordWake(word);
  if (all) {
    WakeByAddressAll(const_cast<std::int32_t*>(word));
  } else {
    WakeByAddressSingle(const_cast<std::int32_t*>(word));
  }
}

namespace internal {

std::uint32_t WindowsTidFromGuestTid(std::int32_t tid) noexcept {
  if (tid == 0) return GetCurrentThreadId();
  if (static_cast<DWORD>(tid) == g_process_id) return g_main_windows_tid;
  return static_cast<std::uint32_t>(tid);
}

std::int64_t MonotonicNowNs() noexcept {
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  const std::int64_t frequency = QpcFrequency();
  const std::int64_t ticks = counter.QuadPart;
  return (ticks / frequency) * kNsPerSecond + (ticks % frequency) * kNsPerSecond / frequency;
}

std::int64_t RealtimeNowNs() noexcept {
  FILETIME now;
  GetSystemTimePreciseAsFileTime(&now);
  const std::int64_t value =
      (static_cast<std::int64_t>(now.dwHighDateTime) << 32) | static_cast<std::int64_t>(now.dwLowDateTime);
  return (value - kUnixEpochIn100ns) * 100;
}

int CheckTimespec(const guest::Timespec* ts, bool null_allowed) noexcept {
  if (null_allowed && ts == nullptr) return 0;
  if (ts == nullptr) return guest::kEINVAL;
  if (ts->tv_nsec < 0 || ts->tv_nsec >= kNsPerSecond) return guest::kEINVAL;
  if (ts->tv_sec < 0) return guest::kETIMEDOUT;
  return 0;
}

std::int64_t RelativeTimeoutNs(const guest::Timespec* deadline, bool realtime) noexcept {
  if (deadline == nullptr) return -1;
  if (deadline->tv_sec < 0) return 0;
  if (deadline->tv_sec >= kMaxTimeoutNs / kNsPerSecond) return kMaxTimeoutNs;
  const std::int64_t target = deadline->tv_sec * kNsPerSecond + deadline->tv_nsec;
  const std::int64_t now = realtime ? RealtimeNowNs() : MonotonicNowNs();
  const std::int64_t remaining = target - now;
  return remaining > 0 ? remaining : 0;
}

int FutexWaitAbsolute(volatile void* word, std::uint32_t expected, bool realtime,
                      const guest::Timespec* deadline) noexcept {
  const std::int64_t timeout = RelativeTimeoutNs(deadline, realtime);
  return FutexWait(static_cast<volatile std::int32_t*>(word), static_cast<std::int32_t>(expected), timeout);
}

void SleepNs(std::int64_t ns) noexcept {
  if (ns <= 0) return;
  HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                        TIMER_ALL_ACCESS);
  if (timer != nullptr) {
    LARGE_INTEGER due;
    due.QuadPart = -((ns + 99) / 100);
    if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
      WaitForSingleObject(timer, INFINITE);
      CloseHandle(timer);
      return;
    }
    CloseHandle(timer);
  }
  const std::int64_t deadline = MonotonicNowNs() + ns;
  for (;;) {
    const std::int64_t remaining = deadline - MonotonicNowNs();
    if (remaining <= 0) return;
    Sleep(static_cast<DWORD>((remaining + 999999) / 1000000));
  }
}

namespace {
constexpr std::int32_t kLockUnlocked = 0;
constexpr std::int32_t kLockLockedWithoutWaiter = 1;
constexpr std::int32_t kLockLockedWithWaiter = 2;
}

void LockInit(GuestLock* lock, bool process_shared) noexcept {
  __atomic_store_n(&lock->state, kLockUnlocked, __ATOMIC_RELAXED);
  lock->process_shared = process_shared;
}

bool LockTry(GuestLock* lock) noexcept {
  std::int32_t expected = kLockUnlocked;
  return __atomic_compare_exchange_n(&lock->state, &expected, kLockLockedWithoutWaiter, false,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

void LockAcquire(GuestLock* lock) noexcept {
  if (LockTry(lock)) return;
  while (__atomic_exchange_n(&lock->state, kLockLockedWithWaiter, __ATOMIC_ACQUIRE) != kLockUnlocked) {
    FutexWait(&lock->state, kLockLockedWithWaiter, -1);
  }
}

void LockRelease(GuestLock* lock) noexcept {
  volatile std::int32_t* word = &lock->state;
  if (__atomic_exchange_n(&lock->state, kLockUnlocked, __ATOMIC_RELEASE) == kLockLockedWithWaiter) {
    FutexWake(word, false);
  }
}

void FatalError(const char* message) noexcept {
  HostLog(kAndroidLogFatal, "libc", message);
  TerminateProcess(GetCurrentProcess(), 128 + guest::kSIGABRT);
  __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

}
}
