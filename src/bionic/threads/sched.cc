// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <cstdio>
#include <cstring>

#include "bionic/memory.h"
#include "bionic/threads/thread_internal.h"

namespace elixir::bionic::threads::internal {
namespace {

constexpr std::size_t kCpuSetBits = 1024;
constexpr std::size_t kMaxGroups = 64;

struct ProcessorTopology {
  WORD group_count = 0;
  DWORD group_offset[kMaxGroups] = {};
  DWORD group_size[kMaxGroups] = {};
  DWORD total = 0;
};

const ProcessorTopology& Topology() noexcept {
  static const ProcessorTopology topology = [] {
    ProcessorTopology result;
    WORD groups = GetActiveProcessorGroupCount();
    if (groups == 0) groups = 1;
    if (groups > kMaxGroups) groups = kMaxGroups;
    result.group_count = groups;
    DWORD offset = 0;
    for (WORD g = 0; g < groups; ++g) {
      result.group_offset[g] = offset;
      result.group_size[g] = GetActiveProcessorCount(g);
      offset += result.group_size[g];
    }
    result.total = offset != 0 ? offset : 1;
    return result;
  }();
  return topology;
}

bool IsRealtimePolicy(int policy) noexcept { return policy == guest::kSCHED_FIFO || policy == guest::kSCHED_RR; }

bool IsNormalPolicy(int policy) noexcept {
  return policy == guest::kSCHED_NORMAL || policy == guest::kSCHED_BATCH || policy == guest::kSCHED_IDLE;
}

void LogRealtimeDenied(int policy) noexcept {
  static LONG logged = 0;
  if (InterlockedExchange(&logged, 1) == 0) {
    char message[128];
    std::snprintf(message, sizeof(message), "denied guest real-time scheduling (policy=%d): keeping normal Windows priority", policy);
    HostLog(kAndroidLogWarn, "libc", message);
  }
}

template <typename F>
bool WithSchedTarget(std::int32_t pid, F&& fn) noexcept {
  bool done = false;
  if (WithThreadByTidDo(pid, [&](PthreadInternal* thread) {
        fn(static_cast<HANDLE>(thread->handle), thread);
        done = true;
      })) {
    return done;
  }
  const DWORD windows_tid = WindowsTidFromGuestTid(pid);
  HANDLE handle = OpenThread(THREAD_QUERY_INFORMATION | THREAD_SET_INFORMATION, FALSE, windows_tid);
  if (handle == nullptr) return false;
  if (GetProcessIdOfThread(handle) != GetCurrentProcessId()) {
    CloseHandle(handle);
    return false;
  }
  fn(handle, static_cast<PthreadInternal*>(nullptr));
  CloseHandle(handle);
  return true;
}

}

int ValidateSchedPolicy(int policy, int priority) noexcept {
  if (IsNormalPolicy(policy)) return priority == 0 ? 0 : guest::kEINVAL;
  if (IsRealtimePolicy(policy)) {
    if (priority < 1 || priority > 99) return guest::kEINVAL;
    LogRealtimeDenied(policy);
    return guest::kEPERM;
  }
  return guest::kEINVAL;
}

void ApplySchedPolicy(void* handle, int old_policy, int new_policy) noexcept {
  if (handle == nullptr) return;
  if (new_policy == guest::kSCHED_IDLE) {
    SetThreadPriority(static_cast<HANDLE>(handle), THREAD_PRIORITY_LOWEST);
  } else if (old_policy == guest::kSCHED_IDLE) {
    SetThreadPriority(static_cast<HANDLE>(handle), THREAD_PRIORITY_NORMAL);
  }
}

}

namespace {
namespace internal = elixir::bionic::threads::internal;
namespace guest = elixir::bionic::guest;
using elixir::bionic::FailWith;
using internal::PthreadInternal;

struct CpuSet {
  std::uint64_t bits[internal::kCpuSetBits / 64];
};
static_assert(sizeof(CpuSet) == 128, "bionic cpu_set_t is 128 bytes on LP64");

}

ELIXIR_BIONIC_FN(int, sched_yield)() noexcept {
  if (!SwitchToThread()) Sleep(0);
  return 0;
}

ELIXIR_BIONIC_FN(int, sched_getcpu)() noexcept {
  PROCESSOR_NUMBER number;
  GetCurrentProcessorNumberEx(&number);
  const internal::ProcessorTopology& topology = internal::Topology();
  const DWORD offset = number.Group < topology.group_count ? topology.group_offset[number.Group] : 0;
  return static_cast<int>(offset + number.Number);
}

