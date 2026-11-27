// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <cstdint>
#include <cstring>

#include "abi/sysv_abi.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"
#include "bionic/threads.h"
#include "bionic/wake_trace.h"
#include "platform/windows/guest_tls.h"

namespace elixir::bionic {
namespace core {
namespace {

constexpr std::int64_t kNanosPerSecond = 1000000000;
constexpr std::uint64_t kUnixEpochFiletime = 116444736000000000ull;

constexpr std::int64_t kTenMegahertz = 10000000;
std::int64_t g_qpc_frequency = 0;

std::int64_t QpcFrequency() noexcept {
  std::int64_t frequency = __atomic_load_n(&g_qpc_frequency, __ATOMIC_RELAXED);
  if (__builtin_expect(frequency == 0, 0)) {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    frequency = value.QuadPart > 0 ? value.QuadPart : kTenMegahertz;
    __atomic_store_n(&g_qpc_frequency, frequency, __ATOMIC_RELAXED);
  }
  return frequency;
}

std::uint64_t FiletimeTicks(const FILETIME& time) noexcept {
  return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

std::int64_t ProcessCpuNanoseconds(HANDLE process, int which) noexcept {
  FILETIME creation, exit_time, kernel_time, user_time;
  if (!GetProcessTimes(process, &creation, &exit_time, &kernel_time, &user_time)) return -1;
  const std::uint64_t user = FiletimeTicks(user_time);
  const std::uint64_t total = which == 1 ? user : user + FiletimeTicks(kernel_time);
  return static_cast<std::int64_t>(total * 100);
}

std::int64_t ThreadCpuNanoseconds(HANDLE thread, int which) noexcept {
  FILETIME creation, exit_time, kernel_time, user_time;
  if (!GetThreadTimes(thread, &creation, &exit_time, &kernel_time, &user_time)) return -1;
  const std::uint64_t user = FiletimeTicks(user_time);
  const std::uint64_t total = which == 1 ? user : user + FiletimeTicks(kernel_time);
  return static_cast<std::int64_t>(total * 100);
}

int ReadDynamicCpuClock(std::int32_t clock_id, std::int64_t* nanoseconds) noexcept {
  const int which = clock_id & 3;
  if (which == 3) return guest::kEINVAL;
  const bool per_thread = (clock_id & 4) != 0;
  const std::int32_t id = ~(clock_id >> 3);
  if (per_thread) {
    if (id == 0 || id == CurrentTid()) {
      *nanoseconds = ThreadCpuNanoseconds(GetCurrentThread(), which);
      return *nanoseconds >= 0 ? 0 : guest::kEINVAL;
    }
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threads::WindowsThreadIdFromTid(id));
    if (thread == nullptr) return guest::kEINVAL;
    const bool ours = GetProcessIdOfThread(thread) == GetCurrentProcessId();
    *nanoseconds = ours ? ThreadCpuNanoseconds(thread, which) : -1;
    CloseHandle(thread);
    return *nanoseconds >= 0 ? 0 : guest::kEINVAL;
  }
  if (id == 0 || id == CurrentPid()) {
    *nanoseconds = ProcessCpuNanoseconds(GetCurrentProcess(), which);
    return *nanoseconds >= 0 ? 0 : guest::kEINVAL;
  }
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(id));
  if (process == nullptr) return guest::kEINVAL;
  *nanoseconds = ProcessCpuNanoseconds(process, which);
  CloseHandle(process);
  return *nanoseconds >= 0 ? 0 : guest::kEINVAL;
}

std::int64_t TimerTickNanoseconds() noexcept {
  DWORD adjustment = 0, increment = 0;
  BOOL disabled = FALSE;
  if (GetSystemTimeAdjustment(&adjustment, &increment, &disabled) && increment != 0) {
    return static_cast<std::int64_t>(increment) * 100;
  }
  return 15625000;
}

struct ThreadSleepTimer {
  HANDLE handle = nullptr;
  bool high_resolution = false;
  ~ThreadSleepTimer() {
    if (handle != nullptr) CloseHandle(handle);
  }
};
thread_local ThreadSleepTimer t_sleep_timer;

HANDLE SleepTimer() noexcept {
  ThreadSleepTimer& timer = t_sleep_timer;
  if (timer.handle == nullptr) {
    timer.handle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    timer.high_resolution = timer.handle != nullptr;
    if (timer.handle == nullptr) timer.handle = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
  }
  return timer.handle;
}

}

std::int64_t CounterToNanoseconds(std::int64_t ticks, std::int64_t frequency) noexcept {
  if (__builtin_expect(frequency == kTenMegahertz, 1)) return ticks * 100;
  const std::int64_t seconds = ticks / frequency;
  const std::int64_t remainder = ticks % frequency;
  return seconds * kNanosPerSecond + remainder * kNanosPerSecond / frequency;
}

std::int64_t MonotonicNanoseconds() noexcept {
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return CounterToNanoseconds(counter.QuadPart, QpcFrequency());
}

std::int64_t RealtimeNanoseconds() noexcept {
  FILETIME now;
  GetSystemTimePreciseAsFileTime(&now);
  return static_cast<std::int64_t>(FiletimeTicks(now) - kUnixEpochFiletime) * 100;
}

int ReadClock(std::int32_t clock_id, guest::Timespec* out) noexcept {
  using namespace guest;
  std::int64_t value = 0;
  if (clock_id < 0) {
    const int error = ReadDynamicCpuClock(clock_id, &value);
    if (error != 0) return error;
  } else {
    switch (clock_id) {
      case kCLOCK_REALTIME:
      case kCLOCK_REALTIME_COARSE:
      case kCLOCK_REALTIME_ALARM:
      case kCLOCK_TAI:
        value = RealtimeNanoseconds();
        break;
      case kCLOCK_MONOTONIC:
      case kCLOCK_MONOTONIC_RAW:
      case kCLOCK_MONOTONIC_COARSE:
      case kCLOCK_BOOTTIME:
      case kCLOCK_BOOTTIME_ALARM:
        value = MonotonicNanoseconds();
        break;
      case kCLOCK_PROCESS_CPUTIME_ID:
        value = ProcessCpuNanoseconds(GetCurrentProcess(), 2);
        break;
      case kCLOCK_THREAD_CPUTIME_ID:
        value = ThreadCpuNanoseconds(GetCurrentThread(), 2);
        break;
      default: return kEINVAL;
    }
    if (value < 0) return kEINVAL;
  }
  *out = NanosecondsToTimespec(value);
  return 0;
}

void PreciseSleep(std::int64_t nanoseconds) noexcept {
  if (nanoseconds <= 0) return;
  const std::int64_t deadline = MonotonicNanoseconds() + nanoseconds;
  HANDLE timer = SleepTimer();
  for (;;) {
    const std::int64_t remaining = deadline - MonotonicNanoseconds();
    if (remaining <= 0) break;
    if (remaining <= 50000 || timer == nullptr) {
      while (MonotonicNanoseconds() < deadline) {
        if (timer == nullptr && deadline - MonotonicNanoseconds() > 2000000) {
          Sleep(1);
        } else {
          YieldProcessor();
        }
      }
      break;
    }
    LARGE_INTEGER due;
    due.QuadPart = -(remaining / 100 > 0 ? remaining / 100 : 1);
    if (!SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0)) {
      timer = nullptr;
      continue;
    }
    WaitForSingleObject(timer, INFINITE);
  }
  platform::ResumeGuestThread();
}

}

