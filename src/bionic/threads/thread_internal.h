// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_THREADS_THREAD_INTERNAL_H_
#define ELIXIR_BIONIC_THREADS_THREAD_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "abi/sysv_abi.h"
#include "bionic/guest_constants.h"
#include "bionic/guest_types.h"
#include "bionic/runtime.h"
#include "bionic/threads.h"

namespace elixir::bionic::threads::internal {

inline constexpr int kPthreadMutexNormal = 0;
inline constexpr int kPthreadMutexRecursive = 1;
inline constexpr int kPthreadMutexErrorcheck = 2;
inline constexpr int kPthreadRwlockPreferReaderNp = 0;
inline constexpr int kPthreadRwlockPreferWriterNonrecursiveNp = 1;
inline constexpr int kSigBlock = 0;
inline constexpr int kSigUnblock = 1;
inline constexpr int kSigSetmask = 2;
inline constexpr int kSigRtMinKernel = 32;
inline constexpr unsigned kSemValueMax = 0x3fffffffu;
inline constexpr std::int32_t kFutexWaiters = static_cast<std::int32_t>(0x80000000u);
inline constexpr std::int32_t kFutexTidMask = 0x3fffffff;
inline constexpr int kSchedResetOnFork = 0x40000000;
inline constexpr std::size_t kGuestPageSize = 4096;
inline constexpr std::size_t kDefaultStackSize = 1024 * 1024 - 32 * 1024;
inline constexpr std::size_t kDefaultGuardSize = kGuestPageSize;
inline constexpr std::size_t kMaxTaskCommLen = 16;
inline constexpr std::size_t kKeyReservedCount = 2;
inline constexpr std::size_t kKeyCount = kKeyReservedCount + 128;
inline constexpr std::uint32_t kKeyValidFlag = 0x80000000u;
inline constexpr int kAndroidLogWarn = 5;
inline constexpr int kAndroidLogFatal = 7;

std::int64_t MonotonicNowNs() noexcept;
std::int64_t RealtimeNowNs() noexcept;

int CheckTimespec(const guest::Timespec* ts, bool null_allowed) noexcept;

std::int64_t RelativeTimeoutNs(const guest::Timespec* deadline, bool realtime) noexcept;

int FutexWaitAbsolute(volatile void* word, std::uint32_t expected, bool realtime,
                      const guest::Timespec* deadline) noexcept;
inline int FutexWaitWord(volatile void* word, std::uint32_t expected) noexcept {
  return FutexWait(static_cast<volatile std::int32_t*>(word), static_cast<std::int32_t>(expected), -1);
}
inline void FutexWakeWord(volatile void* word, bool all) noexcept {
  FutexWake(static_cast<volatile std::int32_t*>(word), all);
}

void SleepNs(std::int64_t ns) noexcept;

struct GuestLock {
  std::int32_t state;
  bool process_shared;
  std::uint8_t pad[3];
};
static_assert(sizeof(GuestLock) == 8, "bionic Lock is two words");
void LockInit(GuestLock* lock, bool process_shared) noexcept;
bool LockTry(GuestLock* lock) noexcept;
void LockAcquire(GuestLock* lock) noexcept;
void LockRelease(GuestLock* lock) noexcept;

enum ThreadJoinState : std::int32_t {
  kThreadNotJoined = 0,
  kThreadExitedNotJoined = 1,
  kThreadJoined = 2,
  kThreadDetached = 3,
};

struct CleanupFrame {
  CleanupFrame* prev;
  abi::SysvFn<void(void*)> routine;
  void* arg;
};
static_assert(sizeof(CleanupFrame) == 24, "__pthread_cleanup_t is 24 bytes on LP64");

struct ThreadLocalDtor {
  abi::SysvFn<void(void*)> fn;
  void* arg;
  void* dso_handle;
  ThreadLocalDtor* next;
};

struct PthreadInternal {
  PthreadInternal* next;
  PthreadInternal* prev;
  std::int32_t tid;
  std::uint32_t cached_pid;
  guest::PthreadAttr attr;
  std::int32_t join_state;
  CleanupFrame* cleanup_stack;
  abi::SysvFn<void*(void*)> start_routine;
  void* start_routine_arg;
  void* return_value;
  guest::Sigset start_mask;

