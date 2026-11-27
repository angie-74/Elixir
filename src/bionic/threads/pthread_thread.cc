// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The attribute functions, join/detach state machine and exit ordering are
// adapted from AOSP bionic libc/bionic/pthread_attr.cpp, pthread_create.cpp,
// pthread_exit.cpp, pthread_join.cpp, pthread_detach.cpp,
// pthread_setname_np.cpp and __cxa_thread_atexit_impl.cpp:
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
//
// __cxa_thread_atexit_impl.cpp is Copyright (C) 2015 The Android Open
// Source Project, Licensed under the Apache License, Version 2.0.
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "bionic/core/core.h"
#include "bionic/threads/thread_internal.h"
#include "platform/windows/guest_tls.h"

namespace elixir::bionic::threads {
namespace internal {
namespace {

void WINAPI OnFlsThreadExit(void* data);

constexpr std::size_t kStackHeadroom = 64 * 1024;
constexpr std::size_t kStackGranularity = 64 * 1024;
constexpr std::size_t kMinimumStack = 64 * 1024;
constexpr ULONG kStackGuarantee = 32 * 1024;

SRWLOCK g_registry_lock = SRWLOCK_INIT;
PthreadInternal* g_threads = nullptr;

const DWORD g_exit_fls = FlsAlloc(&OnFlsThreadExit);

thread_local PthreadInternal* t_self = nullptr;

class SharedRegistryLock {
 public:
  SharedRegistryLock() noexcept { AcquireSRWLockShared(&g_registry_lock); }
  ~SharedRegistryLock() { ReleaseSRWLockShared(&g_registry_lock); }
  SharedRegistryLock(const SharedRegistryLock&) = delete;
  SharedRegistryLock& operator=(const SharedRegistryLock&) = delete;
};

void RegistryAdd(PthreadInternal* thread) noexcept {
  AcquireSRWLockExclusive(&g_registry_lock);
  thread->prev = nullptr;
  thread->next = g_threads;
  if (g_threads != nullptr) g_threads->prev = thread;
  g_threads = thread;
  ReleaseSRWLockExclusive(&g_registry_lock);
}

void RegistryRemove(PthreadInternal* thread) noexcept {
  AcquireSRWLockExclusive(&g_registry_lock);
  if (thread->prev != nullptr) thread->prev->next = thread->next;
  if (thread->next != nullptr) thread->next->prev = thread->prev;
  if (g_threads == thread) g_threads = thread->next;
  thread->next = thread->prev = nullptr;
  ReleaseSRWLockExclusive(&g_registry_lock);
}

PthreadInternal* FindLocked(guest::PthreadT id) noexcept {
  if (id == 0) return nullptr;
  for (PthreadInternal* thread = g_threads; thread != nullptr; thread = thread->next) {
    if (reinterpret_cast<guest::PthreadT>(thread) == id) return thread;
  }
  return nullptr;
}

void FreeThreadObject(PthreadInternal* thread) noexcept {
  if (thread->handle != nullptr) CloseHandle(thread->handle);
  delete thread;
}

void RecordStackLimits(PthreadInternal* thread) noexcept {
  ULONG_PTR low = 0;
  ULONG_PTR high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  thread->attr.stack_base = reinterpret_cast<void*>(low);
  thread->attr.stack_size = static_cast<guest::g_size>(high - low);
}

void SetWindowsThreadName(HANDLE handle, const char* utf8) noexcept {
  if (handle == nullptr) return;
  wchar_t wide[kMaxTaskCommLen * 2];
  const int written = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, static_cast<int>(sizeof(wide) / sizeof(wide[0])));
  if (written <= 0) {
    std::size_t i = 0;
    for (; utf8[i] != '\0' && i + 1 < sizeof(wide) / sizeof(wide[0]); ++i) wide[i] = static_cast<unsigned char>(utf8[i]);
    wide[i] = L'\0';
  }
  SetThreadDescription(handle, wide);
}

void DefaultThreadName(char* out) noexcept {
  const std::string& package = Options().package_name;
  const char* name = package.c_str();
  const std::size_t length = package.size();
  const bool has_dot = package.find('.') != std::string::npos;
  const bool has_at = package.find('@') != std::string::npos;
  const char* start = (length < kMaxTaskCommLen - 1 || has_at || !has_dot) ? name : name + length - (kMaxTaskCommLen - 1);
  std::size_t i = 0;
  for (; start[i] != '\0' && i < kMaxTaskCommLen - 1; ++i) out[i] = start[i];
  out[i] = '\0';
}

void ReadThreadName(PthreadInternal* thread, char* out) noexcept {
  if (thread->has_name) {
    std::memcpy(out, thread->name, kMaxTaskCommLen);
    out[kMaxTaskCommLen - 1] = '\0';
    return;
  }
  PWSTR description = nullptr;
  if (thread->handle != nullptr && SUCCEEDED(GetThreadDescription(thread->handle, &description)) &&
      description != nullptr) {
    char utf8[64];
    const int bytes = description[0] == L'\0'
                          ? 0
                          : WideCharToMultiByte(CP_UTF8, 0, description, -1, utf8, sizeof(utf8), nullptr, nullptr);
    LocalFree(description);
    if (bytes > 1) {
      std::size_t i = 0;
      for (; utf8[i] != '\0' && i < kMaxTaskCommLen - 1; ++i) out[i] = utf8[i];
      out[i] = '\0';
      return;
    }
  }
  DefaultThreadName(out);
}

PthreadInternal* AdoptCurrentThread() noexcept {
  auto* thread = new (std::nothrow) PthreadInternal{};
  if (thread == nullptr) return nullptr;
  thread->tid = CurrentTid();
  thread->cached_pid = GetCurrentProcessId();
  InitDefaultAttr(&thread->attr);
  RecordStackLimits(thread);
  thread->started = 1;
  thread->join_state = kThreadDetached;
  thread->adopted = true;
  thread->windows_tid = GetCurrentThreadId();
  HANDLE handle = nullptr;
  if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle, 0, FALSE,
                      DUPLICATE_SAME_ACCESS)) {
    thread->handle = handle;
  }
  thread->sched_policy = guest::kSCHED_NORMAL;
  thread->start_mask = elixir::bionic::core::CurrentSignalMask();
  RegistryAdd(thread);
  t_self = thread;
  FlsSetValue(g_exit_fls, thread);
  if (platform::GuestThreadState* state = platform::CurrentGuestThread()) state->set_owner(thread);
  return thread;
}