ELIXIR_BIONIC_FN(int, sched_getaffinity)(std::int32_t pid, guest::g_size set_size, CpuSet* set) noexcept {
  const internal::ProcessorTopology& topology = internal::Topology();
  if (set == nullptr) return FailWith(guest::kEFAULT, -1);
  if ((set_size & 7) != 0 || set_size * 8 < topology.total) return FailWith(guest::kEINVAL, -1);
  GROUP_AFFINITY affinity{};
  bool ok = false;
  const bool found = internal::WithSchedTarget(pid, [&](HANDLE handle, PthreadInternal*) {
    ok = GetThreadGroupAffinity(handle, &affinity) != FALSE;
  });
  if (!found) return FailWith(guest::kESRCH, -1);
  if (!ok) return FailWith(elixir::bionic::ErrnoFromWin32(GetLastError()), -1);
  auto* bytes = reinterpret_cast<unsigned char*>(set);
  std::memset(bytes, 0, static_cast<std::size_t>(set_size));
  const DWORD base = affinity.Group < topology.group_count ? topology.group_offset[affinity.Group] : 0;
  for (DWORD bit = 0; bit < 64; ++bit) {
    if ((affinity.Mask & (KAFFINITY{1} << bit)) == 0) continue;
    const std::size_t cpu = base + bit;
    if (cpu / 8 < set_size) bytes[cpu / 8] |= static_cast<unsigned char>(1u << (cpu % 8));
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, sched_setaffinity)(std::int32_t pid, guest::g_size set_size, const CpuSet* set) noexcept {
  if (set == nullptr) return FailWith(guest::kEFAULT, -1);
  const internal::ProcessorTopology& topology = internal::Topology();
  const auto* bytes = reinterpret_cast<const unsigned char*>(set);
  GROUP_AFFINITY affinity{};
  bool chosen = false;
  for (WORD group = 0; group < topology.group_count && !chosen; ++group) {
    KAFFINITY mask = 0;
    for (DWORD bit = 0; bit < topology.group_size[group] && bit < 64; ++bit) {
      const std::size_t cpu = topology.group_offset[group] + bit;
      if (cpu / 8 >= set_size) break;
      if ((bytes[cpu / 8] & (1u << (cpu % 8))) != 0) mask |= KAFFINITY{1} << bit;
    }
    if (mask != 0) {
      affinity.Group = group;
      affinity.Mask = mask;
      chosen = true;
    }
  }
  if (!chosen) return FailWith(guest::kEINVAL, -1);
  bool ok = false;
  DWORD error = 0;
  const bool found = internal::WithSchedTarget(pid, [&](HANDLE handle, PthreadInternal*) {
    ok = SetThreadGroupAffinity(handle, &affinity, nullptr) != FALSE;
    if (!ok) error = GetLastError();
  });
  if (!found) return FailWith(guest::kESRCH, -1);
  if (!ok) return FailWith(error == ERROR_ACCESS_DENIED ? guest::kEPERM : guest::kEINVAL, -1);
  return 0;
}

ELIXIR_BIONIC_FN(int, sched_get_priority_max)(int policy) noexcept {
  if (internal::IsRealtimePolicy(policy)) return 99;
  if (internal::IsNormalPolicy(policy) || policy == guest::kSCHED_DEADLINE) return 0;
  return FailWith(guest::kEINVAL, -1);
}

ELIXIR_BIONIC_FN(int, sched_get_priority_min)(int policy) noexcept {
  if (internal::IsRealtimePolicy(policy)) return 1;
  if (internal::IsNormalPolicy(policy) || policy == guest::kSCHED_DEADLINE) return 0;
  return FailWith(guest::kEINVAL, -1);
}

ELIXIR_BIONIC_FN(int, sched_getscheduler)(std::int32_t pid) noexcept {
  if (pid < 0) return FailWith(guest::kEINVAL, -1);
  int policy = guest::kSCHED_NORMAL;
  if (!internal::WithSchedTarget(pid, [&](HANDLE, PthreadInternal* thread) {
        if (thread != nullptr) policy = thread->sched_policy;
      })) {
    return FailWith(guest::kESRCH, -1);
  }
  return policy;
}

ELIXIR_BIONIC_FN(int, sched_setscheduler)(std::int32_t pid, int policy, const guest::SchedParam* param) noexcept {
  if (param == nullptr || pid < 0) return FailWith(guest::kEINVAL, -1);
  int result = 0;
  const int base_policy = policy & ~internal::kSchedResetOnFork;
  if (!internal::WithSchedTarget(pid, [&](HANDLE handle, PthreadInternal* thread) {
        result = internal::ValidateSchedPolicy(base_policy, param->sched_priority);
        if (result != 0) return;
        const int old_policy = thread != nullptr ? thread->sched_policy : guest::kSCHED_NORMAL;
        internal::ApplySchedPolicy(handle, old_policy, base_policy);
        if (thread != nullptr) {
          thread->sched_policy = base_policy;
          thread->sched_priority = param->sched_priority;
        }
      })) {
    return FailWith(guest::kESRCH, -1);
  }
  return result == 0 ? 0 : FailWith(result, -1);
}

ELIXIR_BIONIC_FN(int, sched_getparam)(std::int32_t pid, guest::SchedParam* param) noexcept {
  if (param == nullptr || pid < 0) return FailWith(guest::kEINVAL, -1);
  int priority = 0;
  if (!internal::WithSchedTarget(pid, [&](HANDLE, PthreadInternal* thread) {
        if (thread != nullptr) priority = thread->sched_priority;
      })) {
    return FailWith(guest::kESRCH, -1);
  }
  param->sched_priority = priority;
  return 0;
}

ELIXIR_BIONIC_FN(int, sched_setparam)(std::int32_t pid, const guest::SchedParam* param) noexcept {
  if (param == nullptr || pid < 0) return FailWith(guest::kEINVAL, -1);
  int result = 0;
  if (!internal::WithSchedTarget(pid, [&](HANDLE, PthreadInternal* thread) {
        const int policy = thread != nullptr ? thread->sched_policy : guest::kSCHED_NORMAL;
        result = internal::ValidateSchedPolicy(policy, param->sched_priority);
        if (result == 0 && thread != nullptr) thread->sched_priority = param->sched_priority;
      })) {
    return FailWith(guest::kESRCH, -1);
  }
  return result == 0 ? 0 : FailWith(result, -1);
}

ELIXIR_BIONIC_FN(int, sched_rr_get_interval)(std::int32_t pid, guest::Timespec* interval) noexcept {
  if (interval == nullptr || pid < 0) return FailWith(guest::kEINVAL, -1);
  if (!internal::WithSchedTarget(pid, [](HANDLE, PthreadInternal*) {})) return FailWith(guest::kESRCH, -1);
  DWORD adjustment = 0;
  DWORD increment = 0;
  BOOL disabled = FALSE;
  std::int64_t slice_ns = 15625000;
  if (GetSystemTimeAdjustment(&adjustment, &increment, &disabled) && increment != 0) {
    slice_ns = static_cast<std::int64_t>(increment) * 100;
  }
  interval->tv_sec = slice_ns / 1000000000;
  interval->tv_nsec = slice_ns % 1000000000;
  return 0;
}

ELIXIR_BIONIC_FN(int, __sched_cpucount)(guest::g_size set_size, const CpuSet* set) noexcept {
  int count = 0;
  const std::size_t words = static_cast<std::size_t>(set_size) / sizeof(std::uint64_t);
  for (std::size_t i = 0; i < words; ++i) count += __builtin_popcountll(set->bits[i]);
  return count;
}

ELIXIR_BIONIC_FN(CpuSet*, __sched_cpualloc)(guest::g_size count) noexcept {
  const std::size_t bytes = static_cast<std::size_t>((count + 63) / 64) * sizeof(std::uint64_t);
  return static_cast<CpuSet*>(elixir::bionic::GuestMalloc(bytes));
}

ELIXIR_BIONIC_FN(void, __sched_cpufree)(CpuSet* set) noexcept { elixir::bionic::GuestFree(set); }

namespace elixir::bionic::threads::internal {

void RegisterSchedExports(ExportTable& table) {
#define ELIXIR_THREADS_EXPORT(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_THREADS_EXPORT(sched_yield);
  ELIXIR_THREADS_EXPORT(sched_getcpu);
  ELIXIR_THREADS_EXPORT(sched_getaffinity);
  ELIXIR_THREADS_EXPORT(sched_setaffinity);
  ELIXIR_THREADS_EXPORT(sched_get_priority_max);
  ELIXIR_THREADS_EXPORT(sched_get_priority_min);
  ELIXIR_THREADS_EXPORT(sched_getscheduler);
  ELIXIR_THREADS_EXPORT(sched_setscheduler);
  ELIXIR_THREADS_EXPORT(sched_getparam);
  ELIXIR_THREADS_EXPORT(sched_setparam);
  ELIXIR_THREADS_EXPORT(sched_rr_get_interval);
  ELIXIR_THREADS_EXPORT(__sched_cpucount);
  ELIXIR_THREADS_EXPORT(__sched_cpualloc);
  ELIXIR_THREADS_EXPORT(__sched_cpufree);
#undef ELIXIR_THREADS_EXPORT
}

}
