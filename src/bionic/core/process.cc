// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <iterator>
#include <mutex>
#include <vector>

#include "bionic/core/core.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"
#include "platform/windows/guest_tls.h"

namespace elixir::bionic {

ELIXIR_BIONIC_FN(void, abort)() noexcept;

namespace {

using core::SignalBit;

std::uintptr_t g_stack_chk_guard = 0;

SRWLOCK g_abort_message_lock = SRWLOCK_INIT;
char* g_abort_message = nullptr;

void SetAbortMessage(const char* message) noexcept {
  if (message == nullptr) return;
  AcquireSRWLockExclusive(&g_abort_message_lock);
  if (g_abort_message == nullptr) {
    const std::size_t length = std::strlen(message);
    auto* copy = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, length + 1));
    if (copy != nullptr) {
      std::memcpy(copy, message, length + 1);
      g_abort_message = copy;
    }
  }
  ReleaseSRWLockExclusive(&g_abort_message_lock);
}

constexpr int kLogFatal = 7;

[[noreturn]] void FatalWithMessage(const char* message) noexcept {
  HostLog(kLogFatal, "libc", message);
  SetAbortMessage(message);
  elixir_bionic_abort();
  core::TerminateForSignal(guest::kSIGABRT);
}

struct ExitHandler {
  abi::SysvFn<void(void*)> function;
  void* argument;
  void* dso;
};

struct ForkHandlers {
  abi::SysvFn<void()> prepare;
  abi::SysvFn<void()> parent;
  abi::SysvFn<void()> child;
  void* dso;
};

struct ExitState {
  CRITICAL_SECTION lock;
  std::vector<ExitHandler> handlers;
  std::uint64_t generation = 0;
  std::vector<ExitHandler> quick_handlers;
  std::vector<ForkHandlers> fork_handlers;
  std::vector<core::ExitHook> hooks;
  CRITICAL_SECTION exit_lock;
};

ExitState& State() noexcept {
  static ExitState* state = [] {
    auto* created = new ExitState();
    InitializeCriticalSection(&created->lock);
    InitializeCriticalSection(&created->exit_lock);
    return created;
  }();
  return *state;
}

class Locked {
 public:
  explicit Locked(CRITICAL_SECTION* section) noexcept : section_(section) { EnterCriticalSection(section_); }
  ~Locked() { LeaveCriticalSection(section_); }
  Locked(const Locked&) = delete;
  Locked& operator=(const Locked&) = delete;

 private:
  CRITICAL_SECTION* section_;
};