void RunThreadExitHandlers(PthreadInternal* thread, void* return_value) noexcept {
  while (ThreadLocalDtor* dtor = thread->thread_local_dtors) {
    thread->thread_local_dtors = dtor->next;
    dtor->fn(dtor->arg);
    delete dtor;
  }
  thread->return_value = return_value;
  while (CleanupFrame* frame = thread->cleanup_stack) {
    thread->cleanup_stack = frame->prev;
    frame->routine(frame->arg);
  }
  RunKeyDestructors();
}

void FinishCurrentThread(PthreadInternal* thread, void* return_value) noexcept {
  RunThreadExitHandlers(thread, return_value);
  NotifyLibcThreadExitCallbacks();
  platform::ReleaseCurrentGuestThread();
  FlsSetValue(g_exit_fls, nullptr);
  t_self = nullptr;
  std::int32_t old_state = kThreadNotJoined;
  while (old_state == kThreadNotJoined &&
         !__atomic_compare_exchange_n(&thread->join_state, &old_state, kThreadExitedNotJoined, true,
                                      __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
  }
  if (old_state == kThreadDetached) {
    RegistryRemove(thread);
    FreeThreadObject(thread);
  } else {
    __atomic_store_n(&thread->tid, 0, __ATOMIC_RELEASE);
  }
}

void WINAPI OnFlsThreadExit(void* data) {
  auto* thread = static_cast<PthreadInternal*>(data);
  if (thread == nullptr || thread != t_self) return;
  const bool guest_work = thread->thread_local_dtors != nullptr || thread->cleanup_stack != nullptr || HasKeyValues();
  if (guest_work && platform::CurrentGuestThread() == nullptr && platform::GuestTlsAvailable()) {
    platform::EnsureGuestThread();
  }
  FinishCurrentThread(thread, nullptr);
}

DWORD WINAPI GuestThreadMain(void* parameter) {
  auto* thread = static_cast<PthreadInternal*>(parameter);
  t_self = thread;
  elixir::bionic::core::SetSignalMask(thread->start_mask);
  ULONG guarantee = kStackGuarantee;
  SetThreadStackGuarantee(&guarantee);
  RecordStackLimits(thread);
  __atomic_store_n(&thread->started, 1, __ATOMIC_RELEASE);
  FutexWake(&thread->started, true);
  FlsSetValue(g_exit_fls, thread);
  if (platform::GuestThreadState* state = platform::EnsureGuestThread()) state->set_owner(thread);
  void* result;
  if (thread->c11_start_routine != nullptr) {
    const int value = thread->c11_start_routine(thread->start_routine_arg);
    result = reinterpret_cast<void*>(static_cast<std::intptr_t>(value));
  } else {
    result = thread->start_routine(thread->start_routine_arg);
  }
  FinishCurrentThread(thread, result);
  return 0;
}

std::size_t AlignUp(std::size_t value, std::size_t alignment) noexcept {
  return (value + alignment - 1) & ~(alignment - 1);
}

guest::PthreadT ToHandle(PthreadInternal* thread) noexcept { return reinterpret_cast<guest::PthreadT>(thread); }


}