ELIXIR_BIONIC_FN(int, clock_gettime)(std::int32_t clock_id, guest::Timespec* ts) noexcept {
  guest::Timespec value;
  const int error = core::ReadClock(clock_id, &value);
  if (error != 0) return FailWith(error, -1);
  if (ts == nullptr) return FailWith(guest::kEFAULT, -1);
  *ts = value;
  return 0;
}

ELIXIR_BIONIC_FN(int, clock_getres)(std::int32_t clock_id, guest::Timespec* ts) noexcept {
  using namespace guest;
  std::int64_t resolution = 0;
  if (clock_id < 0) {
    std::int64_t ignored = 0;
    const int error = core::ReadDynamicCpuClock(clock_id, &ignored);
    if (error != 0) return FailWith(error, -1);
    resolution = core::TimerTickNanoseconds();
  } else {
    switch (clock_id) {
      case kCLOCK_REALTIME:
      case kCLOCK_REALTIME_COARSE:
      case kCLOCK_REALTIME_ALARM:
      case kCLOCK_TAI: resolution = 100; break;
      case kCLOCK_MONOTONIC:
      case kCLOCK_MONOTONIC_RAW:
      case kCLOCK_MONOTONIC_COARSE:
      case kCLOCK_BOOTTIME:
      case kCLOCK_BOOTTIME_ALARM: {
        const std::int64_t frequency = core::QpcFrequency();
        resolution = frequency >= 1000000000 ? 1 : (1000000000 + frequency - 1) / frequency;
        break;
      }
      case kCLOCK_PROCESS_CPUTIME_ID:
      case kCLOCK_THREAD_CPUTIME_ID: resolution = core::TimerTickNanoseconds(); break;
      default: return FailWith(kEINVAL, -1);
    }
  }
  if (ts != nullptr) *ts = core::NanosecondsToTimespec(resolution);
  return 0;
}

