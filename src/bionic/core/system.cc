// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <bcrypt.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "abi/sysv_abi.h"
#include "bionic/core/core.h"
#include "bionic/core/core_internal.h"
#include "bionic/core/process_ext.h"
#include "bionic/runtime.h"
#include "bionic/threads.h"
#include "bionic/wake_trace.h"
#include "platform/windows/guest_tls.h"

namespace elixir::bionic {

ELIXIR_BIONIC_FN(int, clock_gettime)(std::int32_t clock_id, guest::Timespec* ts) noexcept;
ELIXIR_BIONIC_FN(int, clock_getres)(std::int32_t clock_id, guest::Timespec* ts) noexcept;
ELIXIR_BIONIC_FN(int, clock_settime)(std::int32_t clock_id, const guest::Timespec* ts) noexcept;
ELIXIR_BIONIC_FN(int, clock_nanosleep)(std::int32_t clock_id, int flags, const guest::Timespec* request,
                                       guest::Timespec* remain) noexcept;
ELIXIR_BIONIC_FN(int, nanosleep)(const guest::Timespec* request, guest::Timespec* remain) noexcept;
ELIXIR_BIONIC_FN(int, gettimeofday)(guest::Timeval* tv, guest::Timezone* tz) noexcept;
ELIXIR_BIONIC_FN(abi::g_time, time)(abi::g_time* out) noexcept;
ELIXIR_BIONIC_FN(int, kill)(abi::g_pid pid, int sig) noexcept;
ELIXIR_BIONIC_FN(int, tgkill)(abi::g_pid tgid, abi::g_pid tid, int sig) noexcept;
ELIXIR_BIONIC_FN(int, sigaction)(int sig, const guest::Sigaction* action, guest::Sigaction* old) noexcept;
ELIXIR_BIONIC_FN(int, sigprocmask)(int how, const guest::Sigset* set, guest::Sigset* old) noexcept;
ELIXIR_BIONIC_FN(int, sigpending)(guest::Sigset* set) noexcept;
ELIXIR_BIONIC_FN(int, sigsuspend)(const guest::Sigset* mask) noexcept;
ELIXIR_BIONIC_FN(int, sigaltstack)(const guest::StackT* stack, guest::StackT* old) noexcept;
ELIXIR_BIONIC_FN(void, _exit)(int status) noexcept;

namespace core {
namespace {

constexpr std::uint64_t kRlimInfinity = ~std::uint64_t{0};
constexpr int kRlimitCount = 16;

SRWLOCK g_identity_lock = SRWLOCK_INIT;
GuestIdentity* g_identity = nullptr;

SRWLOCK g_rlimit_lock = SRWLOCK_INIT;
guest::Rlimit g_rlimits[kRlimitCount] = {
    {kRlimInfinity, kRlimInfinity},
    {kRlimInfinity, kRlimInfinity},
    {kRlimInfinity, kRlimInfinity},
    {8u << 20, kRlimInfinity},
    {0, kRlimInfinity},
    {kRlimInfinity, kRlimInfinity},
    {32768, 32768},
    {32768, 32768},
    {65536, 65536},
    {kRlimInfinity, kRlimInfinity},
    {kRlimInfinity, kRlimInfinity},
    {32768, 32768},
    {819200, 819200},
    {40, 40},
    {0, 0},
    {kRlimInfinity, kRlimInfinity},
};
constexpr std::uint64_t kNrOpen = 1048576;

guest::Rlimit LimitOf(int resource) noexcept {
  AcquireSRWLockShared(&g_rlimit_lock);
  const guest::Rlimit value = g_rlimits[resource];
  ReleaseSRWLockShared(&g_rlimit_lock);
  return value;
}

int DoPrlimit(abi::g_pid pid, int resource, const guest::Rlimit* new_limit, guest::Rlimit* old_limit) noexcept {
  if (resource < 0 || resource >= kRlimitCount) return guest::kEINVAL;
  if (pid != 0 && pid != CurrentPid()) return guest::kESRCH;
  guest::Rlimit incoming{};
  if (new_limit != nullptr) {
    std::memcpy(&incoming, new_limit, sizeof(incoming));
    if (incoming.rlim_cur > incoming.rlim_max) return guest::kEINVAL;
    if (resource == guest::kRLIMIT_NOFILE && incoming.rlim_max != kRlimInfinity && incoming.rlim_max > kNrOpen) {
      return guest::kEPERM;
    }
  }
  AcquireSRWLockExclusive(&g_rlimit_lock);
  const guest::Rlimit current = g_rlimits[resource];
  if (new_limit != nullptr) {
    if (incoming.rlim_max > current.rlim_max) {
      ReleaseSRWLockExclusive(&g_rlimit_lock);
      return guest::kEPERM;
    }
    g_rlimits[resource] = incoming;
  }
  ReleaseSRWLockExclusive(&g_rlimit_lock);
  if (old_limit != nullptr) std::memcpy(old_limit, &current, sizeof(current));
  return 0;
}

MEMORYSTATUSEX MemoryStatus() noexcept {
  MEMORYSTATUSEX status{};
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  return status;
}

struct CacheGeometry {
  abi::g_long size = 0, assoc = 0, linesize = 0;
};
struct CacheInfo {
  CacheGeometry l1i, l1d, l2, l3, l4;
};

const CacheInfo& Caches() noexcept {
  static const CacheInfo info = [] {
    CacheInfo result;
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &length);
    std::vector<unsigned char> buffer(length);
    if (length == 0 ||
        !GetLogicalProcessorInformationEx(
            RelationCache, reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()), &length)) {
      return result;
    }
    for (DWORD offset = 0; offset < length;) {
      auto* entry = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
      if (entry->Relationship == RelationCache) {
        const CACHE_RELATIONSHIP& cache = entry->Cache;
        CacheGeometry geometry;
        geometry.size = cache.CacheSize;
        geometry.assoc = cache.Associativity == CACHE_FULLY_ASSOCIATIVE ? 0 : cache.Associativity;
        geometry.linesize = cache.LineSize;
        CacheGeometry* slot = nullptr;
        switch (cache.Level) {
          case 1: slot = cache.Type == CacheInstruction ? &result.l1i : (cache.Type == CacheData || cache.Type == CacheUnified ? &result.l1d : nullptr); break;
          case 2: slot = &result.l2; break;
          case 3: slot = &result.l3; break;
          case 4: slot = &result.l4; break;
          default: break;
        }
        if (slot != nullptr && slot->size == 0) *slot = geometry;
      }
      if (entry->Size == 0) break;
      offset += entry->Size;
    }
    return result;
  }();
  return info;
}

abi::g_long ProcessorsConfigured() noexcept {
  const DWORD count = GetMaximumProcessorCount(ALL_PROCESSOR_GROUPS);
  return count != 0 ? count : 1;
}
abi::g_long ProcessorsOnline() noexcept {
  const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  return count != 0 ? count : 1;
}

SRWLOCK g_load_lock = SRWLOCK_INIT;
bool g_load_initialized = false;
std::uint64_t g_load_idle = 0, g_load_total = 0, g_load_stamp_ns = 0;
double g_load_avg[3] = {0, 0, 0};

std::uint64_t FileTimeValue(const FILETIME& time) noexcept {
  return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

void SampleLoad(double out[3]) noexcept {
  FILETIME idle_time, kernel_time, user_time;
  if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
    out[0] = out[1] = out[2] = 0;
    return;
  }
  const std::uint64_t idle = FileTimeValue(idle_time);
  const std::uint64_t total = FileTimeValue(kernel_time) + FileTimeValue(user_time);
  const auto now = static_cast<std::uint64_t>(MonotonicNanoseconds());
  const double cpus = static_cast<double>(ProcessorsOnline());
  AcquireSRWLockExclusive(&g_load_lock);
  if (!g_load_initialized) {
    const double busy = total > 0 ? 1.0 - static_cast<double>(idle) / static_cast<double>(total) : 0.0;
    for (double& value : g_load_avg) value = busy * cpus;
    g_load_initialized = true;
    g_load_idle = idle;
    g_load_total = total;
    g_load_stamp_ns = now;
  } else if (now - g_load_stamp_ns >= 100000000ull && total > g_load_total) {
    const double busy =
        1.0 - static_cast<double>(idle - g_load_idle) / static_cast<double>(total - g_load_total);
    const double instant = (busy < 0 ? 0 : busy) * cpus;
    const double elapsed = static_cast<double>(now - g_load_stamp_ns) / 1e9;
    static const double kPeriods[3] = {60.0, 300.0, 900.0};
    for (int i = 0; i < 3; ++i) {
      const double decay = std::exp(-elapsed / kPeriods[i]);
      g_load_avg[i] = g_load_avg[i] * decay + instant * (1.0 - decay);
    }
    g_load_idle = idle;
    g_load_total = total;
    g_load_stamp_ns = now;
  }
  for (int i = 0; i < 3; ++i) out[i] = g_load_avg[i];
  ReleaseSRWLockExclusive(&g_load_lock);
}

std::string WideToUtf8String(const wchar_t* text) {
  if (text == nullptr || *text == 0) return {};
  const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (length <= 1) return {};
  std::string out(static_cast<std::size_t>(length - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), length, nullptr, nullptr);
  return out;
}

std::wstring Utf8ToWideString(const char* text, std::size_t length) {
  if (length == 0) return {};
  const int count = MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(length), nullptr, 0);
  if (count <= 0) return {};
  std::wstring out(static_cast<std::size_t>(count), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(length), out.data(), count);
  return out;
}

