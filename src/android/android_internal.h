// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_ANDROID_ANDROID_INTERNAL_H_
#define ELIXIR_ANDROID_ANDROID_INTERNAL_H_

#include "android/android.h"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <new>

#include "abi/sysv_abi.h"
#include "bionic/runtime.h"

#define ELIXIR_ANDROID_FN(return_type, name) extern "C" ELIXIR_SYSV return_type elixir_android_##name

namespace elixir::android {

void RegisterNativeWindowExports(bionic::ExportTable& table);
void RegisterLooperExports(bionic::ExportTable& table);
void RegisterInputExports(bionic::ExportTable& table);
void RegisterAssetExports(bionic::ExportTable& table);
void RegisterConfigurationExports(bionic::ExportTable& table);
void RegisterChoreographerExports(bionic::ExportTable& table);
void RegisterSensorExports(bionic::ExportTable& table);
void RegisterServiceExports(bionic::ExportTable& table);
void RegisterEglExports(bionic::ExportTable& table);
void RegisterGlesExports(bionic::ExportTable& table);
void RegisterMediaExports(bionic::ExportTable& table);

void* FindEglEntryPoint(const char* name) noexcept;
void* FindGlesEntryPoint(const char* name) noexcept;

void Log(int priority, const char* format, ...) noexcept;
inline constexpr int kLogDebug = 3;
inline constexpr int kLogInfo = 4;
inline constexpr int kLogWarn = 5;
inline constexpr int kLogError = 6;

bool ProcessShuttingDown() noexcept;

std::int64_t MonotonicNs() noexcept;

template <typename R, typename F>
R Guarded(R failure, F&& body) noexcept {
  try {
    return body();
  } catch (...) {
    return failure;
  }
}
template <typename F>
void GuardedVoid(F&& body) noexcept {
  try {
    body();
  } catch (...) {
  }
}

namespace guest_libc {
void Prime() noexcept;
bool HasPoll() noexcept;
bool HasEventfd() noexcept;
int Poll(GuestPollfd* fds, std::size_t count, int timeout_ms, int* guest_errno) noexcept;
int Pipe2(int fds[2], int flags, int* guest_errno) noexcept;
int Eventfd(unsigned initial, int flags, int* guest_errno) noexcept;
std::int64_t Read(int fd, void* buffer, std::size_t size, int* guest_errno) noexcept;
std::int64_t Write(int fd, const void* buffer, std::size_t size, int* guest_errno) noexcept;
int Close(int fd) noexcept;
using KeyDestructor = abi::SysvFn<void(void*)>;
int PthreadKeyCreate(int* key, KeyDestructor destructor) noexcept;
int PthreadSetspecific(int key, const void* value) noexcept;
}

namespace looper {

inline constexpr int kPollWake = -1;
inline constexpr int kPollCallback = -2;
inline constexpr int kPollTimeout = -3;
inline constexpr int kPollError = -4;
inline constexpr int kEventInput = 1 << 0;
inline constexpr int kEventOutput = 1 << 1;
inline constexpr int kEventError = 1 << 2;
inline constexpr int kEventHangup = 1 << 3;
inline constexpr int kEventInvalid = 1 << 4;

using GuestLooperCallback = abi::SysvFn<int(int, int, void*)>;

struct HostSource {
  bool (*ready)(void* context) noexcept = nullptr;
  void (*dispatch)(void* context) noexcept = nullptr;
  void* context = nullptr;
  GuestLooperCallback guest_callback = nullptr;
  int ident = 0;
  void* data = nullptr;
  int fake_fd = -1;
};
using HostSourceId = std::uint64_t;
HostSourceId AddHostSource(ALooper* looper, const HostSource& source) noexcept;
void RemoveHostSource(ALooper* looper, HostSourceId id) noexcept;
void SignalLooper(ALooper* looper) noexcept;
void AcquireLooper(ALooper* looper) noexcept;
void ReleaseLooper(ALooper* looper) noexcept;
ALooper* CurrentThreadLooper() noexcept;
bool AllowsNonCallbacks(ALooper* looper) noexcept;

}

void ReleaseThreadChoreographer() noexcept;

bool PrimaryWindowGeometry(HWND* hwnd, int* width, int* height) noexcept;

}

#endif