ELIXIR_BIONIC_FN(int, clock_settime)(std::int32_t clock_id, const guest::Timespec* ts) noexcept {
  guest::Timespec ignored;
  if (core::ReadClock(clock_id, &ignored) != 0) return FailWith(guest::kEINVAL, -1);
  if (ts == nullptr) return FailWith(guest::kEFAULT, -1);
  if (!core::ValidTimespec(*ts)) return FailWith(guest::kEINVAL, -1);
  if (clock_id != guest::kCLOCK_REALTIME) return FailWith(guest::kEINVAL, -1);
  return FailWith(guest::kEPERM, -1);
}

ELIXIR_BIONIC_FN(int, clock_getcpuclockid)(abi::g_pid pid, std::int32_t* clock_id) noexcept {
  if (pid != 0 && pid != core::CurrentPid()) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) return GetLastError() == ERROR_ACCESS_DENIED ? guest::kEPERM : guest::kESRCH;
    CloseHandle(process);
  }
  if (clock_id == nullptr) return guest::kEFAULT;
  const std::int32_t target = pid == 0 ? core::CurrentPid() : pid;
  *clock_id = static_cast<std::int32_t>((static_cast<std::uint32_t>(~target) << 3) | 2u);
  return 0;
}

ELIXIR_BIONIC_FN(int, gettimeofday)(guest::Timeval* tv, guest::Timezone* tz) noexcept {
  if (tv != nullptr) {
    const std::int64_t now = core::RealtimeNanoseconds();
    tv->tv_sec = now / 1000000000;
    tv->tv_usec = (now % 1000000000) / 1000;
  }
  if (tz != nullptr) {
    tz->tz_minuteswest = 0;
    tz->tz_dsttime = 0;
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, settimeofday)(const guest::Timeval* tv, const guest::Timezone*) noexcept {
  if (tv != nullptr && (tv->tv_usec < 0 || tv->tv_usec >= 1000000)) return FailWith(guest::kEINVAL, -1);
  return FailWith(guest::kEPERM, -1);
}

ELIXIR_BIONIC_FN(abi::g_time, time)(abi::g_time* out) noexcept {
  const abi::g_time now = core::RealtimeNanoseconds() / 1000000000;
  if (out != nullptr) *out = now;
  return now;
}

ELIXIR_BIONIC_FN(abi::g_clock, clock)() noexcept {
  const std::int64_t cpu = core::ProcessCpuNanoseconds(GetCurrentProcess(), 2);
  if (cpu < 0) return -1;
  return cpu / 1000;
}

ELIXIR_BIONIC_FN(abi::g_clock, times)(guest::Tms* out) noexcept {
  if (out != nullptr) {
    FILETIME creation, exit_time, kernel_time, user_time;
    guest::Tms value{};
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit_time, &kernel_time, &user_time)) {
      value.tms_utime = static_cast<abi::g_long>(core::FiletimeTicks(user_time) / 100000);
      value.tms_stime = static_cast<abi::g_long>(core::FiletimeTicks(kernel_time) / 100000);
    }
    *out = value;
  }
  return static_cast<abi::g_clock>(GetTickCount64() / 10);
}

ELIXIR_BIONIC_FN(double, difftime)(abi::g_time end, abi::g_time start) noexcept {
  if ((end >= 0) == (start >= 0)) return static_cast<double>(end - start);
  return static_cast<double>(end) - static_cast<double>(start);
}

ELIXIR_BIONIC_FN(int, timespec_get)(guest::Timespec* ts, int base) noexcept {
  constexpr int kTimeUtc = 1, kTimeMonotonic = 2, kTimeActive = 3, kTimeThreadActive = 4;
  std::int32_t clock_id;
  switch (base) {
    case kTimeUtc: clock_id = guest::kCLOCK_REALTIME; break;
    case kTimeMonotonic: clock_id = guest::kCLOCK_MONOTONIC; break;
    case kTimeActive: clock_id = guest::kCLOCK_PROCESS_CPUTIME_ID; break;
    case kTimeThreadActive: clock_id = guest::kCLOCK_THREAD_CPUTIME_ID; break;
    default: return 0;
  }
  if (ts == nullptr || core::ReadClock(clock_id, ts) != 0) return 0;
  return base;
}