std::string HostName() {
  wchar_t buffer[256] = {};
  DWORD size = 256;
  if (!GetComputerNameExW(ComputerNameDnsHostname, buffer, &size) || size == 0) return "localhost";
  std::string name = WideToUtf8String(buffer);
  if (name.size() > 64) name.resize(64);
  return name.empty() ? std::string("localhost") : name;
}

void CopyField(char* field, const std::string& value) noexcept {
  const std::size_t length = std::min<std::size_t>(value.size(), 64);
  std::memcpy(field, value.data(), length);
  field[length] = '\0';
}

thread_local char t_thread_name[16];
thread_local bool t_thread_name_set = false;
thread_local int t_nice = 0;
thread_local bool t_nice_set = false;
std::atomic<int> g_process_nice{0};
std::atomic<int> g_dumpable{1};
std::atomic<int> g_keepcaps{0};
std::atomic<int> g_no_new_privs{0};
std::atomic<int> g_child_subreaper{0};
std::atomic<int> g_thp_disable{0};
thread_local int t_pdeathsig = 0;
thread_local std::uint64_t t_timerslack_ns = 50000;

int NiceToWindowsPriority(int nice) noexcept {
  if (nice <= -15) return THREAD_PRIORITY_HIGHEST;
  if (nice <= -5) return THREAD_PRIORITY_ABOVE_NORMAL;
  if (nice < 5) return THREAD_PRIORITY_NORMAL;
  if (nice < 15) return THREAD_PRIORITY_BELOW_NORMAL;
  if (nice < 19) return THREAD_PRIORITY_LOWEST;
  return THREAD_PRIORITY_IDLE;
}

int WindowsPriorityToNice(int priority) noexcept {
  switch (priority) {
    case THREAD_PRIORITY_TIME_CRITICAL: return -20;
    case THREAD_PRIORITY_HIGHEST: return -16;
    case THREAD_PRIORITY_ABOVE_NORMAL: return -8;
    case THREAD_PRIORITY_NORMAL: return 0;
    case THREAD_PRIORITY_BELOW_NORMAL: return 10;
    case THREAD_PRIORITY_LOWEST: return 16;
    case THREAD_PRIORITY_IDLE: return 19;
    default: return 0;
  }
}

constexpr abi::g_long kSysRead = 0, kSysWrite = 1, kSysRtSigaction = 13, kSysRtSigprocmask = 14,
                      kSysSchedYield = 24, kSysNanosleep = 35, kSysGetpid = 39, kSysExit = 60, kSysKill = 62,
                      kSysUname = 63, kSysGettimeofday = 96, kSysGetrlimit = 97, kSysGetrusage = 98,
                      kSysSysinfo = 99, kSysGetuid = 102, kSysGetgid = 104, kSysGeteuid = 107,
                      kSysGetegid = 108, kSysGetppid = 110, kSysRtSigpending = 127, kSysRtSigsuspend = 130,
                      kSysSigaltstack = 131, kSysGetpriority = 140, kSysSetpriority = 141, kSysPrctl = 157,
                      kSysArchPrctl = 158, kSysSetrlimit = 160, kSysGettid = 186, kSysTkill = 200,
                      kSysTime = 201, kSysFutex = 202, kSysSchedGetaffinity = 204, kSysClockSettime = 227,
                      kSysClockGettime = 228, kSysClockGetres = 229, kSysClockNanosleep = 230,
                      kSysExitGroup = 231, kSysTgkill = 234, kSysPrlimit64 = 302, kSysGetcpu = 309,
                      kSysGetrandom = 318, kSysMembarrier = 324;

struct KernelSigaction {
  void* handler;
  std::uint64_t flags;
  void* restorer;
  guest::Sigset mask;
};


abi::g_long ErrnoResult(int error) noexcept { return FailWith(error, abi::g_long{-1}); }

struct FutexCounters {
  std::atomic<std::uint64_t> waits{0};
  std::atomic<std::uint64_t> timed_waits{0};
  std::atomic<std::uint64_t> masked_waits{0};
  std::atomic<std::uint64_t> long_waits{0};
  std::atomic<std::uint64_t> long_timeouts{0};
  std::atomic<std::uint64_t> wakes{0};
  std::atomic<std::uint64_t> masked_wakes{0};
  std::atomic<std::uint64_t> requeues{0};
  std::atomic<std::uint64_t> wake_ops{0};
};
FutexCounters g_futex;

