// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_THREADS_H_
#define ELIXIR_BIONIC_THREADS_H_

#include <cstdint>

#include "abi/sysv_abi.h"

namespace elixir::bionic::threads {

std::int32_t CurrentTid() noexcept;
std::uint32_t WindowsThreadIdFromTid(std::int32_t tid) noexcept;
std::int32_t TidFromWindowsThreadId(std::uint32_t windows_thread_id) noexcept;

void AddThreadExitCallback(abi::SysvFn<void(void*)> fn, void* arg, void* dso_handle) noexcept;

int FutexWait(volatile std::int32_t* word, std::int32_t expected, std::int64_t timeout_ns) noexcept;
void FutexWake(volatile std::int32_t* word, bool all) noexcept;
void NoteWake(const volatile std::int32_t* word) noexcept;

enum class WordWait { kWoken, kTimedOut };
WordWait WaitOnWord(volatile std::int32_t* word, std::int32_t expected, std::int64_t timeout_ns) noexcept;
bool WordWaitsKeepTheirTimeout() noexcept;

inline constexpr std::int64_t kDefaultWordWaitSpinNs = 20000;
void SetWordWaitSpin(std::int64_t nanoseconds) noexcept;
std::int64_t WordWaitSpin() noexcept;

inline constexpr int kWordWaitLengthBuckets = 6;

struct WordWaitCounts {
  std::uint64_t ended_spinning = 0;
  std::uint64_t spun_in_vain = 0;
  std::uint64_t not_spun = 0;
  std::uint64_t spun_nanoseconds = 0;
  std::uint64_t sleeps[kWordWaitLengthBuckets] = {};
};
WordWaitCounts TakeWordWaitCounts() noexcept;

}

#endif