PthreadInternal* CurrentThreadIfAny() noexcept { return t_self; }

PthreadInternal* Self() noexcept {
  PthreadInternal* self = t_self;
  if (self != nullptr) return self;
  return AdoptCurrentThread();
}

bool WithThread(guest::PthreadT thread, void (*fn)(PthreadInternal*, void*), void* context) noexcept {
  SharedRegistryLock lock;
  PthreadInternal* found = FindLocked(thread);
  if (found == nullptr) return false;
  fn(found, context);
  return true;
}

bool WithThreadByTid(std::int32_t tid, void (*fn)(PthreadInternal*, void*), void* context) noexcept {
  if (tid == 0) {
    PthreadInternal* self = Self();
    if (self == nullptr) return false;
    SharedRegistryLock lock;
    fn(self, context);
    return true;
  }
  SharedRegistryLock lock;
  for (PthreadInternal* thread = g_threads; thread != nullptr; thread = thread->next) {
    if (__atomic_load_n(&thread->tid, __ATOMIC_ACQUIRE) == tid) {
      fn(thread, context);
      return true;
    }
  }
  return false;
}

void InitDefaultAttr(guest::PthreadAttr* attr) noexcept {
  std::memset(attr, 0, sizeof(*attr));
  attr->flags = 0;
  attr->stack_base = nullptr;
  attr->stack_size = kDefaultStackSize;
  attr->guard_size = kDefaultGuardSize;
  attr->sched_policy = guest::kSCHED_NORMAL;
  attr->sched_priority = 0;
}