abi::g_long Futex(volatile std::int32_t* word, int op, std::int32_t value, abi::g_long timeout_or_value2,
                  volatile std::int32_t* word2, std::int32_t value3, std::uintptr_t site) noexcept {
  constexpr int kPrivate = 128, kClockRealtime = 256;
  constexpr int kWait = 0, kWake = 1, kRequeue = 3, kCmpRequeue = 4, kWakeOp = 5, kWaitBitset = 9,
                kWakeBitset = 10;
  constexpr std::uint32_t kMatchAny = 0xffffffffu;
  const int command = op & ~(kPrivate | kClockRealtime);
  if (word == nullptr || (reinterpret_cast<std::uintptr_t>(word) & 3) != 0) return ErrnoResult(guest::kEINVAL);
  switch (command) {
    case kWait:
    case kWaitBitset: {
      if (command == kWaitBitset && value3 == 0) return ErrnoResult(guest::kEINVAL);
      const auto* timeout = reinterpret_cast<const guest::Timespec*>(timeout_or_value2);
      std::int64_t remaining_ns = -1;
      if (timeout != nullptr) {
        if (!ValidTimespec(*timeout)) return ErrnoResult(guest::kEINVAL);
        if (command == kWait) {
          remaining_ns = TimespecToNanoseconds(*timeout);
        } else {
          const std::int64_t now = (op & kClockRealtime) != 0 ? RealtimeNanoseconds() : MonotonicNanoseconds();
          remaining_ns = TimespecToNanoseconds(*timeout) - now;
          if (remaining_ns < 0) remaining_ns = 0;
        }
      }
      if (*word != value) return ErrnoResult(guest::kEAGAIN);
      DWORD wait_ms = INFINITE;
      if (remaining_ns >= 0) {
        const std::int64_t ms = (remaining_ns + 999999) / 1000000;
        wait_ms = ms >= 0x7fffffff ? 0x7fffffff : static_cast<DWORD>(ms);
      }
      g_futex.waits.fetch_add(1, std::memory_order_relaxed);
      if (timeout != nullptr) g_futex.timed_waits.fetch_add(1, std::memory_order_relaxed);
      if (command == kWaitBitset && static_cast<std::uint32_t>(value3) != kMatchAny) {
        g_futex.masked_waits.fetch_add(1, std::memory_order_relaxed);
      }
      const std::int64_t started = MonotonicNanoseconds();
      std::int32_t expected = value;
      const bool woken = WaitOnAddress(const_cast<std::int32_t*>(word), &expected, sizeof(expected), wait_ms) != FALSE;
      const bool timed_out = !woken && GetLastError() == ERROR_TIMEOUT;
      const std::int64_t waited = MonotonicNanoseconds() - started;
      if (waited >= 30000000) {
        g_futex.long_waits.fetch_add(1, std::memory_order_relaxed);
        if (timed_out) g_futex.long_timeouts.fetch_add(1, std::memory_order_relaxed);
      }
      if (wake_trace::Enabled() && waited >= wake_trace::kRecordWaitsFromNs) {
        wake_trace::Record(timed_out ? wake_trace::Event::kTimedOut : wake_trace::Event::kWoken, word, site, waited);
      }
      if (timed_out) return ErrnoResult(guest::kETIMEDOUT);
      return 0;
    }
    case kWake:
    case kWakeBitset:
      if (command == kWakeBitset && value3 == 0) return ErrnoResult(guest::kEINVAL);
      g_futex.wakes.fetch_add(1, std::memory_order_relaxed);
      if (value > 0 && wake_trace::Enabled()) wake_trace::Record(wake_trace::Event::kWake, word, site, 0);
      if (command == kWakeBitset && static_cast<std::uint32_t>(value3) != kMatchAny) {
        g_futex.masked_wakes.fetch_add(1, std::memory_order_relaxed);
        if (value > 0) WakeByAddressAll(const_cast<std::int32_t*>(word));
        return 0;
      }
      if (value == 1) {
        WakeByAddressSingle(const_cast<std::int32_t*>(word));
      } else if (value > 1) {
        WakeByAddressAll(const_cast<std::int32_t*>(word));
      }
      return 0;
    case kCmpRequeue:
      if (*word != value3) return ErrnoResult(guest::kEAGAIN);
      [[fallthrough]];
    case kRequeue:
      g_futex.requeues.fetch_add(1, std::memory_order_relaxed);
      if (wake_trace::Enabled()) wake_trace::Record(wake_trace::Event::kWake, word, site, 0);
      WakeByAddressAll(const_cast<std::int32_t*>(word));
      return 0;
    case kWakeOp: {
      g_futex.wake_ops.fetch_add(1, std::memory_order_relaxed);
      if (wake_trace::Enabled()) {
        wake_trace::Record(wake_trace::Event::kWake, word, site, 0);
        wake_trace::Record(wake_trace::Event::kWake, word2, site, 0);
      }
      if (word2 == nullptr || (reinterpret_cast<std::uintptr_t>(word2) & 3) != 0) return ErrnoResult(guest::kEINVAL);
      const std::uint32_t encoded = static_cast<std::uint32_t>(value3);
      const std::uint32_t operation = (encoded >> 28) & 0xf;
      const std::uint32_t comparison = (encoded >> 24) & 0xf;
      std::int32_t operand = static_cast<std::int32_t>((encoded >> 12) & 0xfff);
      const std::int32_t compare_arg = static_cast<std::int32_t>(encoded & 0xfff);
      if ((operation & 8) != 0) operand = 1 << (operand & 31);
      auto* target = reinterpret_cast<volatile LONG*>(const_cast<std::int32_t*>(word2));
      LONG old_value;
      for (;;) {
        old_value = *target;
        LONG new_value;
        switch (operation & 7) {
          case 0: new_value = operand; break;
          case 1: new_value = old_value + operand; break;
          case 2: new_value = old_value | operand; break;
          case 3: new_value = old_value & ~operand; break;
          case 4: new_value = old_value ^ operand; break;
          default: return ErrnoResult(guest::kENOSYS);
        }
        if (InterlockedCompareExchange(target, new_value, old_value) == old_value) break;
      }
      bool condition = false;
      switch (comparison) {
        case 0: condition = old_value == compare_arg; break;
        case 1: condition = old_value != compare_arg; break;
        case 2: condition = old_value < compare_arg; break;
        case 3: condition = old_value <= compare_arg; break;
        case 4: condition = old_value > compare_arg; break;
        case 5: condition = old_value >= compare_arg; break;
        default: return ErrnoResult(guest::kENOSYS);
      }
      if (value > 0) WakeByAddressAll(const_cast<std::int32_t*>(word));
      if (condition && timeout_or_value2 > 0) WakeByAddressAll(const_cast<std::int32_t*>(word2));
      return 0;
    }
    default: return ErrnoResult(guest::kENOSYS);
  }
}

}

std::uint32_t CurrentUid() noexcept {
  AcquireSRWLockShared(&g_identity_lock);
  const std::uint32_t uid = g_identity != nullptr ? g_identity->uid : GuestIdentity{}.uid;
  ReleaseSRWLockShared(&g_identity_lock);
  return uid;
}
std::uint32_t CurrentGid() noexcept {
  AcquireSRWLockShared(&g_identity_lock);
  const std::uint32_t gid = g_identity != nullptr ? g_identity->gid : GuestIdentity{}.gid;
  ReleaseSRWLockShared(&g_identity_lock);
  return gid;
}
std::int32_t CurrentPid() noexcept { return static_cast<std::int32_t>(GetCurrentProcessId()); }
std::int32_t CurrentTid() noexcept { return threads::CurrentTid(); }

bool FillRandom(void* buffer, std::size_t length) noexcept {
  auto* bytes = static_cast<unsigned char*>(buffer);
  while (length > 0) {
    const ULONG chunk = length > 0x40000000 ? 0x40000000 : static_cast<ULONG>(length);
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, bytes, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) return false;
    bytes += chunk;
    length -= chunk;
  }
  return true;
}

void SetGuestIdentity(const GuestIdentity& identity) {
  auto* copy = new GuestIdentity(identity);
  AcquireSRWLockExclusive(&g_identity_lock);
  GuestIdentity* old = g_identity;
  g_identity = copy;
  ReleaseSRWLockExclusive(&g_identity_lock);
  delete old;
}

GuestIdentity CurrentGuestIdentity() {
  AcquireSRWLockShared(&g_identity_lock);
  GuestIdentity copy = g_identity != nullptr ? *g_identity : GuestIdentity{};
  ReleaseSRWLockShared(&g_identity_lock);
  return copy;
}

}

std::string FutexStatisticsReport() {
  const auto take = [](std::atomic<std::uint64_t>& counter) {
    return std::to_string(counter.exchange(0, std::memory_order_relaxed));
  };
  core::FutexCounters& f = core::g_futex;
  if (f.waits.load(std::memory_order_relaxed) == 0 && f.wakes.load(std::memory_order_relaxed) == 0) return {};
  std::string report = "futex: " + take(f.waits) + " waits (" + take(f.timed_waits) + " timed, " +
                       take(f.masked_waits) + " with a bitset; " + take(f.long_waits) + " lasted 30 ms or more, " +
                       take(f.long_timeouts) + " of them until their timeout), " + take(f.wakes) + " wakes (" +
                       take(f.masked_wakes) + " with a bitset), " + take(f.requeues) + " requeues, " +
                       take(f.wake_ops) + " wake-ops";
  return report;
}

ELIXIR_BIONIC_FN(int, getpagesize)() noexcept { return 4096; }
ELIXIR_BIONIC_FN(int, get_nprocs_conf)() noexcept { return static_cast<int>(core::ProcessorsConfigured()); }
ELIXIR_BIONIC_FN(int, get_nprocs)() noexcept { return static_cast<int>(core::ProcessorsOnline()); }
ELIXIR_BIONIC_FN(abi::g_long, get_phys_pages)() noexcept {
  return static_cast<abi::g_long>(core::MemoryStatus().ullTotalPhys / 4096);
}
ELIXIR_BIONIC_FN(abi::g_long, get_avphys_pages)() noexcept {
  return static_cast<abi::g_long>(core::MemoryStatus().ullAvailPhys / 4096);
}

