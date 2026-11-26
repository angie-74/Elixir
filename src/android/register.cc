// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <mcpelauncher/linker.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>

#include "android/android_internal.h"
#include "bionic/runtime.h"

namespace elixir::android {

void Log(int priority, const char* format, ...) noexcept {
  char message[1024];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  bionic::HostLog(priority, "elixir-android", message);
}

bool ProcessShuttingDown() noexcept {
  using ShutdownFn = BOOLEAN(NTAPI*)();
  static const ShutdownFn shutdown_in_progress = reinterpret_cast<ShutdownFn>(
      reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlDllShutdownInProgress")));
  return shutdown_in_progress != nullptr && shutdown_in_progress() != FALSE;
}

std::int64_t MonotonicNs() noexcept {
  static const std::int64_t frequency = [] {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    return static_cast<std::int64_t>(value.QuadPart);
  }();
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  constexpr std::int64_t kNanosPerSecond = 1000000000;
  const std::int64_t seconds = counter.QuadPart / frequency;
  const std::int64_t remainder = counter.QuadPart % frequency;
  return seconds * kNanosPerSecond + remainder * kNanosPerSecond / frequency;
}

namespace guest_libc {
namespace {

using PollFn = abi::SysvFn<int(GuestPollfd*, std::uint64_t, int)>;
using Pipe2Fn = abi::SysvFn<int(int*, int)>;
using EventfdFn = abi::SysvFn<int(unsigned, int)>;
using ReadFn = abi::SysvFn<abi::g_ssize(int, void*, abi::g_size)>;
using WriteFn = abi::SysvFn<abi::g_ssize(int, const void*, abi::g_size)>;
using CloseFn = abi::SysvFn<int(int)>;
using KeyCreateFn = abi::SysvFn<int(int*, KeyDestructor)>;
using SetspecificFn = abi::SysvFn<int(int, const void*)>;

struct Entries {
  PollFn poll = nullptr;
  Pipe2Fn pipe2 = nullptr;
  EventfdFn eventfd = nullptr;
  ReadFn read = nullptr;
  WriteFn write = nullptr;
  CloseFn close = nullptr;
  KeyCreateFn key_create = nullptr;
  SetspecificFn setspecific = nullptr;
};

std::atomic<const Entries*> g_entries{nullptr};

const Entries* Resolve() noexcept {
  const Entries* entries = g_entries.load(std::memory_order_acquire);
  if (entries != nullptr) return entries;
  void* libc = linker::dlopen("libc.so", RTLD_NOW | RTLD_NOLOAD);
  if (libc == nullptr) return nullptr;
  auto* resolved = new (std::nothrow) Entries();
  if (resolved == nullptr) return nullptr;
  resolved->poll = reinterpret_cast<PollFn>(linker::dlsym(libc, "poll"));
  resolved->pipe2 = reinterpret_cast<Pipe2Fn>(linker::dlsym(libc, "pipe2"));
  resolved->eventfd = reinterpret_cast<EventfdFn>(linker::dlsym(libc, "eventfd"));
  resolved->read = reinterpret_cast<ReadFn>(linker::dlsym(libc, "read"));
  resolved->write = reinterpret_cast<WriteFn>(linker::dlsym(libc, "write"));
  resolved->close = reinterpret_cast<CloseFn>(linker::dlsym(libc, "close"));
  resolved->key_create = reinterpret_cast<KeyCreateFn>(linker::dlsym(libc, "pthread_key_create"));
  resolved->setspecific = reinterpret_cast<SetspecificFn>(linker::dlsym(libc, "pthread_setspecific"));
  const Entries* expected = nullptr;
  if (!g_entries.compare_exchange_strong(expected, resolved, std::memory_order_acq_rel, std::memory_order_acquire)) {
    delete resolved;
    return expected;
  }
  return resolved;
}

template <typename Fn, typename... Args>
auto Call(Fn Entries::*member, int* guest_errno, Args... args) noexcept -> decltype((Entries{}.*member)(args...)) {
  const Entries* entries = Resolve();
  if (entries == nullptr || entries->*member == nullptr) {
    if (guest_errno != nullptr) *guest_errno = bionic::guest::kENOSYS;
    return -1;
  }
  const auto result = (entries->*member)(args...);
  if (result < 0 && guest_errno != nullptr) *guest_errno = bionic::GuestErrno();
  return result;
}

}

void Prime() noexcept { (void)Resolve(); }

bool HasPoll() noexcept {
  const Entries* entries = Resolve();
  return entries != nullptr && entries->poll != nullptr;
}

bool HasEventfd() noexcept {
  const Entries* entries = Resolve();
  return entries != nullptr && entries->eventfd != nullptr;
}

int Poll(GuestPollfd* fds, std::size_t count, int timeout_ms, int* guest_errno) noexcept {
  return Call(&Entries::poll, guest_errno, fds, static_cast<std::uint64_t>(count), timeout_ms);
}
int Pipe2(int fds[2], int flags, int* guest_errno) noexcept { return Call(&Entries::pipe2, guest_errno, fds, flags); }
int Eventfd(unsigned initial, int flags, int* guest_errno) noexcept {
  return Call(&Entries::eventfd, guest_errno, initial, flags);
}
std::int64_t Read(int fd, void* buffer, std::size_t size, int* guest_errno) noexcept {
  return Call(&Entries::read, guest_errno, fd, buffer, static_cast<abi::g_size>(size));
}
std::int64_t Write(int fd, const void* buffer, std::size_t size, int* guest_errno) noexcept {
  return Call(&Entries::write, guest_errno, fd, buffer, static_cast<abi::g_size>(size));
}
int Close(int fd) noexcept { return Call(&Entries::close, nullptr, fd); }

int PthreadKeyCreate(int* key, KeyDestructor destructor) noexcept {
  const Entries* entries = Resolve();
  if (entries == nullptr || entries->key_create == nullptr) return bionic::guest::kENOSYS;
  return entries->key_create(key, destructor);
}
int PthreadSetspecific(int key, const void* value) noexcept {
  const Entries* entries = Resolve();
  if (entries == nullptr || entries->setspecific == nullptr) return bionic::guest::kENOSYS;
  return entries->setspecific(key, value);
}

}

void RegisterAndroidExports(bionic::ExportTable& table) {
  RegisterNativeWindowExports(table);
  RegisterLooperExports(table);
  RegisterInputExports(table);
  RegisterAssetExports(table);
  RegisterConfigurationExports(table);
  RegisterChoreographerExports(table);
  RegisterSensorExports(table);
  RegisterServiceExports(table);
  RegisterEglExports(table);
  RegisterGlesExports(table);
  RegisterMediaExports(table);
}

std::size_t InstallAndroidLibraries() {
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  bionic::ExportTable table;
  RegisterAndroidExports(table);
  for (const std::string& conflict : table.conflicts()) {
    Log(kLogError, "android export registered twice with different addresses: %s", conflict.c_str());
  }
  static const char* const kSonames[] = {kLibandroid, kLibEgl, kLibGlesV2, kLibGlesV3, kLibMediandk};
  std::size_t installed = 0;
  for (const char* soname : kSonames) {
    const auto it = table.libraries().find(soname);
    if (it == table.libraries().end()) continue;
    if (linker::dlopen(soname, RTLD_NOW | RTLD_NOLOAD) != nullptr) continue;
    std::unordered_map<std::string, void*> map(it->second.begin(), it->second.end());
    if (linker::load_library(soname, map) != nullptr) {
      ++installed;
    } else {
      Log(kLogError, "cannot register %s with the loader", soname);
    }
  }
  if (table.libraries().size() != sizeof(kSonames) / sizeof(kSonames[0])) {
    Log(kLogError, "android export table has a SONAME that InstallAndroidLibraries does not install");
  }
  guest_libc::Prime();
  (void)ProcessShuttingDown();
  return installed;
}

}