void UnregisterForkHandlers(void* dso) noexcept {
  ExitState& state = State();
  Locked lock(&state.lock);
  auto& list = state.fork_handlers;
  for (std::size_t i = 0; i < list.size();) {
    if (list[i].dso == dso) {
      list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
}

void RunFinalizers(void* dso) noexcept {
  ExitState& state = State();
  EnterCriticalSection(&state.lock);
restart:
  for (std::size_t i = state.handlers.size(); i-- > 0;) {
    ExitHandler handler = state.handlers[i];
    if (handler.function == nullptr) continue;
    if (dso != nullptr && handler.dso != dso) continue;
    state.handlers[i].function = nullptr;
    const std::uint64_t generation = state.generation;
    LeaveCriticalSection(&state.lock);
    handler.function(handler.argument);
    EnterCriticalSection(&state.lock);
    if (state.generation != generation) goto restart;
  }
  std::size_t out = 0;
  for (std::size_t i = 0; i < state.handlers.size(); ++i) {
    if (state.handlers[i].function != nullptr) state.handlers[out++] = state.handlers[i];
  }
  state.handlers.resize(out);
  LeaveCriticalSection(&state.lock);
}

constexpr abi::g_ulong kAtUid = 11, kAtEuid = 12, kAtGid = 13, kAtEgid = 14, kAtPlatform = 15,
                       kAtHwcap = 16, kAtClktck = 17, kAtSecure = 23, kAtRandom = 25, kAtHwcap2 = 26,
                       kAtExecfn = 31, kAtPagesz = 6, kAtMinsigstksz = 51;

alignas(16) unsigned char g_at_random[16];
std::once_flag g_at_random_once;
const char kAtPlatformString[] = "x86_64";
const char kAtExecfnString[] = "/system/bin/app_process64";

}

ELIXIR_BIONIC_FN(int*, __errno)() noexcept { return GuestErrnoLocation(); }

ELIXIR_BIONIC_FN(void, __stack_chk_fail)() noexcept {
  HostLog(kLogFatal, "libc", "stack corruption detected (-fstack-protector)");
  SetAbortMessage("stack corruption detected (-fstack-protector)");
  RaiseFailFastException(nullptr, nullptr, 0);
  TerminateProcess(GetCurrentProcess(), 128 + guest::kSIGABRT);
  for (;;) Sleep(INFINITE);
}

ELIXIR_BIONIC_FN(void, android_set_abort_message)(const char* message) noexcept { SetAbortMessage(message); }

ELIXIR_BIONIC_FN(void, abort)() noexcept {
  core::SetSignalMask(core::CurrentSignalMask() & ~SignalBit(guest::kSIGABRT));
  core::RaiseOnCurrentThread(guest::kSIGABRT, guest::kSI_TKILL);
  core::ResetSignalToDefault(guest::kSIGABRT);
  core::SetSignalMask(core::CurrentSignalMask() & ~SignalBit(guest::kSIGABRT));
  core::RaiseOnCurrentThread(guest::kSIGABRT, guest::kSI_TKILL);
  core::TerminateForSignal(guest::kSIGABRT);
}

ELIXIR_BIONIC_FN(void, __assert)(const char* file, int line, const char* expression) noexcept {
  char message[1024];
  std::snprintf(message, sizeof(message), "%s:%d: %s", file != nullptr ? file : "?", line,
                expression != nullptr ? expression : "?");
  FatalWithMessage(message);
}

ELIXIR_BIONIC_FN(void, __assert2)(const char* file, int line, const char* function,
                                  const char* expression) noexcept {
  char message[1024];
  std::snprintf(message, sizeof(message), "%s:%d: %s: assertion \"%s\" failed", file != nullptr ? file : "?",
                line, function != nullptr ? function : "?", expression != nullptr ? expression : "?");
  FatalWithMessage(message);
}

ELIXIR_BIONIC_FN(int, __cxa_atexit)(abi::SysvFn<void(void*)> function, void* argument, void* dso) noexcept {
  if (function == nullptr) return 0;
  ExitState& state = State();
  Locked lock(&state.lock);
  try {
    state.handlers.push_back(ExitHandler{function, argument, dso});
  } catch (...) {
    return -1;
  }
  ++state.generation;
  return 0;
}

ELIXIR_BIONIC_FN(int, atexit)(abi::SysvFn<void()> function) noexcept {
  return elixir_bionic___cxa_atexit(reinterpret_cast<abi::SysvFn<void(void*)>>(function), nullptr, nullptr);
}

ELIXIR_BIONIC_FN(void, __cxa_finalize)(void* dso) noexcept {
  RunFinalizers(dso);
  if (dso != nullptr) UnregisterForkHandlers(dso);
}

ELIXIR_BIONIC_FN(int, at_quick_exit)(abi::SysvFn<void()> function) noexcept {
  if (function == nullptr) return 0;
  ExitState& state = State();
  Locked lock(&state.lock);
  try {
    state.quick_handlers.push_back(
        ExitHandler{reinterpret_cast<abi::SysvFn<void(void*)>>(function), nullptr, nullptr});
  } catch (...) {
    return -1;
  }
  return 0;
}

ELIXIR_BIONIC_FN(void, _exit)(int status) noexcept {
  TerminateProcess(GetCurrentProcess(), static_cast<UINT>(status) & 0xff);
  ExitProcess(static_cast<UINT>(status) & 0xff);
}

ELIXIR_BIONIC_FN(void, _Exit)(int status) noexcept { elixir_bionic__exit(status); }

ELIXIR_BIONIC_FN(void, quick_exit)(int status) noexcept {
  ExitState& state = State();
  EnterCriticalSection(&state.lock);
  while (!state.quick_handlers.empty()) {
    ExitHandler handler = state.quick_handlers.back();
    state.quick_handlers.pop_back();
    LeaveCriticalSection(&state.lock);
    handler.function(nullptr);
    EnterCriticalSection(&state.lock);
  }
  LeaveCriticalSection(&state.lock);
  elixir_bionic__exit(status);
}

ELIXIR_BIONIC_FN(void, exit)(int status) noexcept {
  ExitState& state = State();
  EnterCriticalSection(&state.exit_lock);
  RunFinalizers(nullptr);
  for (;;) {
    core::ExitHook hook = nullptr;
    {
      Locked lock(&state.lock);
      if (state.hooks.empty()) break;
      hook = state.hooks.back();
      state.hooks.pop_back();
    }
    hook();
  }
  std::fflush(nullptr);
  ExitProcess(static_cast<UINT>(status) & 0xff);
}

ELIXIR_BIONIC_FN(int, __register_atfork)(abi::SysvFn<void()> prepare, abi::SysvFn<void()> parent,
                                         abi::SysvFn<void()> child, void* dso) noexcept {
  ExitState& state = State();
  Locked lock(&state.lock);
  try {
    state.fork_handlers.push_back(ForkHandlers{prepare, parent, child, dso});
  } catch (...) {
    return guest::kENOMEM;
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, pthread_atfork)(abi::SysvFn<void()> prepare, abi::SysvFn<void()> parent,
                                      abi::SysvFn<void()> child) noexcept {
  return elixir_bionic___register_atfork(prepare, parent, child, nullptr);
}

ELIXIR_BIONIC_FN(abi::g_ulong, getauxval)(abi::g_ulong type) noexcept {
  switch (type) {
    case kAtPagesz: return 4096;
    case kAtClktck: return 100;
    case kAtHwcap:
    case kAtHwcap2: return 0;
    case kAtSecure: return 0;
    case kAtUid:
    case kAtEuid: return core::CurrentUid();
    case kAtGid:
    case kAtEgid: return core::CurrentGid();
    case kAtPlatform: return reinterpret_cast<abi::g_ulong>(kAtPlatformString);
    case kAtExecfn: return reinterpret_cast<abi::g_ulong>(kAtExecfnString);
    case kAtMinsigstksz: return 4096;
    case kAtRandom:
      std::call_once(g_at_random_once, [] {
        if (!core::FillRandom(g_at_random, sizeof(g_at_random))) {
          LARGE_INTEGER counter;
          QueryPerformanceCounter(&counter);
          std::uint64_t seed = static_cast<std::uint64_t>(counter.QuadPart) ^ GetCurrentProcessId();
          for (unsigned char& byte : g_at_random) {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            byte = static_cast<unsigned char>(seed >> 56);
          }
        }
      });
      return reinterpret_cast<abi::g_ulong>(g_at_random);
    default: SetGuestErrno(guest::kENOENT); return 0;
  }
}

namespace core {

std::string GuestAuxiliaryVector() {
  constexpr abi::g_ulong tags[] = {
      kAtPagesz, kAtUid, kAtEuid, kAtGid, kAtEgid, kAtPlatform, kAtHwcap, kAtClktck,
      kAtSecure, kAtRandom, kAtHwcap2, kAtExecfn, kAtMinsigstksz,
  };
  abi::g_ulong entries[std::size(tags) + 1][2]{};
  for (std::size_t i = 0; i < std::size(tags); ++i) {
    entries[i][0] = tags[i];
    entries[i][1] = elixir_bionic_getauxval(tags[i]);
  }
  return std::string(reinterpret_cast<const char*>(entries), sizeof(entries));
}

void AddExitHook(ExitHook hook) noexcept {
  if (hook == nullptr) return;
  ExitState& state = State();
  Locked lock(&state.lock);
  try {
    state.hooks.push_back(hook);
  } catch (...) {
  }
}

}

void RegisterCoreExports(ExportTable& table) {
  g_stack_chk_guard = platform::GuestStackGuard();
  if (g_stack_chk_guard == 0) {
    HostLog(5, "elixir-core", "RegisterCoreExports before InitializeRuntime: __stack_chk_guard is 0");
  }
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(__errno);
  ELIXIR_ADD(__stack_chk_fail);
  table.Add(kLibc, "__stack_chk_guard", &g_stack_chk_guard);
  ELIXIR_ADD(abort);
  ELIXIR_ADD(__assert);
  ELIXIR_ADD(__assert2);
  ELIXIR_ADD(android_set_abort_message);
  ELIXIR_ADD(exit);
  ELIXIR_ADD(_exit);
  ELIXIR_ADD(_Exit);
  ELIXIR_ADD(atexit);
  ELIXIR_ADD(__cxa_atexit);
  ELIXIR_ADD(__cxa_finalize);
  ELIXIR_ADD(at_quick_exit);
  ELIXIR_ADD(quick_exit);
  ELIXIR_ADD(__register_atfork);
  ELIXIR_ADD(pthread_atfork);
  ELIXIR_ADD(getauxval);
#undef ELIXIR_ADD
  core::RegisterSetjmpExports(table);
  core::RegisterPropertyExports(table);
}

}