ELIXIR_BIONIC_FN(abi::g_long, sysconf)(int name) noexcept {
  using namespace guest;
  constexpr abi::g_long kPosixVersion = 200809;
  constexpr abi::g_long kMissing = -1;
  const auto rlimit_cur = [](int resource) -> abi::g_long {
    const std::uint64_t value = core::LimitOf(resource).rlim_cur;
    return value == core::kRlimInfinity ? -1 : static_cast<abi::g_long>(value);
  };
  const core::CacheInfo& caches = core::Caches();
  switch (name) {
    case k_SC_ARG_MAX: {
      const abi::g_long stack = rlimit_cur(kRLIMIT_STACK);
      const abi::g_long quarter = stack < 0 ? (6 << 20) : std::min<abi::g_long>(stack / 4, 6 << 20);
      return std::max<abi::g_long>(quarter, 131072);
    }
    case k_SC_AVPHYS_PAGES: return elixir_bionic_get_avphys_pages();
    case k_SC_CHILD_MAX: return rlimit_cur(kRLIMIT_NPROC);
    case k_SC_CLK_TCK: return 100;
    case k_SC_NPROCESSORS_CONF: return core::ProcessorsConfigured();
    case k_SC_NPROCESSORS_ONLN: return core::ProcessorsOnline();
    case k_SC_OPEN_MAX: return rlimit_cur(kRLIMIT_NOFILE);
    case k_SC_PAGESIZE:
    case k_SC_PAGE_SIZE: return 4096;
    case k_SC_PHYS_PAGES: return elixir_bionic_get_phys_pages();
    case k_SC_BC_BASE_MAX: return 99;
    case k_SC_BC_DIM_MAX: return 2048;
    case k_SC_BC_SCALE_MAX: return 99;
    case k_SC_BC_STRING_MAX: return 1000;
    case k_SC_COLL_WEIGHTS_MAX: return 2;
    case k_SC_EXPR_NEST_MAX: return 32;
    case k_SC_LINE_MAX: return 2048;
    case k_SC_NGROUPS_MAX: return 65536;
    case k_SC_NSIG: return kNSIG;
    case k_SC_PASS_MAX: return 128;
    case k_SC_2_C_BIND: return kPosixVersion;
    case k_SC_2_C_DEV: return kMissing;
    case k_SC_2_CHAR_TERM: return kPosixVersion;
    case k_SC_2_FORT_DEV:
    case k_SC_2_FORT_RUN:
    case k_SC_2_LOCALEDEF:
    case k_SC_2_SW_DEV:
    case k_SC_2_UPE: return kMissing;
    case k_SC_2_VERSION: return kPosixVersion;
    case k_SC_JOB_CONTROL: return kPosixVersion;
    case k_SC_SAVED_IDS: return 1;
    case k_SC_VERSION: return kPosixVersion;
    case k_SC_RE_DUP_MAX: return 255;
    case k_SC_STREAM_MAX: return 20;
    case k_SC_TZNAME_MAX: return 6;
    case k_SC_XOPEN_CRYPT: return kMissing;
    case k_SC_XOPEN_ENH_I18N: return 1;
    case k_SC_XOPEN_SHM: return 1;
    case k_SC_XOPEN_VERSION: return 700;
    case k_SC_XOPEN_REALTIME: return 1;
    case k_SC_XOPEN_REALTIME_THREADS: return 1;
    case k_SC_XOPEN_LEGACY: return kMissing;
    case k_SC_ATEXIT_MAX: return INT64_MAX;
    case k_SC_IOV_MAX: return 1024;
    case k_SC_XOPEN_UNIX: return 1;
    case k_SC_AIO_LISTIO_MAX: return 2;
    case k_SC_AIO_MAX: return 1;
    case k_SC_AIO_PRIO_DELTA_MAX: return 0;
    case k_SC_DELAYTIMER_MAX: return 32;
    case k_SC_MQ_OPEN_MAX: return 8;
    case k_SC_MQ_PRIO_MAX: return 32;
    case k_SC_RTSIG_MAX: return 32;
    case k_SC_SEM_NSEMS_MAX: return 256;
    case k_SC_SEM_VALUE_MAX: return 0x3fffffff;
    case k_SC_SIGQUEUE_MAX: return 32;
    case k_SC_TIMER_MAX: return 32;
    case k_SC_ASYNCHRONOUS_IO: return kMissing;
    case k_SC_FSYNC: return kPosixVersion;
    case k_SC_MAPPED_FILES: return kPosixVersion;
    case k_SC_MEMLOCK: return kPosixVersion;
    case k_SC_MEMLOCK_RANGE: return kPosixVersion;
    case k_SC_MEMORY_PROTECTION: return kPosixVersion;
    case k_SC_MESSAGE_PASSING: return kMissing;
    case k_SC_PRIORITIZED_IO: return kMissing;
    case k_SC_PRIORITY_SCHEDULING: return kPosixVersion;
    case k_SC_REALTIME_SIGNALS: return kPosixVersion;
    case k_SC_SEMAPHORES: return kPosixVersion;
    case k_SC_SHARED_MEMORY_OBJECTS: return kMissing;
    case k_SC_SYNCHRONIZED_IO: return kPosixVersion;
    case k_SC_TIMERS: return kPosixVersion;
    case k_SC_GETGR_R_SIZE_MAX: return 1024;
    case k_SC_GETPW_R_SIZE_MAX: return 1024;
    case k_SC_LOGIN_NAME_MAX: return 256;
    case k_SC_THREAD_DESTRUCTOR_ITERATIONS: return 4;
    case k_SC_THREAD_KEYS_MAX: return 128;
    case k_SC_THREAD_STACK_MIN: return 16384;
    case k_SC_THREAD_THREADS_MAX: return -1;
    case k_SC_TTY_NAME_MAX: return 32;
    case k_SC_THREADS: return kPosixVersion;
    case k_SC_THREAD_ATTR_STACKADDR: return kPosixVersion;
    case k_SC_THREAD_ATTR_STACKSIZE: return kPosixVersion;
    case k_SC_THREAD_PRIORITY_SCHEDULING: return kPosixVersion;
    case k_SC_THREAD_PRIO_INHERIT: return kMissing;
    case k_SC_THREAD_PRIO_PROTECT: return kMissing;
    case k_SC_THREAD_SAFE_FUNCTIONS: return kPosixVersion;
    case k_SC_MONOTONIC_CLOCK: return kPosixVersion;
    case k_SC_2_PBS:
    case k_SC_2_PBS_ACCOUNTING:
    case k_SC_2_PBS_CHECKPOINT:
    case k_SC_2_PBS_LOCATE:
    case k_SC_2_PBS_MESSAGE:
    case k_SC_2_PBS_TRACK: return -1;
    case k_SC_ADVISORY_INFO: return kPosixVersion;
    case k_SC_BARRIERS: return kPosixVersion;
    case k_SC_CLOCK_SELECTION: return kPosixVersion;
    case k_SC_CPUTIME: return kPosixVersion;
    case k_SC_HOST_NAME_MAX: return 255;
    case k_SC_IPV6: return kPosixVersion;
    case k_SC_RAW_SOCKETS: return kPosixVersion;
    case k_SC_READER_WRITER_LOCKS: return kPosixVersion;
    case k_SC_REGEXP: return 1;
    case k_SC_SHELL: return 1;
    case k_SC_SPAWN: return kPosixVersion;
    case k_SC_SPIN_LOCKS: return kPosixVersion;
    case k_SC_SPORADIC_SERVER: return kMissing;
    case k_SC_SS_REPL_MAX: return -1;
    case k_SC_SYMLOOP_MAX: return 8;
    case k_SC_THREAD_CPUTIME: return kPosixVersion;
    case k_SC_THREAD_PROCESS_SHARED: return kPosixVersion;
    case k_SC_THREAD_ROBUST_PRIO_INHERIT: return kMissing;
    case k_SC_THREAD_ROBUST_PRIO_PROTECT: return kMissing;
    case k_SC_THREAD_SPORADIC_SERVER: return kMissing;
    case k_SC_TIMEOUTS: return kPosixVersion;
    case k_SC_TRACE:
    case k_SC_TRACE_EVENT_FILTER:
    case k_SC_TRACE_EVENT_NAME_MAX:
    case k_SC_TRACE_INHERIT:
    case k_SC_TRACE_LOG:
    case k_SC_TRACE_NAME_MAX:
    case k_SC_TRACE_SYS_MAX:
    case k_SC_TRACE_USER_EVENT_MAX: return -1;
    case k_SC_TYPED_MEMORY_OBJECTS: return kMissing;
    case k_SC_V7_ILP32_OFF32: return -1;
    case k_SC_V7_ILP32_OFFBIG: return -1;
    case k_SC_V7_LP64_OFF64: return 1;
    case k_SC_V7_LPBIG_OFFBIG: return 1;
    case k_SC_XOPEN_STREAMS: return -1;
    case k_SC_XOPEN_UUCP: return -1;
    case k_SC_LEVEL1_ICACHE_SIZE: return caches.l1i.size;
    case k_SC_LEVEL1_ICACHE_ASSOC: return caches.l1i.assoc;
    case k_SC_LEVEL1_ICACHE_LINESIZE: return caches.l1i.linesize;
    case k_SC_LEVEL1_DCACHE_SIZE: return caches.l1d.size;
    case k_SC_LEVEL1_DCACHE_ASSOC: return caches.l1d.assoc;
    case k_SC_LEVEL1_DCACHE_LINESIZE: return caches.l1d.linesize;
    case k_SC_LEVEL2_CACHE_SIZE: return caches.l2.size;
    case k_SC_LEVEL2_CACHE_ASSOC: return caches.l2.assoc;
    case k_SC_LEVEL2_CACHE_LINESIZE: return caches.l2.linesize;
    case k_SC_LEVEL3_CACHE_SIZE: return caches.l3.size;
    case k_SC_LEVEL3_CACHE_ASSOC: return caches.l3.assoc;
    case k_SC_LEVEL3_CACHE_LINESIZE: return caches.l3.linesize;
    case k_SC_LEVEL4_CACHE_SIZE: return caches.l4.size;
    case k_SC_LEVEL4_CACHE_ASSOC: return caches.l4.assoc;
    case k_SC_LEVEL4_CACHE_LINESIZE: return caches.l4.linesize;
    default: return FailWith(kEINVAL, abi::g_long{-1});
  }
}