int CreateGuestThread(guest::PthreadT* out, const guest::PthreadAttr* attr,
                      abi::SysvFn<void*(void*)> start_routine, abi::SysvFn<int(void*)> c11_start,
                      void* arg) noexcept {
  if (out == nullptr || (start_routine == nullptr && c11_start == nullptr)) return guest::kEINVAL;
  guest::PthreadAttr thread_attr;
  if (attr == nullptr) {
    InitDefaultAttr(&thread_attr);
  } else {
    thread_attr = *attr;
  }
  if (thread_attr.stack_base == nullptr) {
    const std::size_t guard = AlignUp(thread_attr.guard_size, kGuestPageSize);
    if (guard < thread_attr.guard_size) return guest::kEAGAIN;
    thread_attr.guard_size = guard;
  } else {
    thread_attr.flags |= guest::kPthreadAttrFlagUserAllocatedStack;
  }

  PthreadInternal* parent = Self();
  int child_policy = parent != nullptr ? parent->sched_policy : guest::kSCHED_NORMAL;
  int child_priority = parent != nullptr ? parent->sched_priority : 0;
  bool need_set;
  if ((thread_attr.flags & guest::kPthreadAttrFlagInheritSched) != 0) {
    need_set = false;
  } else {
    need_set = true;
  }
  if ((thread_attr.flags & (guest::kPthreadAttrFlagInheritSched | guest::kPthreadAttrFlagExplicitSched)) == 0) {
    need_set = thread_attr.sched_policy != guest::kSCHED_NORMAL;
  }
  if (need_set) {
    const int policy = thread_attr.sched_policy & ~kSchedResetOnFork;
    const int result = ValidateSchedPolicy(policy, thread_attr.sched_priority);
    if (result != 0) return result;
    child_policy = policy;
    child_priority = thread_attr.sched_priority;
  }

  std::size_t stack = thread_attr.stack_size;
  if (stack < kMinimumStack) stack = kMinimumStack;
  if (stack > SIZE_MAX / 4) return guest::kEAGAIN;
  const std::size_t commit = AlignUp(stack + kStackHeadroom, kStackGranularity);

  auto* thread = new (std::nothrow) PthreadInternal{};
  if (thread == nullptr) return guest::kEAGAIN;
  thread->attr = thread_attr;
  thread->join_state = (thread_attr.flags & guest::kPthreadAttrFlagDetached) != 0 ? kThreadDetached : kThreadNotJoined;
  thread->start_routine = start_routine;
  thread->c11_start_routine = c11_start;
  thread->start_routine_arg = arg;
  thread->cached_pid = GetCurrentProcessId();
  thread->start_mask = elixir::bionic::core::CurrentSignalMask();
  thread->sched_policy = child_policy;
  thread->sched_priority = child_priority;
  if (parent != nullptr && parent->has_name) {
    std::memcpy(thread->name, parent->name, kMaxTaskCommLen);
    thread->has_name = true;
  }

  DWORD windows_tid = 0;
  HANDLE handle = CreateThread(nullptr, commit, &GuestThreadMain, thread, CREATE_SUSPENDED, &windows_tid);
  if (handle == nullptr) {
    delete thread;
    return guest::kEAGAIN;
  }
  thread->handle = handle;
  thread->windows_tid = windows_tid;
  thread->tid = static_cast<std::int32_t>(windows_tid);
  const int parent_priority = GetThreadPriority(GetCurrentThread());
  if (parent_priority != THREAD_PRIORITY_ERROR_RETURN) SetThreadPriority(handle, parent_priority);
  if (need_set) ApplySchedPolicy(handle, parent != nullptr ? parent->sched_policy : guest::kSCHED_NORMAL, child_policy);
  if (thread->has_name) SetWindowsThreadName(handle, thread->name);
  RegistryAdd(thread);
  *out = ToHandle(thread);
  ResumeThread(handle);
  return 0;
}

int JoinThread(guest::PthreadT thread, void** return_value) noexcept {
  PthreadInternal* self = Self();
  if (self != nullptr && thread == ToHandle(self)) return guest::kEDEADLK;
  PthreadInternal* target = nullptr;
  std::int32_t old_state = kThreadNotJoined;
  const bool found = WithThreadDo(thread, [&](PthreadInternal* candidate) {
    target = candidate;
    while ((old_state == kThreadNotJoined || old_state == kThreadExitedNotJoined) &&
           !__atomic_compare_exchange_n(&candidate->join_state, &old_state, kThreadJoined, true, __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
    }
  });
  if (!found) return guest::kESRCH;
  if (old_state == kThreadDetached || old_state == kThreadJoined) return guest::kEINVAL;
  if (target->handle != nullptr) WaitForSingleObject(target->handle, INFINITE);
  if (return_value != nullptr) *return_value = target->return_value;
  RegistryRemove(target);
  FreeThreadObject(target);
  return 0;
}

void ExitCurrentThread(void* return_value) noexcept {
  if (PthreadInternal* self = Self()) {
    FinishCurrentThread(self, return_value);
  } else {
    RunKeyDestructors();
    platform::ReleaseCurrentGuestThread();
  }
  ExitThread(0);
}

}

void AddThreadExitCallback(abi::SysvFn<void(void*)> fn, void* arg, void* dso_handle) noexcept {
  internal::PthreadInternal* self = internal::Self();
  if (self == nullptr || fn == nullptr) return;
  auto* dtor = new (std::nothrow) internal::ThreadLocalDtor{fn, arg, dso_handle, self->thread_local_dtors};
  if (dtor == nullptr) {
    HostLog(internal::kAndroidLogWarn, "libc", "__cxa_thread_atexit_impl: out of memory");
    return;
  }
  self->thread_local_dtors = dtor;
}

}

