// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_PLATFORM_WINDOWS_RAW_SYSCALL_TRAP_H_
#define ELIXIR_PLATFORM_WINDOWS_RAW_SYSCALL_TRAP_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "platform/windows/raw_syscall_scan.h"

namespace elixir::platform {

struct RawSyscallRequest {
  std::uint64_t number = 0;
  std::uint64_t args[6] = {};
  std::uintptr_t site = 0;
  std::uintptr_t image_base = 0;
  const char* image = "";
};

using RawSyscallHandler = std::int64_t (*)(const RawSyscallRequest& request);
void SetRawSyscallHandler(RawSyscallHandler handler) noexcept;

bool RawSyscallTrapEnabled() noexcept;
void SetRawSyscallTrapEnabled(bool enabled) noexcept;

struct RawSyscallWatch {
  std::size_t functions = 0;
  std::size_t syscalls = 0;
  std::size_t watched_functions = 0;
  std::size_t watched_syscalls = 0;
};

RawSyscallWatch AddRawSyscallFunctions(std::vector<RawSyscallFunction> functions, std::uintptr_t image_base,
                                       const char* image_name);
void RemoveRawSyscallFunctions(std::uintptr_t start, std::uintptr_t end) noexcept;

void ArmRawSyscallsOnThisThread() noexcept;
void ApplyRawSyscallBreakpoints(void* context, bool always) noexcept;

struct RawSyscallStats {
  std::uint64_t handled = 0;
  std::uint64_t stops = 0;
  std::uint64_t traced = 0;
  std::uint64_t unwatched = 0;
  std::size_t watched_functions = 0;
  std::size_t known_functions = 0;
};
RawSyscallStats GetRawSyscallStats() noexcept;

}

#endif