ELIXIR_BIONIC_FN(int, uname)(guest::Utsname* out) noexcept {
  if (out == nullptr) return FailWith(guest::kEFAULT, -1);
  guest::Utsname name{};
  try {
    core::CopyField(name.sysname, "Linux");
    core::CopyField(name.nodename, core::HostName());
    core::CopyField(name.release, "6.6.0-elixir");
    core::CopyField(name.version, "#1 SMP PREEMPT Wed Mar  5 00:00:00 UTC 2025");
    core::CopyField(name.machine, "x86_64");
    core::CopyField(name.domainname, "(none)");
  } catch (...) {
    return FailWith(guest::kENOMEM, -1);
  }
  std::memcpy(out, &name, sizeof(name));
  return 0;
}

ELIXIR_BIONIC_FN(int, sysinfo)(guest::Sysinfo* out) noexcept {
  if (out == nullptr) return FailWith(guest::kEFAULT, -1);
  guest::Sysinfo info{};
  info.uptime = static_cast<abi::g_long>(GetTickCount64() / 1000);
  double loads[3];
  core::SampleLoad(loads);
  for (int i = 0; i < 3; ++i) info.loads[i] = static_cast<abi::g_ulong>(loads[i] * 65536.0);
  const MEMORYSTATUSEX memory = core::MemoryStatus();
  info.totalram = memory.ullTotalPhys;
  info.freeram = memory.ullAvailPhys;
  info.sharedram = 0;
  info.bufferram = 0;
  info.totalswap = memory.ullTotalPageFile > memory.ullTotalPhys ? memory.ullTotalPageFile - memory.ullTotalPhys : 0;
  info.freeswap = memory.ullAvailPageFile > memory.ullAvailPhys ? memory.ullAvailPageFile - memory.ullAvailPhys : 0;
  if (info.freeswap > info.totalswap) info.freeswap = info.totalswap;
  PERFORMANCE_INFORMATION performance{};
  performance.cb = sizeof(performance);
  if (GetPerformanceInfo(&performance, sizeof(performance))) {
    info.procs = static_cast<std::uint16_t>(performance.ThreadCount > 0xffff ? 0xffff : performance.ThreadCount);
  }
  info.totalhigh = 0;
  info.freehigh = 0;
  info.mem_unit = 1;
  std::memcpy(out, &info, sizeof(info));
  return 0;
}

ELIXIR_BIONIC_FN(int, getloadavg)(double* averages, int count) noexcept {
  if (count < 0 || averages == nullptr) return -1;
  double loads[3];
  core::SampleLoad(loads);
  const int n = count > 3 ? 3 : count;
  for (int i = 0; i < n; ++i) averages[i] = loads[i];
  return n;
}

ELIXIR_BIONIC_FN(abi::g_pid, getpid)() noexcept { return core::CurrentPid(); }

ELIXIR_BIONIC_FN(abi::g_pid, getppid)() noexcept {
  static const abi::g_pid parent = [] {
    abi::g_pid result = 1;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return result;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    const DWORD self = GetCurrentProcessId();
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
      if (entry.th32ProcessID == self) {
        result = static_cast<abi::g_pid>(entry.th32ParentProcessID);
        break;
      }
    }
    CloseHandle(snapshot);
    return result;
  }();
  return parent;
}

ELIXIR_BIONIC_FN(abi::g_uid, getuid)() noexcept { return core::CurrentUid(); }
ELIXIR_BIONIC_FN(abi::g_uid, geteuid)() noexcept { return core::CurrentUid(); }
ELIXIR_BIONIC_FN(abi::g_gid, getgid)() noexcept { return core::CurrentGid(); }
ELIXIR_BIONIC_FN(abi::g_gid, getegid)() noexcept { return core::CurrentGid(); }

ELIXIR_BIONIC_FN(int, getresuid)(abi::g_uid* ruid, abi::g_uid* euid, abi::g_uid* suid) noexcept {
  const abi::g_uid uid = core::CurrentUid();
  if (ruid != nullptr) *ruid = uid;
  if (euid != nullptr) *euid = uid;
  if (suid != nullptr) *suid = uid;
  return 0;
}
ELIXIR_BIONIC_FN(int, getresgid)(abi::g_gid* rgid, abi::g_gid* egid, abi::g_gid* sgid) noexcept {
  const abi::g_gid gid = core::CurrentGid();
  if (rgid != nullptr) *rgid = gid;
  if (egid != nullptr) *egid = gid;
  if (sgid != nullptr) *sgid = gid;
  return 0;
}

ELIXIR_BIONIC_FN(int, getgroups)(int size, abi::g_gid* list) noexcept {
  const core::GuestIdentity identity = core::CurrentGuestIdentity();
  const int count = static_cast<int>(identity.groups.size());
  if (size < 0) return FailWith(guest::kEINVAL, -1);
  if (size == 0) return count;
  if (size < count) return FailWith(guest::kEINVAL, -1);
  if (list == nullptr) return FailWith(guest::kEFAULT, -1);
  for (int i = 0; i < count; ++i) list[i] = identity.groups[static_cast<std::size_t>(i)];
  return count;
}

namespace {
int SetIdentity(std::uint32_t requested, std::uint32_t current) noexcept {
  if (requested == static_cast<std::uint32_t>(-1) || requested == current) return 0;
  return FailWith(guest::kEPERM, -1);
}
}

ELIXIR_BIONIC_FN(int, setuid)(abi::g_uid uid) noexcept { return SetIdentity(uid, core::CurrentUid()); }
ELIXIR_BIONIC_FN(int, seteuid)(abi::g_uid uid) noexcept { return SetIdentity(uid, core::CurrentUid()); }
ELIXIR_BIONIC_FN(int, setgid)(abi::g_gid gid) noexcept { return SetIdentity(gid, core::CurrentGid()); }
ELIXIR_BIONIC_FN(int, setegid)(abi::g_gid gid) noexcept { return SetIdentity(gid, core::CurrentGid()); }
ELIXIR_BIONIC_FN(int, setreuid)(abi::g_uid ruid, abi::g_uid euid) noexcept {
  const abi::g_uid uid = core::CurrentUid();
  return SetIdentity(ruid, uid) == 0 && SetIdentity(euid, uid) == 0 ? 0 : -1;
}
ELIXIR_BIONIC_FN(int, setregid)(abi::g_gid rgid, abi::g_gid egid) noexcept {
  const abi::g_gid gid = core::CurrentGid();
  return SetIdentity(rgid, gid) == 0 && SetIdentity(egid, gid) == 0 ? 0 : -1;
}
ELIXIR_BIONIC_FN(int, setresuid)(abi::g_uid ruid, abi::g_uid euid, abi::g_uid suid) noexcept {
  const abi::g_uid uid = core::CurrentUid();
  return SetIdentity(ruid, uid) == 0 && SetIdentity(euid, uid) == 0 && SetIdentity(suid, uid) == 0 ? 0 : -1;
}
ELIXIR_BIONIC_FN(int, setresgid)(abi::g_gid rgid, abi::g_gid egid, abi::g_gid sgid) noexcept {
  const abi::g_gid gid = core::CurrentGid();
  return SetIdentity(rgid, gid) == 0 && SetIdentity(egid, gid) == 0 && SetIdentity(sgid, gid) == 0 ? 0 : -1;
}
ELIXIR_BIONIC_FN(int, setgroups)(abi::g_size, const abi::g_gid*) noexcept { return FailWith(guest::kEPERM, -1); }
ELIXIR_BIONIC_FN(int, setfsuid)(abi::g_uid) noexcept { return static_cast<int>(core::CurrentUid()); }
ELIXIR_BIONIC_FN(int, setfsgid)(abi::g_gid) noexcept { return static_cast<int>(core::CurrentGid()); }

ELIXIR_BIONIC_FN(abi::g_pid, getpgrp)() noexcept { return core::CurrentPid(); }

namespace {
bool OtherProcessExists(abi::g_pid pid) noexcept {
  if (pid <= 0) return false;
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (process == nullptr) return GetLastError() == ERROR_ACCESS_DENIED;
  DWORD code = 0;
  const bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
  CloseHandle(process);
  return alive;
}
}