namespace {

namespace abi = elixir::abi;
namespace internal = elixir::bionic::threads::internal;
namespace guest = elixir::bionic::guest;
using elixir::abi::SysvFn;
using internal::PthreadInternal;

struct TlsIndex {
  guest::g_size module_id;
  guest::g_size offset;
};

}

ELIXIR_BIONIC_FN(int, pthread_create)(guest::PthreadT* thread_out, const guest::PthreadAttr* attr,
                                      SysvFn<void*(void*)> start_routine, void* arg) noexcept {
  return internal::CreateGuestThread(thread_out, attr, start_routine, nullptr, arg);
}

ELIXIR_BIONIC_FN(void, pthread_exit)(void* return_value) noexcept { internal::ExitCurrentThread(return_value); }

ELIXIR_BIONIC_FN(int, pthread_join)(guest::PthreadT thread, void** return_value) noexcept {
  const elixir::platform::GuestThreadResume resume;
  return internal::JoinThread(thread, return_value);
}

ELIXIR_BIONIC_FN(int, pthread_detach)(guest::PthreadT thread) noexcept {
  std::int32_t old_state = internal::kThreadNotJoined;
  const bool found = internal::WithThreadDo(thread, [&](PthreadInternal* target) {
    while (old_state == internal::kThreadNotJoined &&
           !__atomic_compare_exchange_n(&target->join_state, &old_state, internal::kThreadDetached, true,
                                        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    }
  });
  if (!found) return guest::kESRCH;
  if (old_state == internal::kThreadNotJoined) return 0;
  if (old_state == internal::kThreadExitedNotJoined) return internal::JoinThread(thread, nullptr);
  return guest::kEINVAL;
}

ELIXIR_BIONIC_FN(guest::PthreadT, pthread_self)() noexcept {
  return reinterpret_cast<guest::PthreadT>(internal::Self());
}

ELIXIR_BIONIC_FN(int, pthread_equal)(guest::PthreadT lhs, guest::PthreadT rhs) noexcept { return lhs == rhs ? 1 : 0; }

ELIXIR_BIONIC_FN(int, pthread_kill)(guest::PthreadT thread, int signal) noexcept {
  std::int32_t tid = 0;
  if (!internal::WithThreadDo(thread, [&](PthreadInternal* target) { tid = __atomic_load_n(&target->tid, __ATOMIC_ACQUIRE); })) {
    return guest::kESRCH;
  }
  if (tid == 0) return guest::kESRCH;
  if (signal < 0 || signal >= guest::kNSIG) return guest::kEINVAL;
  if (signal == 0) return 0;
  return elixir::bionic::core::SendGuestSignalToThread(tid, signal);
}

ELIXIR_BIONIC_FN(int, pthread_getattr_np)(guest::PthreadT thread, guest::PthreadAttr* attr) noexcept {
  if (attr == nullptr) return guest::kEINVAL;
  const bool found = internal::WithThreadDo(thread, [&](PthreadInternal* target) {
    while (__atomic_load_n(&target->started, __ATOMIC_ACQUIRE) == 0) {
      elixir::bionic::threads::FutexWait(&target->started, 0, -1);
    }
    *attr = target->attr;
    if (__atomic_load_n(&target->join_state, __ATOMIC_ACQUIRE) == internal::kThreadDetached) {
      attr->flags |= guest::kPthreadAttrFlagDetached;
    }
  });
  return found ? 0 : guest::kESRCH;
}

ELIXIR_BIONIC_FN(int, pthread_attr_init)(guest::PthreadAttr* attr) noexcept {
  internal::InitDefaultAttr(attr);
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_destroy)(guest::PthreadAttr* attr) noexcept {
  std::memset(attr, 0x42, sizeof(*attr));
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_setinheritsched)(guest::PthreadAttr* attr, int flag) noexcept {
  if (flag == guest::kPTHREAD_EXPLICIT_SCHED) {
    attr->flags &= ~guest::kPthreadAttrFlagInheritSched;
    attr->flags |= guest::kPthreadAttrFlagExplicitSched;
  } else if (flag == guest::kPTHREAD_INHERIT_SCHED) {
    attr->flags |= guest::kPthreadAttrFlagInheritSched;
    attr->flags &= ~guest::kPthreadAttrFlagExplicitSched;
  } else {
    return guest::kEINVAL;
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_getinheritsched)(const guest::PthreadAttr* attr, int* flag) noexcept {
  if ((attr->flags & guest::kPthreadAttrFlagInheritSched) != 0) {
    *flag = guest::kPTHREAD_INHERIT_SCHED;
  } else if ((attr->flags & guest::kPthreadAttrFlagExplicitSched) != 0) {
    *flag = guest::kPTHREAD_EXPLICIT_SCHED;
  } else {
    *flag = attr->sched_policy != guest::kSCHED_NORMAL ? guest::kPTHREAD_EXPLICIT_SCHED : guest::kPTHREAD_INHERIT_SCHED;
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_setdetachstate)(guest::PthreadAttr* attr, int state) noexcept {
  if (state == guest::kPTHREAD_CREATE_DETACHED) {
    attr->flags |= guest::kPthreadAttrFlagDetached;
  } else if (state == guest::kPTHREAD_CREATE_JOINABLE) {
    attr->flags &= ~guest::kPthreadAttrFlagDetached;
  } else {
    return guest::kEINVAL;
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_getdetachstate)(const guest::PthreadAttr* attr, int* state) noexcept {
  *state = (attr->flags & guest::kPthreadAttrFlagDetached) != 0 ? guest::kPTHREAD_CREATE_DETACHED
                                                                : guest::kPTHREAD_CREATE_JOINABLE;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_setschedpolicy)(guest::PthreadAttr* attr, int policy) noexcept {
  attr->sched_policy = policy;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_getschedpolicy)(const guest::PthreadAttr* attr, int* policy) noexcept {
  *policy = attr->sched_policy;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_setschedparam)(guest::PthreadAttr* attr, const guest::SchedParam* param) noexcept {
  attr->sched_priority = param->sched_priority;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_getschedparam)(const guest::PthreadAttr* attr, guest::SchedParam* param) noexcept {
  param->sched_priority = attr->sched_priority;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_setstacksize)(guest::PthreadAttr* attr, guest::g_size stack_size) noexcept {
  if (stack_size < static_cast<guest::g_size>(guest::kPTHREAD_STACK_MIN)) return guest::kEINVAL;
  attr->stack_size = stack_size;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_getstack)(const guest::PthreadAttr* attr, void** stack_base,
                                             guest::g_size* stack_size) noexcept {
  *stack_base = attr->stack_base;
  *stack_size = attr->stack_size;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_getstacksize)(const guest::PthreadAttr* attr, guest::g_size* stack_size) noexcept {
  *stack_size = attr->stack_size;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_setstack)(guest::PthreadAttr* attr, void* stack_base, guest::g_size stack_size) noexcept {
  const guest::g_size page_mask = internal::kGuestPageSize - 1;
  if ((stack_size & page_mask) != 0 || stack_size < static_cast<guest::g_size>(guest::kPTHREAD_STACK_MIN)) {
    return guest::kEINVAL;
  }
  if ((reinterpret_cast<std::uintptr_t>(stack_base) & page_mask) != 0) return guest::kEINVAL;
  attr->stack_base = stack_base;
  attr->stack_size = stack_size;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_setguardsize)(guest::PthreadAttr* attr, guest::g_size guard_size) noexcept {
  attr->guard_size = guard_size;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_getguardsize)(const guest::PthreadAttr* attr, guest::g_size* guard_size) noexcept {
  *guard_size = attr->guard_size;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_attr_setscope)(guest::PthreadAttr*, int scope) noexcept {
  if (scope == guest::kPTHREAD_SCOPE_SYSTEM) return 0;
  if (scope == guest::kPTHREAD_SCOPE_PROCESS) return guest::kENOTSUP;
  return guest::kEINVAL;
}

ELIXIR_BIONIC_FN(int, pthread_attr_getscope)(const guest::PthreadAttr*, int* scope) noexcept {
  *scope = guest::kPTHREAD_SCOPE_SYSTEM;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_setname_np)(guest::PthreadT thread, const char* name) noexcept {
  if (name == nullptr) return guest::kEINVAL;
  const std::size_t length = std::strlen(name);
  if (length >= internal::kMaxTaskCommLen) return guest::kERANGE;
  if (internal::CurrentThreadIfAny() == nullptr) internal::Self();
  const bool found = internal::WithThreadDo(thread, [&](PthreadInternal* target) {
    std::memcpy(target->name, name, length + 1);
    target->has_name = true;
    internal::SetWindowsThreadName(target->handle, target->name);
  });
  return found ? 0 : guest::kENOENT;
}

ELIXIR_BIONIC_FN(int, pthread_getname_np)(guest::PthreadT thread, char* buffer, guest::g_size buffer_size) noexcept {
  if (buffer_size < internal::kMaxTaskCommLen) return guest::kERANGE;
  if (internal::CurrentThreadIfAny() == nullptr) internal::Self();
  const bool found = internal::WithThreadDo(thread, [&](PthreadInternal* target) { internal::ReadThreadName(target, buffer); });
  return found ? 0 : guest::kENOENT;
}

ELIXIR_BIONIC_FN(std::int32_t, pthread_gettid_np)(guest::PthreadT thread) noexcept {
  std::int32_t tid = -1;
  internal::WithThreadDo(thread, [&](PthreadInternal* target) { tid = __atomic_load_n(&target->tid, __ATOMIC_ACQUIRE); });
  return tid;
}

ELIXIR_BIONIC_FN(std::int32_t, gettid)() noexcept { return elixir::bionic::threads::CurrentTid(); }

ELIXIR_BIONIC_FN(int, pthread_getcpuclockid)(guest::PthreadT thread, abi::g_clockid* clock_id) noexcept {
  std::int32_t tid = -1;
  internal::WithThreadDo(thread, [&](PthreadInternal* target) { tid = __atomic_load_n(&target->tid, __ATOMIC_ACQUIRE); });
  if (tid == -1) return guest::kESRCH;
  abi::g_clockid result = static_cast<abi::g_clockid>(static_cast<std::uint32_t>(~tid) << 3);
  result |= 2 | 4;
  *clock_id = result;
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_setschedparam)(guest::PthreadT thread, int policy, const guest::SchedParam* param) noexcept {
  int result = 0;
  const bool found = internal::WithThreadDo(thread, [&](PthreadInternal* target) {
    if (param == nullptr) {
      result = guest::kEINVAL;
      return;
    }
    const int base_policy = policy & ~internal::kSchedResetOnFork;
    result = internal::ValidateSchedPolicy(base_policy, param->sched_priority);
    if (result != 0) return;
    internal::ApplySchedPolicy(target->handle, target->sched_policy, base_policy);
    target->sched_policy = base_policy;
    target->sched_priority = param->sched_priority;
  });
  return found ? result : guest::kESRCH;
}

ELIXIR_BIONIC_FN(int, pthread_getschedparam)(guest::PthreadT thread, int* policy, guest::SchedParam* param) noexcept {
  const bool found = internal::WithThreadDo(thread, [&](PthreadInternal* target) {
    *policy = target->sched_policy;
    param->sched_priority = target->sched_priority;
  });
  return found ? 0 : guest::kESRCH;
}

ELIXIR_BIONIC_FN(int, pthread_setschedprio)(guest::PthreadT thread, int priority) noexcept {
  int result = 0;
  const bool found = internal::WithThreadDo(thread, [&](PthreadInternal* target) {
    result = internal::ValidateSchedPolicy(target->sched_policy, priority);
    if (result == 0) target->sched_priority = priority;
  });
  return found ? result : guest::kESRCH;
}

ELIXIR_BIONIC_FN(void, __pthread_cleanup_push)(internal::CleanupFrame* frame, SysvFn<void(void*)> routine,
                                               void* arg) noexcept {
  PthreadInternal* self = internal::Self();
  frame->routine = routine;
  frame->arg = arg;
  if (self == nullptr) {
    frame->prev = nullptr;
    return;
  }
  frame->prev = self->cleanup_stack;
  self->cleanup_stack = frame;
}

ELIXIR_BIONIC_FN(void, __pthread_cleanup_pop)(internal::CleanupFrame* frame, int execute) noexcept {
  if (PthreadInternal* self = internal::Self()) self->cleanup_stack = frame->prev;
  if (execute != 0) frame->routine(frame->arg);
}

ELIXIR_BIONIC_FN(int, __cxa_thread_atexit_impl)(SysvFn<void(void*)> fn, void* arg, void* dso_handle) noexcept {
  PthreadInternal* self = internal::Self();
  if (self == nullptr) return -1;
  internal::ThreadLocalDtor* before = self->thread_local_dtors;
  elixir::bionic::threads::AddThreadExitCallback(fn, arg, dso_handle);
  return self->thread_local_dtors != before ? 0 : -1;
}

ELIXIR_BIONIC_FN(void*, __tls_get_addr)(const TlsIndex* index) noexcept {
  char* block = static_cast<char*>(elixir::platform::GetTlsBlock(index->module_id));
  if (block == nullptr) return nullptr;
  return block + index->offset;
}

namespace elixir::bionic::threads::internal {

void RegisterLifecycleExports(ExportTable& table) {
#define ELIXIR_THREADS_EXPORT(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_THREADS_EXPORT(pthread_create);
  ELIXIR_THREADS_EXPORT(pthread_exit);
  ELIXIR_THREADS_EXPORT(pthread_join);
  ELIXIR_THREADS_EXPORT(pthread_detach);
  ELIXIR_THREADS_EXPORT(pthread_self);
  ELIXIR_THREADS_EXPORT(pthread_equal);
  ELIXIR_THREADS_EXPORT(pthread_kill);
  ELIXIR_THREADS_EXPORT(pthread_getattr_np);
  ELIXIR_THREADS_EXPORT(pthread_attr_init);
  ELIXIR_THREADS_EXPORT(pthread_attr_destroy);
  ELIXIR_THREADS_EXPORT(pthread_attr_setinheritsched);
  ELIXIR_THREADS_EXPORT(pthread_attr_getinheritsched);
  ELIXIR_THREADS_EXPORT(pthread_attr_setdetachstate);
  ELIXIR_THREADS_EXPORT(pthread_attr_getdetachstate);
  ELIXIR_THREADS_EXPORT(pthread_attr_setschedpolicy);
  ELIXIR_THREADS_EXPORT(pthread_attr_getschedpolicy);
  ELIXIR_THREADS_EXPORT(pthread_attr_setschedparam);
  ELIXIR_THREADS_EXPORT(pthread_attr_getschedparam);
  ELIXIR_THREADS_EXPORT(pthread_attr_setstacksize);
  ELIXIR_THREADS_EXPORT(pthread_attr_getstacksize);
  ELIXIR_THREADS_EXPORT(pthread_attr_setstack);
  ELIXIR_THREADS_EXPORT(pthread_attr_getstack);
  ELIXIR_THREADS_EXPORT(pthread_attr_setguardsize);
  ELIXIR_THREADS_EXPORT(pthread_attr_getguardsize);
  ELIXIR_THREADS_EXPORT(pthread_attr_setscope);
  ELIXIR_THREADS_EXPORT(pthread_attr_getscope);
  ELIXIR_THREADS_EXPORT(pthread_setname_np);
  ELIXIR_THREADS_EXPORT(pthread_getname_np);
  ELIXIR_THREADS_EXPORT(pthread_gettid_np);
  ELIXIR_THREADS_EXPORT(gettid);
  ELIXIR_THREADS_EXPORT(pthread_getcpuclockid);
  ELIXIR_THREADS_EXPORT(pthread_setschedparam);
  ELIXIR_THREADS_EXPORT(pthread_getschedparam);
  ELIXIR_THREADS_EXPORT(pthread_setschedprio);
  ELIXIR_THREADS_EXPORT(__pthread_cleanup_push);
  ELIXIR_THREADS_EXPORT(__pthread_cleanup_pop);
  ELIXIR_THREADS_EXPORT(__cxa_thread_atexit_impl);
  ELIXIR_THREADS_EXPORT(__tls_get_addr);
#undef ELIXIR_THREADS_EXPORT
}

}
