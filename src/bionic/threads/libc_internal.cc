// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Semantics follow bionic libc/bionic/sys_thread_properties.cpp,
// bionic_elf_tls.cpp (__notify_thread_exit_callbacks), libc_init_dynamic.cpp
// and libc_init_common.cpp (__libc_fini):
//
//   Copyright (C) 2020 The Android Open Source Project (sys_thread_properties.cpp)
//   Copyright (C) 2019 The Android Open Source Project (bionic_elf_tls.cpp)
//   Copyright (C) 2008 The Android Open Source Project (libc_init_dynamic.cpp,
//                                                       libc_init_common.cpp)
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
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <vector>

#include "abi/sysv_abi.h"
#include "bionic/core/core.h"
#include "bionic/runtime.h"
#include "bionic/threads.h"
#include "bionic/threads/thread_internal.h"
#include "platform/windows/guest_tls.h"

ELIXIR_BIONIC_FN(int, __cxa_atexit)(elixir::abi::SysvFn<void(void*)> function, void* argument, void* dso) noexcept;
ELIXIR_BIONIC_FN(void, exit)(int status) noexcept;
ELIXIR_BIONIC_FN(void, abort)() noexcept;
ELIXIR_BIONIC_FN(void, android_set_abort_message)(const char* message) noexcept;

namespace elixir::bionic::threads::internal {
namespace {

using ThreadExitCallback = abi::SysvFn<void()>;
using DynamicTlsListener = abi::SysvFn<void(void*, void*)>;
using DynamicTlsIterator = abi::SysvFn<void(void*, void*, guest::g_size, void*)>;

SRWLOCK g_exit_callbacks_lock = SRWLOCK_INIT;
std::vector<ThreadExitCallback>* g_exit_callbacks = nullptr;
std::atomic<bool> g_have_exit_callbacks{false};

std::atomic<DynamicTlsListener> g_on_creation{nullptr};
std::atomic<DynamicTlsListener> g_on_destruction{nullptr};

void ForwardCreation(void* begin, void* end) {
  if (DynamicTlsListener listener = g_on_creation.load(std::memory_order_acquire)) listener(begin, end);
}
void ForwardDestruction(void* begin, void* end) {
  if (DynamicTlsListener listener = g_on_destruction.load(std::memory_order_acquire)) listener(begin, end);
}

struct IterationContext {
  DynamicTlsIterator callback;
  void* argument;
};

void ForwardBlock(void* begin, void* end, std::size_t module_id, void* context) {
  auto* iteration = static_cast<IterationContext*>(context);
  iteration->callback(begin, end, module_id, iteration->argument);
}

struct KernelArguments {
  int argc;
  char** argv;
  char** envp;
};

KernelArguments ParseKernelArguments(void* raw_args) noexcept {
  auto* words = static_cast<std::uintptr_t*>(raw_args);
  KernelArguments args;
  args.argc = static_cast<int>(words[0]);
  args.argv = reinterpret_cast<char**>(words + 1);
  args.envp = args.argv + args.argc + 1;
  return args;
}

struct StructorsArray {
  abi::SysvFn<void()>* preinit_array;
  abi::SysvFn<void()>* init_array;
  abi::SysvFn<void()>* fini_array;
};

ELIXIR_SYSV void LibcFini(void* array) noexcept {
  using Dtor = abi::SysvFn<void()>;
  auto* fini_array = static_cast<Dtor*>(array);
  const auto minus1 = reinterpret_cast<Dtor>(static_cast<std::uintptr_t>(-1));
  if (array == nullptr || fini_array[0] != minus1) return;
  fini_array += 1;
  std::size_t count = 0;
  while (fini_array[count] != nullptr) ++count;
  while (count > 0) {
    Dtor dtor = fini_array[--count];
    if (dtor != minus1) dtor();
  }
}

[[noreturn]] void FatalMisuse(const char* message) noexcept {
  HostLog(kAndroidLogFatal, "libc", message);
  elixir_bionic_android_set_abort_message(message);
  elixir_bionic_abort();
  for (;;) Sleep(INFINITE);
}

}

void NotifyLibcThreadExitCallbacks() noexcept {
  if (!g_have_exit_callbacks.load(std::memory_order_acquire)) return;
  std::vector<ThreadExitCallback> callbacks;
  AcquireSRWLockShared(&g_exit_callbacks_lock);
  try {
    if (g_exit_callbacks != nullptr) callbacks = *g_exit_callbacks;
  } catch (...) {
  }
  ReleaseSRWLockShared(&g_exit_callbacks_lock);
  if (callbacks.empty()) return;
  if (platform::CurrentGuestThread() == nullptr && platform::GuestTlsAvailable()) platform::EnsureGuestThread();
  platform::ActivateGuestThreadPointer();
  core::SetSignalMask(~std::uint64_t{0});
  for (std::size_t i = callbacks.size(); i-- > 0;) callbacks[i]();
}

}