ELIXIR_BIONIC_FN(abi::g_pid, getpgid)(abi::g_pid pid) noexcept {
  if (pid == 0 || pid == core::CurrentPid()) return core::CurrentPid();
  if (!OtherProcessExists(pid)) return FailWith(guest::kESRCH, -1);
  return pid;
}
ELIXIR_BIONIC_FN(abi::g_pid, getsid)(abi::g_pid pid) noexcept { return elixir_bionic_getpgid(pid); }
ELIXIR_BIONIC_FN(abi::g_pid, setsid)() noexcept {
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, setpgid)(abi::g_pid pid, abi::g_pid group) noexcept {
  const abi::g_pid self = core::CurrentPid();
  if (pid != 0 && pid != self) return FailWith(guest::kESRCH, -1);
  if (group < 0) return FailWith(guest::kEINVAL, -1);
  if (group != 0 && group != self) return FailWith(guest::kEPERM, -1);
  return 0;
}

ELIXIR_BIONIC_FN(int, gethostname)(char* buffer, abi::g_size size) noexcept {
  try {
    const std::string name = core::HostName();
    if (name.size() + 1 > size) return FailWith(guest::kENAMETOOLONG, -1);
    if (buffer == nullptr) return FailWith(guest::kEFAULT, -1);
    std::memcpy(buffer, name.c_str(), name.size() + 1);
    return 0;
  } catch (...) {
    return FailWith(guest::kENOMEM, -1);
  }
}
ELIXIR_BIONIC_FN(int, sethostname)(const char*, abi::g_size) noexcept { return FailWith(guest::kEPERM, -1); }
ELIXIR_BIONIC_FN(int, getdomainname)(char* buffer, abi::g_size size) noexcept {
  static const char kDomain[] = "(none)";
  if (sizeof(kDomain) > size) return FailWith(guest::kEINVAL, -1);
  if (buffer == nullptr) return FailWith(guest::kEFAULT, -1);
  std::memcpy(buffer, kDomain, sizeof(kDomain));
  return 0;
}
ELIXIR_BIONIC_FN(int, setdomainname)(const char*, abi::g_size) noexcept { return FailWith(guest::kEPERM, -1); }

ELIXIR_BIONIC_FN(int, prlimit)(abi::g_pid pid, int resource, const guest::Rlimit* new_limit,
                               guest::Rlimit* old_limit) noexcept {
  const int error = core::DoPrlimit(pid, resource, new_limit, old_limit);
  return error == 0 ? 0 : FailWith(error, -1);
}
ELIXIR_BIONIC_FN(int, getrlimit)(int resource, guest::Rlimit* limit) noexcept {
  if (limit == nullptr) return FailWith(guest::kEFAULT, -1);
  return elixir_bionic_prlimit(0, resource, nullptr, limit);
}
ELIXIR_BIONIC_FN(int, setrlimit)(int resource, const guest::Rlimit* limit) noexcept {
  if (limit == nullptr) return FailWith(guest::kEFAULT, -1);
  return elixir_bionic_prlimit(0, resource, limit, nullptr);
}

ELIXIR_BIONIC_FN(int, getrusage)(int who, guest::Rusage* usage) noexcept {
  if (usage == nullptr) return FailWith(guest::kEFAULT, -1);
  guest::Rusage result{};
  const auto to_timeval = [](const FILETIME& time) {
    const std::uint64_t ticks = core::FileTimeValue(time);
    guest::Timeval tv;
    tv.tv_sec = static_cast<abi::g_time>(ticks / 10000000);
    tv.tv_usec = static_cast<abi::g_long>((ticks % 10000000) / 10);
    return tv;
  };
  FILETIME creation, exit_time, kernel_time, user_time;
  if (who == guest::kRUSAGE_SELF || who == guest::kRUSAGE_THREAD) {
    const BOOL ok = who == guest::kRUSAGE_SELF
                        ? GetProcessTimes(GetCurrentProcess(), &creation, &exit_time, &kernel_time, &user_time)
                        : GetThreadTimes(GetCurrentThread(), &creation, &exit_time, &kernel_time, &user_time);
    if (ok) {
      result.ru_utime = to_timeval(user_time);
      result.ru_stime = to_timeval(kernel_time);
    }
    PROCESS_MEMORY_COUNTERS memory{};
    memory.cb = sizeof(memory);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory))) {
      result.ru_maxrss = static_cast<abi::g_long>(memory.PeakWorkingSetSize / 1024);
      if (who == guest::kRUSAGE_SELF) result.ru_minflt = memory.PageFaultCount;
    }
    IO_COUNTERS io{};
    if (who == guest::kRUSAGE_SELF && GetProcessIoCounters(GetCurrentProcess(), &io)) {
      result.ru_inblock = static_cast<abi::g_long>(io.ReadTransferCount / 512);
      result.ru_oublock = static_cast<abi::g_long>(io.WriteTransferCount / 512);
    }
  } else if (who != guest::kRUSAGE_CHILDREN) {
    return FailWith(guest::kEINVAL, -1);
  }
  std::memcpy(usage, &result, sizeof(result));
  return 0;
}

ELIXIR_BIONIC_FN(int, getpriority)(int which, abi::g_uid who) noexcept {
  using namespace guest;
  if (which != kPRIO_PROCESS && which != kPRIO_PGRP && which != kPRIO_USER) return FailWith(kEINVAL, -1);
  const auto target = static_cast<std::int32_t>(who);
  if (which == kPRIO_PROCESS && (target == 0 || target == core::CurrentTid())) {
    if (core::t_nice_set) return core::t_nice;
    return core::WindowsPriorityToNice(GetThreadPriority(GetCurrentThread()));
  }
  if (target == 0 || target == core::CurrentPid() ||
      (which == kPRIO_USER && static_cast<std::uint32_t>(who) == core::CurrentUid())) {
    return core::g_process_nice.load();
  }
  if (which == kPRIO_PROCESS) {
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threads::WindowsThreadIdFromTid(target));
    if (thread != nullptr) {
      const bool ours = GetProcessIdOfThread(thread) == GetCurrentProcessId();
      const int priority = GetThreadPriority(thread);
      CloseHandle(thread);
      if (ours && priority != THREAD_PRIORITY_ERROR_RETURN) return core::WindowsPriorityToNice(priority);
    }
  }
  return FailWith(kESRCH, -1);
}

ELIXIR_BIONIC_FN(int, setpriority)(int which, abi::g_uid who, int value) noexcept {
  using namespace guest;
  if (which != kPRIO_PROCESS && which != kPRIO_PGRP && which != kPRIO_USER) return FailWith(kEINVAL, -1);
  const int nice = value < kPRIO_MIN ? kPRIO_MIN : (value > kPRIO_MAX - 1 ? kPRIO_MAX - 1 : value);
  const auto target = static_cast<std::int32_t>(who);
  if (which == kPRIO_PROCESS && (target == 0 || target == core::CurrentTid())) {
    SetThreadPriority(GetCurrentThread(), core::NiceToWindowsPriority(nice));
    core::t_nice = nice;
    core::t_nice_set = true;
    return 0;
  }
  if (target == 0 || target == core::CurrentPid() ||
      (which == kPRIO_USER && static_cast<std::uint32_t>(who) == core::CurrentUid())) {
    core::g_process_nice.store(nice);
    return 0;
  }
  if (which == kPRIO_PROCESS) {
    HANDLE thread = OpenThread(THREAD_SET_LIMITED_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                               threads::WindowsThreadIdFromTid(target));
    if (thread != nullptr) {
      const bool ours = GetProcessIdOfThread(thread) == GetCurrentProcessId();
      const bool ok = ours && SetThreadPriority(thread, core::NiceToWindowsPriority(nice));
      CloseHandle(thread);
      if (ok) return 0;
    }
  }
  return FailWith(kESRCH, -1);
}

