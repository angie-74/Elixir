// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <cstdint>

#include "bionic/threads/thread_internal.h"

namespace {
namespace internal = elixir::bionic::threads::internal;
namespace guest = elixir::bionic::guest;
using elixir::abi::SysvFn;

constexpr int kMtxPlain = 0x1;
constexpr int kMtxRecursive = 0x2;
constexpr int kMtxTimed = 0x4;
constexpr int kThrdSuccess = 0;
constexpr int kThrdBusy = 1;
constexpr int kThrdError = 2;
constexpr int kThrdNomem = 3;
constexpr int kThrdTimedout = 4;

int ThrdError(int pthread_code) noexcept {
  switch (pthread_code) {
    case 0: return kThrdSuccess;
    case guest::kENOMEM: return kThrdNomem;
    case guest::kETIMEDOUT: return kThrdTimedout;
    case guest::kEBUSY: return kThrdBusy;
    default: return kThrdError;
  }
}

}

extern "C" ELIXIR_SYSV int elixir_bionic_pthread_detach(guest::PthreadT) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_mutexattr_init(guest::PthreadMutexattr*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_mutexattr_settype(guest::PthreadMutexattr*, int) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_mutex_init(guest::PthreadMutex*, const guest::PthreadMutexattr*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_mutex_destroy(guest::PthreadMutex*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_mutex_trylock(guest::PthreadMutex*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_mutex_timedlock(guest::PthreadMutex*, const guest::Timespec*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_cond_init(guest::PthreadCond*, const guest::PthreadCondattr*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_cond_destroy(guest::PthreadCond*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_cond_signal(guest::PthreadCond*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_cond_broadcast(guest::PthreadCond*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_cond_wait(guest::PthreadCond*, guest::PthreadMutex*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_cond_timedwait(guest::PthreadCond*, guest::PthreadMutex*,
                                                                const guest::Timespec*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_key_create(guest::PthreadKey*, SysvFn<void(void*)>) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_key_delete(guest::PthreadKey) noexcept;
extern "C" ELIXIR_SYSV void* elixir_bionic_pthread_getspecific(guest::PthreadKey) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_setspecific(guest::PthreadKey, const void*) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_pthread_once(guest::PthreadOnce*, SysvFn<void()>) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_sched_yield() noexcept;

ELIXIR_BIONIC_FN(int, thrd_create)(guest::PthreadT* thread, SysvFn<int(void*)> function, void* arg) noexcept {
  return ThrdError(internal::CreateGuestThread(thread, nullptr, nullptr, function, arg));
}

ELIXIR_BIONIC_FN(guest::PthreadT, thrd_current)() noexcept {
  return reinterpret_cast<guest::PthreadT>(internal::Self());
}

ELIXIR_BIONIC_FN(int, thrd_detach)(guest::PthreadT thread) noexcept {
  return ThrdError(elixir_bionic_pthread_detach(thread));
}

ELIXIR_BIONIC_FN(int, thrd_equal)(guest::PthreadT lhs, guest::PthreadT rhs) noexcept { return lhs == rhs ? 1 : 0; }

ELIXIR_BIONIC_FN(void, thrd_exit)(int result) noexcept {
  internal::ExitCurrentThread(reinterpret_cast<void*>(static_cast<std::intptr_t>(result)));
}

ELIXIR_BIONIC_FN(int, thrd_join)(guest::PthreadT thread, int* result) noexcept {
  void* value = nullptr;
  if (internal::JoinThread(thread, &value) != 0) return kThrdError;
  if (result != nullptr) *result = static_cast<int>(reinterpret_cast<std::intptr_t>(value));
  return kThrdSuccess;
}

ELIXIR_BIONIC_FN(int, thrd_sleep)(const guest::Timespec* duration, guest::Timespec* remaining) noexcept {
  if (duration == nullptr || duration->tv_nsec < 0 || duration->tv_nsec >= 1000000000 || duration->tv_sec < 0) {
    elixir::bionic::SetGuestErrno(guest::kEINVAL);
    return -2;
  }
  constexpr std::int64_t kMaxSeconds = INT64_MAX / 1000000000 - 1;
  const std::int64_t seconds = duration->tv_sec > kMaxSeconds ? kMaxSeconds : duration->tv_sec;
  internal::SleepNs(seconds * 1000000000 + duration->tv_nsec);
  if (remaining != nullptr) {
    remaining->tv_sec = 0;
    remaining->tv_nsec = 0;
  }
  return 0;
}

ELIXIR_BIONIC_FN(void, thrd_yield)() noexcept { elixir_bionic_sched_yield(); }

ELIXIR_BIONIC_FN(int, mtx_init)(guest::PthreadMutex* mutex, int type) noexcept {
  const int pthread_type = (type & kMtxRecursive) != 0 ? internal::kPthreadMutexRecursive : internal::kPthreadMutexNormal;
  type &= ~kMtxRecursive;
  if (type != kMtxPlain && type != kMtxTimed) return kThrdError;
  guest::PthreadMutexattr attr;
  elixir_bionic_pthread_mutexattr_init(&attr);
  elixir_bionic_pthread_mutexattr_settype(&attr, pthread_type);
  return ThrdError(elixir_bionic_pthread_mutex_init(mutex, &attr));
}

ELIXIR_BIONIC_FN(void, mtx_destroy)(guest::PthreadMutex* mutex) noexcept { elixir_bionic_pthread_mutex_destroy(mutex); }

ELIXIR_BIONIC_FN(int, mtx_lock)(guest::PthreadMutex* mutex) noexcept { return ThrdError(internal::MutexLock(mutex)); }

ELIXIR_BIONIC_FN(int, mtx_timedlock)(guest::PthreadMutex* mutex, const guest::Timespec* deadline) noexcept {
  return ThrdError(elixir_bionic_pthread_mutex_timedlock(mutex, deadline));
}

ELIXIR_BIONIC_FN(int, mtx_trylock)(guest::PthreadMutex* mutex) noexcept {
  return ThrdError(elixir_bionic_pthread_mutex_trylock(mutex));
}

ELIXIR_BIONIC_FN(int, mtx_unlock)(guest::PthreadMutex* mutex) noexcept { return ThrdError(internal::MutexUnlock(mutex)); }

ELIXIR_BIONIC_FN(int, cnd_init)(guest::PthreadCond* cond) noexcept {
  return ThrdError(elixir_bionic_pthread_cond_init(cond, nullptr));
}

ELIXIR_BIONIC_FN(void, cnd_destroy)(guest::PthreadCond* cond) noexcept { elixir_bionic_pthread_cond_destroy(cond); }

ELIXIR_BIONIC_FN(int, cnd_signal)(guest::PthreadCond* cond) noexcept {
  return ThrdError(elixir_bionic_pthread_cond_signal(cond));
}

ELIXIR_BIONIC_FN(int, cnd_broadcast)(guest::PthreadCond* cond) noexcept {
  return ThrdError(elixir_bionic_pthread_cond_broadcast(cond));
}

ELIXIR_BIONIC_FN(int, cnd_wait)(guest::PthreadCond* cond, guest::PthreadMutex* mutex) noexcept {
  return ThrdError(elixir_bionic_pthread_cond_wait(cond, mutex));
}

ELIXIR_BIONIC_FN(int, cnd_timedwait)(guest::PthreadCond* cond, guest::PthreadMutex* mutex,
                                     const guest::Timespec* deadline) noexcept {
  return ThrdError(elixir_bionic_pthread_cond_timedwait(cond, mutex, deadline));
}

ELIXIR_BIONIC_FN(int, tss_create)(guest::PthreadKey* key, SysvFn<void(void*)> destructor) noexcept {
  return ThrdError(elixir_bionic_pthread_key_create(key, destructor));
}

ELIXIR_BIONIC_FN(void, tss_delete)(guest::PthreadKey key) noexcept { elixir_bionic_pthread_key_delete(key); }

ELIXIR_BIONIC_FN(void*, tss_get)(guest::PthreadKey key) noexcept { return elixir_bionic_pthread_getspecific(key); }

ELIXIR_BIONIC_FN(int, tss_set)(guest::PthreadKey key, void* value) noexcept {
  return ThrdError(elixir_bionic_pthread_setspecific(key, value));
}

ELIXIR_BIONIC_FN(void, call_once)(guest::PthreadOnce* flag, SysvFn<void()> function) noexcept {
  elixir_bionic_pthread_once(flag, function);
}

namespace elixir::bionic::threads::internal {

void RegisterC11Exports(ExportTable& table) {
#define ELIXIR_THREADS_EXPORT(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_THREADS_EXPORT(thrd_create);
  ELIXIR_THREADS_EXPORT(thrd_current);
  ELIXIR_THREADS_EXPORT(thrd_detach);
  ELIXIR_THREADS_EXPORT(thrd_equal);
  ELIXIR_THREADS_EXPORT(thrd_exit);
  ELIXIR_THREADS_EXPORT(thrd_join);
  ELIXIR_THREADS_EXPORT(thrd_sleep);
  ELIXIR_THREADS_EXPORT(thrd_yield);
  ELIXIR_THREADS_EXPORT(mtx_init);
  ELIXIR_THREADS_EXPORT(mtx_destroy);
  ELIXIR_THREADS_EXPORT(mtx_lock);
  ELIXIR_THREADS_EXPORT(mtx_timedlock);
  ELIXIR_THREADS_EXPORT(mtx_trylock);
  ELIXIR_THREADS_EXPORT(mtx_unlock);
  ELIXIR_THREADS_EXPORT(cnd_init);
  ELIXIR_THREADS_EXPORT(cnd_destroy);
  ELIXIR_THREADS_EXPORT(cnd_signal);
  ELIXIR_THREADS_EXPORT(cnd_broadcast);
  ELIXIR_THREADS_EXPORT(cnd_wait);
  ELIXIR_THREADS_EXPORT(cnd_timedwait);
  ELIXIR_THREADS_EXPORT(tss_create);
  ELIXIR_THREADS_EXPORT(tss_delete);
  ELIXIR_THREADS_EXPORT(tss_get);
  ELIXIR_THREADS_EXPORT(tss_set);
  ELIXIR_THREADS_EXPORT(call_once);
#undef ELIXIR_THREADS_EXPORT
}

}
