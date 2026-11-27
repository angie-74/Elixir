// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_CORE_CORE_INTERNAL_H_
#define ELIXIR_BIONIC_CORE_CORE_INTERNAL_H_

#include <cstddef>
#include <cstdint>

#include "abi/sysv_abi.h"
#include "bionic/guest_types.h"
#include "bionic/runtime.h"

namespace elixir::bionic::core {

void RegisterPropertyExports(ExportTable& table);
void RegisterSetjmpExports(ExportTable& table);
void RegisterSignalExports(ExportTable& table);
void RegisterLongDoubleMathExports(ExportTable& table);
void RegisterFenvExports(ExportTable& table);
void RegisterComplexExports(ExportTable& table);
void RegisterTimeConversionExports(ExportTable& table);
void RegisterTimerExports(ExportTable& table);

inline constexpr int kSigBlock = 0;
inline constexpr int kSigUnblock = 1;
inline constexpr int kSigSetmask = 2;
inline constexpr int kSigRtMin = 42;
inline constexpr int kSigRtMax = 64;
inline constexpr int kMaxSignal = 64;
inline void* const kSigDfl = nullptr;
inline void* const kSigIgn = reinterpret_cast<void*>(1);
inline void* const kSigHold = reinterpret_cast<void*>(2);
inline void* const kSigErr = reinterpret_cast<void*>(-1);

inline constexpr guest::Sigset SignalBit(int sig) noexcept {
  return sig >= 1 && sig <= kMaxSignal ? (guest::Sigset{1} << (sig - 1)) : 0;
}

guest::Sigset CurrentSignalMask() noexcept;
void SetSignalMask(guest::Sigset mask) noexcept;
int RaiseOnCurrentThread(int sig, int si_code) noexcept;
void GenerateProcessSignal(int sig, int si_code, std::uint64_t si_value) noexcept;
int QueueGuestSignalToThread(std::int32_t tid, int sig, std::uint64_t value) noexcept;
void ResetSignalToDefault(int sig) noexcept;
[[noreturn]] void TerminateForSignal(int sig) noexcept;

std::uint32_t CurrentUid() noexcept;
std::uint32_t CurrentGid() noexcept;
std::int32_t CurrentPid() noexcept;
std::int32_t CurrentTid() noexcept;

int ReadClock(std::int32_t clock_id, guest::Timespec* out) noexcept;
std::int64_t MonotonicNanoseconds() noexcept;
std::int64_t CounterToNanoseconds(std::int64_t ticks, std::int64_t frequency) noexcept;
std::int64_t RealtimeNanoseconds() noexcept;
void PreciseSleep(std::int64_t nanoseconds) noexcept;
inline bool ValidTimespec(const guest::Timespec& ts) noexcept {
  return ts.tv_sec >= 0 && ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000;
}
inline std::int64_t TimespecToNanoseconds(const guest::Timespec& ts) noexcept {
  constexpr std::int64_t kMax = INT64_MAX / 1000000000 - 1;
  if (ts.tv_sec > kMax) return INT64_MAX;
  return ts.tv_sec * 1000000000 + ts.tv_nsec;
}
inline guest::Timespec NanosecondsToTimespec(std::int64_t ns) noexcept {
  guest::Timespec ts;
  ts.tv_sec = ns / 1000000000;
  ts.tv_nsec = ns % 1000000000;
  if (ts.tv_nsec < 0) {
    ts.tv_nsec += 1000000000;
    --ts.tv_sec;
  }
  return ts;
}

bool FillRandom(void* buffer, std::size_t length) noexcept;

void WriteHostStderr(const char* text, std::size_t length) noexcept;

}

#endif