ELIXIR_BIONIC_FN(int, prctl)(int option, abi::g_ulong arg2, abi::g_ulong arg3, abi::g_ulong arg4,
                             abi::g_ulong arg5) noexcept {
  using namespace guest;
  (void)arg4;
  (void)arg5;
  switch (option) {
    case kPR_SET_NAME: {
      const char* name = reinterpret_cast<const char*>(arg2);
      if (name == nullptr) return FailWith(kEFAULT, -1);
      std::size_t length = 0;
      while (length < 15 && name[length] != '\0') ++length;
      std::memcpy(core::t_thread_name, name, length);
      core::t_thread_name[length] = '\0';
      core::t_thread_name_set = true;
      try {
        const std::wstring wide = core::Utf8ToWideString(core::t_thread_name, length);
        SetThreadDescription(GetCurrentThread(), wide.c_str());
      } catch (...) {
      }
      return 0;
    }
    case kPR_GET_NAME: {
      char* out = reinterpret_cast<char*>(arg2);
      if (out == nullptr) return FailWith(kEFAULT, -1);
      char name[16] = {};
      if (core::t_thread_name_set) {
        std::memcpy(name, core::t_thread_name, sizeof(name));
      } else {
        std::string text;
        PWSTR description = nullptr;
        if (SUCCEEDED(GetThreadDescription(GetCurrentThread(), &description)) && description != nullptr) {
          try {
            text = core::WideToUtf8String(description);
          } catch (...) {
          }
          LocalFree(description);
        }
        if (text.empty()) text = Options().package_name;
        std::memcpy(name, text.data(), std::min<std::size_t>(text.size(), 15));
      }
      std::memcpy(out, name, sizeof(name));
      return 0;
    }
    case kPR_SET_VMA:
      if (arg2 != static_cast<abi::g_ulong>(kPR_SET_VMA_ANON_NAME)) return FailWith(kEINVAL, -1);
      return 0;
    case kPR_SET_DUMPABLE:
      if (arg2 > 1) return FailWith(kEINVAL, -1);
      core::g_dumpable.store(static_cast<int>(arg2));
      return 0;
    case kPR_GET_DUMPABLE: return core::g_dumpable.load();
    case kPR_SET_PDEATHSIG:
      if (arg2 > static_cast<abi::g_ulong>(core::kMaxSignal)) return FailWith(kEINVAL, -1);
      core::t_pdeathsig = static_cast<int>(arg2);
      return 0;
    case kPR_GET_PDEATHSIG: {
      int* out = reinterpret_cast<int*>(arg2);
      if (out == nullptr) return FailWith(kEFAULT, -1);
      *out = core::t_pdeathsig;
      return 0;
    }
    case kPR_SET_TIMERSLACK:
      core::t_timerslack_ns = arg2 == 0 ? 50000 : arg2;
      return 0;
    case kPR_GET_TIMERSLACK:
      return core::t_timerslack_ns > 0x7fffffff ? 0x7fffffff : static_cast<int>(core::t_timerslack_ns);
    case kPR_SET_KEEPCAPS:
      if (arg2 > 1) return FailWith(kEINVAL, -1);
      core::g_keepcaps.store(static_cast<int>(arg2));
      return 0;
    case kPR_GET_KEEPCAPS: return core::g_keepcaps.load();
    case kPR_SET_NO_NEW_PRIVS:
      if (arg2 != 1 || arg3 != 0 || arg4 != 0 || arg5 != 0) return FailWith(kEINVAL, -1);
      core::g_no_new_privs.store(1);
      return 0;
    case kPR_GET_NO_NEW_PRIVS: return core::g_no_new_privs.load();
    case kPR_SET_CHILD_SUBREAPER:
      core::g_child_subreaper.store(arg2 != 0 ? 1 : 0);
      return 0;
    case kPR_GET_CHILD_SUBREAPER: {
      int* out = reinterpret_cast<int*>(arg2);
      if (out == nullptr) return FailWith(kEFAULT, -1);
      *out = core::g_child_subreaper.load();
      return 0;
    }
    case kPR_SET_THP_DISABLE:
      core::g_thp_disable.store(arg2 != 0 ? 1 : 0);
      return 0;
    case kPR_GET_THP_DISABLE: return core::g_thp_disable.load();
    case kPR_GET_SECCOMP: return 0;
    case kPR_CAPBSET_READ:
      if (arg2 > 40) return FailWith(kEINVAL, -1);
      return 0;
    case kPR_SET_PTRACER: return 0;
    case kPR_GET_TSC: {
      int* out = reinterpret_cast<int*>(arg2);
      if (out == nullptr) return FailWith(kEFAULT, -1);
      *out = kPR_TSC_ENABLE;
      return 0;
    }
    case kPR_SET_TSC: return arg2 == static_cast<abi::g_ulong>(kPR_TSC_ENABLE) ? 0 : FailWith(kEINVAL, -1);
    case kPR_GET_SPECULATION_CTRL: return kPR_SPEC_NOT_AFFECTED;
    default: return FailWith(kEINVAL, -1);
  }
}