ELIXIR_BIONIC_FN(int, timespec_getres)(guest::Timespec* ts, int base) noexcept {
  constexpr int kTimeUtc = 1, kTimeMonotonic = 2, kTimeActive = 3, kTimeThreadActive = 4;
  std::int32_t clock_id;
  switch (base) {
    case kTimeUtc: clock_id = guest::kCLOCK_REALTIME; break;
    case kTimeMonotonic: clock_id = guest::kCLOCK_MONOTONIC; break;
    case kTimeActive: clock_id = guest::kCLOCK_PROCESS_CPUTIME_ID; break;
    case kTimeThreadActive: clock_id = guest::kCLOCK_THREAD_CPUTIME_ID; break;
    default: return 0;
  }
  if (elixir_bionic_clock_getres(clock_id, ts) != 0) return 0;
  return base;
}

ELIXIR_BIONIC_FN(int, nanosleep)(const guest::Timespec* request, guest::Timespec* remain) noexcept {
  if (request == nullptr) return FailWith(guest::kEFAULT, -1);
  const guest::Timespec copy = *request;
  if (!core::ValidTimespec(copy)) return FailWith(guest::kEINVAL, -1);
  const std::int64_t traced_from = wake_trace::Enabled() ? wake_trace::NowNs() : 0;
  core::PreciseSleep(core::TimespecToNanoseconds(copy));
  if (traced_from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kSlept, nullptr, traced_from);
  (void)remain;
  return 0;
}

ELIXIR_BIONIC_FN(int, clock_nanosleep)(std::int32_t clock_id, int flags, const guest::Timespec* request,
                                       guest::Timespec* remain) noexcept {
  using namespace guest;
  (void)remain;
  bool realtime = false;
  switch (clock_id) {
    case kCLOCK_REALTIME:
    case kCLOCK_TAI: realtime = true; break;
    case kCLOCK_MONOTONIC:
    case kCLOCK_BOOTTIME: break;
    case kCLOCK_REALTIME_ALARM:
    case kCLOCK_BOOTTIME_ALARM: return kEPERM;
    case kCLOCK_PROCESS_CPUTIME_ID: return kENOTSUP;
    case kCLOCK_THREAD_CPUTIME_ID: return kEINVAL;
    default: return kEINVAL;
  }
  if (request == nullptr) return kEFAULT;
  const guest::Timespec copy = *request;
  if (!core::ValidTimespec(copy)) return kEINVAL;
  std::int64_t duration = core::TimespecToNanoseconds(copy);
  if ((flags & kTIMER_ABSTIME) != 0) {
    const std::int64_t now = realtime ? core::RealtimeNanoseconds() : core::MonotonicNanoseconds();
    duration -= now;
    if (duration <= 0) return 0;
  }
  const std::int64_t traced_from = wake_trace::Enabled() ? wake_trace::NowNs() : 0;
  core::PreciseSleep(duration);
  if (traced_from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kSlept, nullptr, traced_from);
  return 0;
}

ELIXIR_BIONIC_FN(int, usleep)(std::uint32_t microseconds) noexcept {
  const std::int64_t traced_from = wake_trace::Enabled() ? wake_trace::NowNs() : 0;
  core::PreciseSleep(static_cast<std::int64_t>(microseconds) * 1000);
  if (traced_from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kSlept, nullptr, traced_from);
  return 0;
}

ELIXIR_BIONIC_FN(unsigned, sleep)(unsigned seconds) noexcept {
  core::PreciseSleep(static_cast<std::int64_t>(seconds) * 1000000000);
  return 0;
}

void RegisterTimeExports(ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(clock_gettime);
  ELIXIR_ADD(clock_getres);
  ELIXIR_ADD(clock_settime);
  ELIXIR_ADD(clock_getcpuclockid);
  ELIXIR_ADD(clock_nanosleep);
  ELIXIR_ADD(nanosleep);
  ELIXIR_ADD(usleep);
  ELIXIR_ADD(sleep);
  ELIXIR_ADD(gettimeofday);
  ELIXIR_ADD(settimeofday);
  ELIXIR_ADD(time);
  ELIXIR_ADD(times);
  ELIXIR_ADD(clock);
  ELIXIR_ADD(difftime);
  ELIXIR_ADD(timespec_get);
  ELIXIR_ADD(timespec_getres);
#undef ELIXIR_ADD
  core::RegisterTimeConversionExports(table);
  core::RegisterTimerExports(table);
}

}