namespace internal = elixir::bionic::threads::internal;
namespace abi = elixir::abi;
namespace guest = elixir::bionic::guest;

ELIXIR_BIONIC_FN(void, __libc_get_static_tls_bounds)(void** static_tls_begin, void** static_tls_end) noexcept {
  void* begin = nullptr;
  void* end = nullptr;
  if (!elixir::platform::StaticTlsBounds(&begin, &end)) {
    if (elixir::platform::EnsureGuestThread() != nullptr) elixir::platform::StaticTlsBounds(&begin, &end);
  }
  if (static_tls_begin != nullptr) *static_tls_begin = begin;
  if (static_tls_end != nullptr) *static_tls_end = end;
}

ELIXIR_BIONIC_FN(void, __libc_register_thread_exit_callback)(internal::ThreadExitCallback callback) noexcept {
  if (callback == nullptr) return;
  AcquireSRWLockExclusive(&internal::g_exit_callbacks_lock);
  try {
    if (internal::g_exit_callbacks == nullptr) internal::g_exit_callbacks = new std::vector<internal::ThreadExitCallback>();
    internal::g_exit_callbacks->push_back(callback);
    internal::g_have_exit_callbacks.store(true, std::memory_order_release);
  } catch (...) {
    elixir::bionic::HostLog(internal::kAndroidLogWarn, "libc", "__libc_register_thread_exit_callback: out of memory");
  }
  ReleaseSRWLockExclusive(&internal::g_exit_callbacks_lock);
}

ELIXIR_BIONIC_FN(void, __libc_iterate_dynamic_tls)(std::int32_t tid, internal::DynamicTlsIterator callback,
                                                   void* argument) noexcept {
  if (callback == nullptr) return;
  internal::IterationContext context{callback, argument};
  const std::uint32_t windows_tid =
      tid == elixir::bionic::threads::CurrentTid() ? 0 : elixir::bionic::threads::WindowsThreadIdFromTid(tid);
  bool found = false;
  try {
    found = elixir::platform::ForEachDynamicTlsBlock(windows_tid, &internal::ForwardBlock, &context);
  } catch (...) {
  }
  if (!found) {
    elixir::bionic::HostLog(internal::kAndroidLogFatal, "libc",
                            "__get_bionic_tcb_for_thread failed to read thread register.");
  }
}

ELIXIR_BIONIC_FN(void, __libc_register_dynamic_tls_listeners)(internal::DynamicTlsListener on_creation,
                                                              internal::DynamicTlsListener on_destruction) noexcept {
  internal::g_on_creation.store(on_creation, std::memory_order_release);
  internal::g_on_destruction.store(on_destruction, std::memory_order_release);
  elixir::platform::SetDynamicTlsListeners(on_creation != nullptr ? &internal::ForwardCreation : nullptr,
                                           on_destruction != nullptr ? &internal::ForwardDestruction : nullptr);
}

ELIXIR_BIONIC_FN(void, __libc_init)(void* raw_args, abi::SysvFn<void()> onexit,
                                    abi::SysvFn<int(int, char**, char**)> slingshot,
                                    const internal::StructorsArray* structors) noexcept {
  (void)onexit;
  if (raw_args == nullptr || slingshot == nullptr || structors == nullptr) {
    internal::FatalMisuse(
        "__libc_init called without an executable's initial stack, main and structors (only crtbegin_dynamic of an "
        "executable may call it)");
  }
  const internal::KernelArguments args = internal::ParseKernelArguments(raw_args);
  if (structors->fini_array != nullptr) {
    elixir_bionic___cxa_atexit(&internal::LibcFini, reinterpret_cast<void*>(structors->fini_array), nullptr);
  }
  elixir_bionic_exit(slingshot(args.argc, args.argv, args.envp));
  for (;;) Sleep(INFINITE);
}

namespace elixir::bionic::threads::internal {

void RegisterLibcInternalExports(ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(__libc_get_static_tls_bounds);
  ELIXIR_ADD(__libc_register_thread_exit_callback);
  ELIXIR_ADD(__libc_iterate_dynamic_tls);
  ELIXIR_ADD(__libc_register_dynamic_tls_listeners);
  ELIXIR_ADD(__libc_init);
#undef ELIXIR_ADD
}

}