ELIXIR_BIONIC_FN(abi::g_ssize, getrandom)(void* buffer, abi::g_size length, unsigned flags) noexcept {
  constexpr unsigned kNonblock = 1, kRandom = 2, kInsecure = 4;
  if ((flags & ~(kNonblock | kRandom | kInsecure)) != 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  if ((flags & kRandom) != 0 && (flags & kInsecure) != 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  if (length == 0) return 0;
  if (buffer == nullptr) return FailWith(guest::kEFAULT, abi::g_ssize{-1});
  const abi::g_size count = length > 0x1ffffff ? 0x1ffffff : length;
  if (!core::FillRandom(buffer, count)) return FailWith(guest::kEIO, abi::g_ssize{-1});
  return static_cast<abi::g_ssize>(count);
}

ELIXIR_BIONIC_FN(int, getentropy)(void* buffer, abi::g_size length) noexcept {
  if (length > 256) return FailWith(guest::kEIO, -1);
  if (length == 0) return 0;
  if (buffer == nullptr) return FailWith(guest::kEFAULT, -1);
  return core::FillRandom(buffer, length) ? 0 : FailWith(guest::kEIO, -1);
}

ELIXIR_BIONIC_FN(abi::g_long, syscall)(abi::g_long number, abi::g_long a1, abi::g_long a2, abi::g_long a3,
                                       abi::g_long a4, abi::g_long a5, abi::g_long a6) noexcept {
  using namespace core;
  (void)a6;
  const auto as_int = [](abi::g_long value) { return static_cast<int>(value); };
  const auto ptr = [](abi::g_long value) { return reinterpret_cast<void*>(value); };
  switch (number) {
    case kSysGetpid: return CurrentPid();
    case kSysGettid: return CurrentTid();
    case kSysGetppid: return elixir_bionic_getppid();
    case kSysGetuid:
    case kSysGeteuid: return CurrentUid();
    case kSysGetgid:
    case kSysGetegid: return CurrentGid();
    case kSysSchedYield: SwitchToThread(); return 0;
    case kSysFutex:
      return Futex(reinterpret_cast<volatile std::int32_t*>(a1), as_int(a2), static_cast<std::int32_t>(a3), a4,
                   reinterpret_cast<volatile std::int32_t*>(a5), static_cast<std::int32_t>(a6),
                   reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)));
    case kSysGetrandom:
      return elixir_bionic_getrandom(ptr(a1), static_cast<abi::g_size>(a2), static_cast<unsigned>(a3));
    case kSysClockGettime:
      return elixir_bionic_clock_gettime(as_int(a1), static_cast<guest::Timespec*>(ptr(a2)));
    case kSysClockGetres: return elixir_bionic_clock_getres(as_int(a1), static_cast<guest::Timespec*>(ptr(a2)));
    case kSysClockSettime:
      return elixir_bionic_clock_settime(as_int(a1), static_cast<const guest::Timespec*>(ptr(a2)));
    case kSysClockNanosleep: {
      const int error = elixir_bionic_clock_nanosleep(as_int(a1), as_int(a2), static_cast<const guest::Timespec*>(ptr(a3)),
                                                      static_cast<guest::Timespec*>(ptr(a4)));
      return error == 0 ? 0 : ErrnoResult(error);
    }
    case kSysNanosleep:
      return elixir_bionic_nanosleep(static_cast<const guest::Timespec*>(ptr(a1)), static_cast<guest::Timespec*>(ptr(a2)));
    case kSysGettimeofday:
      return elixir_bionic_gettimeofday(static_cast<guest::Timeval*>(ptr(a1)), static_cast<guest::Timezone*>(ptr(a2)));
    case kSysTime: return elixir_bionic_time(static_cast<abi::g_time*>(ptr(a1)));
    case kSysUname: return elixir_bionic_uname(static_cast<guest::Utsname*>(ptr(a1)));
    case kSysSysinfo: return elixir_bionic_sysinfo(static_cast<guest::Sysinfo*>(ptr(a1)));
    case kSysPrctl:
      return elixir_bionic_prctl(as_int(a1), static_cast<abi::g_ulong>(a2), static_cast<abi::g_ulong>(a3),
                                 static_cast<abi::g_ulong>(a4), static_cast<abi::g_ulong>(a5));
    case kSysGetrlimit: return elixir_bionic_getrlimit(as_int(a1), static_cast<guest::Rlimit*>(ptr(a2)));
    case kSysSetrlimit: return elixir_bionic_setrlimit(as_int(a1), static_cast<const guest::Rlimit*>(ptr(a2)));
    case kSysPrlimit64:
      return elixir_bionic_prlimit(as_int(a1), as_int(a2), static_cast<const guest::Rlimit*>(ptr(a3)),
                                   static_cast<guest::Rlimit*>(ptr(a4)));
    case kSysGetrusage: return elixir_bionic_getrusage(as_int(a1), static_cast<guest::Rusage*>(ptr(a2)));
    case kSysGetpriority: {
      SetGuestErrno(0);
      const int nice = elixir_bionic_getpriority(as_int(a1), static_cast<abi::g_uid>(a2));
      if (nice == -1 && GuestErrno() != 0) return -1;
      return 20 - nice;
    }
    case kSysSetpriority:
      return elixir_bionic_setpriority(as_int(a1), static_cast<abi::g_uid>(a2), as_int(a3));
    case kSysKill: return elixir_bionic_kill(as_int(a1), as_int(a2));
    case kSysTgkill: return elixir_bionic_tgkill(as_int(a1), as_int(a2), as_int(a3));
    case kSysTkill: return elixir_bionic_tgkill(CurrentPid(), as_int(a1), as_int(a2));
    case kSysRtSigaction: {
      if (a4 != 8) return ErrnoResult(guest::kEINVAL);
      const auto* kernel_new = static_cast<const KernelSigaction*>(ptr(a2));
      auto* kernel_old = static_cast<KernelSigaction*>(ptr(a3));
      guest::Sigaction converted{};
      if (kernel_new != nullptr) {
        converted.sa_handler = kernel_new->handler;
        converted.sa_flags = static_cast<std::int32_t>(kernel_new->flags);
        converted.sa_mask = kernel_new->mask;
        converted.sa_restorer = kernel_new->restorer;
      }
      guest::Sigaction old{};
      if (elixir_bionic_sigaction(as_int(a1), kernel_new != nullptr ? &converted : nullptr, &old) != 0) return -1;
      if (kernel_old != nullptr) {
        kernel_old->handler = old.sa_handler;
        kernel_old->flags = static_cast<std::uint32_t>(old.sa_flags);
        kernel_old->restorer = old.sa_restorer;
        kernel_old->mask = old.sa_mask;
      }
      return 0;
    }
    case kSysRtSigprocmask:
      if (a4 != 8) return ErrnoResult(guest::kEINVAL);
      return elixir_bionic_sigprocmask(as_int(a1), static_cast<const guest::Sigset*>(ptr(a2)),
                                       static_cast<guest::Sigset*>(ptr(a3)));
    case kSysRtSigpending:
      if (a2 != 8) return ErrnoResult(guest::kEINVAL);
      return elixir_bionic_sigpending(static_cast<guest::Sigset*>(ptr(a1)));
    case kSysRtSigsuspend:
      if (a2 != 8) return ErrnoResult(guest::kEINVAL);
      return elixir_bionic_sigsuspend(static_cast<const guest::Sigset*>(ptr(a1)));
    case kSysSigaltstack:
      return elixir_bionic_sigaltstack(static_cast<const guest::StackT*>(ptr(a1)), static_cast<guest::StackT*>(ptr(a2)));
    case kSysSchedGetaffinity: {
      if (a1 != 0 && a1 != CurrentPid() && a1 != CurrentTid()) return ErrnoResult(guest::kESRCH);
      const abi::g_long cpus = ProcessorsConfigured();
      const auto bytes = static_cast<abi::g_size>(((cpus + 63) / 64) * 8);
      if (static_cast<abi::g_size>(a2) < bytes || (a2 & 7) != 0) return ErrnoResult(guest::kEINVAL);
      auto* mask = static_cast<std::uint64_t*>(ptr(a3));
      if (mask == nullptr) return ErrnoResult(guest::kEFAULT);
      std::memset(mask, 0, bytes);
      for (abi::g_long cpu = 0; cpu < ProcessorsOnline(); ++cpu) mask[cpu / 64] |= std::uint64_t{1} << (cpu % 64);
      return static_cast<abi::g_long>(bytes);
    }
    case kSysGetcpu: {
      PROCESSOR_NUMBER processor{};
      GetCurrentProcessorNumberEx(&processor);
      if (a1 != 0) *static_cast<std::uint32_t*>(ptr(a1)) = processor.Group * 64u + processor.Number;
      if (a2 != 0) *static_cast<std::uint32_t*>(ptr(a2)) = 0;
      return 0;
    }
    case kSysMembarrier: {
      constexpr abi::g_long kQuery = 0, kGlobal = 1, kPrivateExpedited = 8, kRegisterPrivateExpedited = 16;
      switch (a1) {
        case kQuery: return kGlobal | kPrivateExpedited | kRegisterPrivateExpedited;
        case kGlobal:
        case kPrivateExpedited: FlushProcessWriteBuffers(); return 0;
        case kRegisterPrivateExpedited: return 0;
        default: return ErrnoResult(guest::kEINVAL);
      }
    }
    case kSysArchPrctl: {
      constexpr abi::g_long kArchSetFs = 0x1002, kArchGetFs = 0x1003;
      if (a1 == kArchGetFs) {
        auto* out = static_cast<std::uint64_t*>(ptr(a2));
        platform::GuestThreadState* state = platform::EnsureGuestThread();
        if (out == nullptr || state == nullptr) return ErrnoResult(guest::kEFAULT);
        *out = reinterpret_cast<std::uint64_t>(state->thread_pointer());
        return 0;
      }
      return ErrnoResult(a1 == kArchSetFs ? guest::kEPERM : guest::kEINVAL);
    }
    case kSysExit: ExitThread(static_cast<DWORD>(a1) & 0xff);
    case kSysExitGroup: elixir_bionic__exit(as_int(a1)); return 0;
    case kSysRead:
    case kSysWrite:
    default:
      return ErrnoResult(guest::kENOSYS);
  }
}

void RegisterSystemExports(ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(sysconf);
  ELIXIR_ADD(getpagesize);
  ELIXIR_ADD(get_nprocs);
  ELIXIR_ADD(get_nprocs_conf);
  ELIXIR_ADD(get_phys_pages);
  ELIXIR_ADD(get_avphys_pages);
  ELIXIR_ADD(uname);
  ELIXIR_ADD(sysinfo);
  ELIXIR_ADD(getloadavg);
  ELIXIR_ADD(getpid);
  ELIXIR_ADD(getppid);
  ELIXIR_ADD(getuid);
  ELIXIR_ADD(geteuid);
  ELIXIR_ADD(getgid);
  ELIXIR_ADD(getegid);
  ELIXIR_ADD(getresuid);
  ELIXIR_ADD(getresgid);
  ELIXIR_ADD(getgroups);
  ELIXIR_ADD(setuid);
  ELIXIR_ADD(seteuid);
  ELIXIR_ADD(setgid);
  ELIXIR_ADD(setegid);
  ELIXIR_ADD(setreuid);
  ELIXIR_ADD(setregid);
  ELIXIR_ADD(setresuid);
  ELIXIR_ADD(setresgid);
  ELIXIR_ADD(setgroups);
  ELIXIR_ADD(setfsuid);
  ELIXIR_ADD(setfsgid);
  ELIXIR_ADD(getpgrp);
  ELIXIR_ADD(getpgid);
  ELIXIR_ADD(getsid);
  ELIXIR_ADD(setsid);
  ELIXIR_ADD(setpgid);
  ELIXIR_ADD(gethostname);
  ELIXIR_ADD(sethostname);
  ELIXIR_ADD(getdomainname);
  ELIXIR_ADD(setdomainname);
  ELIXIR_ADD(getrlimit);
  ELIXIR_ADD(setrlimit);
  ELIXIR_ADD(prlimit);
  table.Add(kLibc, "getrlimit64", reinterpret_cast<void*>(&elixir_bionic_getrlimit));
  table.Add(kLibc, "setrlimit64", reinterpret_cast<void*>(&elixir_bionic_setrlimit));
  table.Add(kLibc, "prlimit64", reinterpret_cast<void*>(&elixir_bionic_prlimit));
  ELIXIR_ADD(getrusage);
  ELIXIR_ADD(getpriority);
  ELIXIR_ADD(setpriority);
  ELIXIR_ADD(prctl);
  ELIXIR_ADD(syscall);
  ELIXIR_ADD(getrandom);
  ELIXIR_ADD(getentropy);
#undef ELIXIR_ADD
  core::RegisterSignalExports(table);
  core::RegisterProcessExtExports(table);
  core::RegisterIdentityExports(table);
  core::RegisterSyslogExports(table);
  core::RegisterSysmiscExports(table);
}

}