  abi::SysvFn<int(void*)> c11_start_routine;
  void* handle;
  std::uint32_t windows_tid;
  bool adopted;
  std::int32_t started;
  ThreadLocalDtor* thread_local_dtors;
  std::int32_t sched_policy;
  std::int32_t sched_priority;
  bool has_name;
  char name[kMaxTaskCommLen];
};
static_assert(offsetof(PthreadInternal, tid) == 16, "pthread_internal_t::tid");
static_assert(offsetof(PthreadInternal, attr) == 24, "pthread_internal_t::attr");
static_assert(offsetof(PthreadInternal, join_state) == 80, "pthread_internal_t::join_state");
static_assert(offsetof(PthreadInternal, cleanup_stack) == 88, "pthread_internal_t::cleanup_stack");
static_assert(offsetof(PthreadInternal, start_routine) == 96, "pthread_internal_t::start_routine");
static_assert(offsetof(PthreadInternal, return_value) == 112, "pthread_internal_t::return_value");

PthreadInternal* Self() noexcept;
PthreadInternal* CurrentThreadIfAny() noexcept;

bool WithThread(guest::PthreadT thread, void (*fn)(PthreadInternal*, void*), void* context) noexcept;
template <typename F>
bool WithThreadDo(guest::PthreadT thread, F&& f) noexcept {
  using Callable = std::remove_reference_t<F>;
  return WithThread(
      thread, [](PthreadInternal* t, void* ctx) { (*static_cast<Callable*>(ctx))(t); },
      const_cast<void*>(static_cast<const void*>(&f)));
}

bool WithThreadByTid(std::int32_t tid, void (*fn)(PthreadInternal*, void*), void* context) noexcept;
template <typename F>
bool WithThreadByTidDo(std::int32_t tid, F&& f) noexcept {
  using Callable = std::remove_reference_t<F>;
  return WithThreadByTid(
      tid, [](PthreadInternal* t, void* ctx) { (*static_cast<Callable*>(ctx))(t); },
      const_cast<void*>(static_cast<const void*>(&f)));
}

std::uint32_t WindowsTidFromGuestTid(std::int32_t tid) noexcept;

int CreateGuestThread(guest::PthreadT* out, const guest::PthreadAttr* attr,
                      abi::SysvFn<void*(void*)> start_routine, abi::SysvFn<int(void*)> c11_start,
                      void* arg) noexcept;
int JoinThread(guest::PthreadT thread, void** return_value) noexcept;
[[noreturn]] void ExitCurrentThread(void* return_value) noexcept;
void InitDefaultAttr(guest::PthreadAttr* attr) noexcept;

int ValidateSchedPolicy(int policy, int priority) noexcept;
void ApplySchedPolicy(void* handle, int old_policy, int new_policy) noexcept;

void RunKeyDestructors() noexcept;
bool HasKeyValues() noexcept;

[[noreturn]] void FatalError(const char* message) noexcept;

void RegisterLifecycleExports(ExportTable& table);
void RegisterKeyExports(ExportTable& table);
void RegisterMutexExports(ExportTable& table);
void RegisterCondExports(ExportTable& table);
void RegisterRwlockExports(ExportTable& table);
void RegisterSyncMiscExports(ExportTable& table);
void RegisterSchedExports(ExportTable& table);
void RegisterC11Exports(ExportTable& table);
void RegisterLibcInternalExports(ExportTable& table);

void NotifyLibcThreadExitCallbacks() noexcept;

int MutexLock(guest::PthreadMutex* mutex) noexcept;
int MutexUnlock(guest::PthreadMutex* mutex) noexcept;

}

#endif
