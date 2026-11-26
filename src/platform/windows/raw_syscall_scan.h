// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_PLATFORM_WINDOWS_RAW_SYSCALL_SCAN_H_
#define ELIXIR_PLATFORM_WINDOWS_RAW_SYSCALL_SCAN_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace elixir::platform {

enum class X86Flow : std::uint8_t {
  kNone,
  kConditional,
  kJump,
  kCall,
  kReturn,
  kIndirectJump,
  kIndirectCall,
  kSyscall,
  kHalt,
};

struct X86Instruction {
  std::uint8_t length = 0;
  X86Flow flow = X86Flow::kNone;
  std::int32_t displacement = 0;
};

X86Instruction DecodeX86(const std::uint8_t* code, std::size_t available) noexcept;

inline constexpr std::size_t kMaxRawSyscallStops = 64;

struct RawSyscallFunction {
  std::uintptr_t start = 0;
  std::uintptr_t end = 0;
  std::vector<std::uint32_t> offsets;
  std::vector<std::uint8_t> lengths;
  std::vector<X86Flow> flows;
  std::vector<std::uint64_t> next;
  std::vector<std::uint8_t> leaves;
  std::vector<std::uint32_t> stops;
  std::size_t syscalls = 0;

  std::ptrdiff_t IndexOf(std::uintptr_t address) const noexcept;
  std::uintptr_t AddressOf(std::size_t index) const noexcept { return start + offsets[index]; }
};

struct RawSyscallLandingPad {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;
  std::uint32_t pad = 0;
};

enum class RawSyscallAnalysis {
  kOk,
  kNoSyscall,
  kUndecodable,
  kTooManyStops,
};

RawSyscallAnalysis AnalyzeRawSyscallFunction(const std::uint8_t* code, std::uintptr_t start, std::size_t size,
                                             bool exception_table, const std::vector<RawSyscallLandingPad>* pads,
                                             RawSyscallFunction* out);

struct RawSyscallImage {
  struct Range {
    std::uintptr_t start = 0;
    std::uintptr_t end = 0;
  };
  std::vector<Range> loaded;
  std::vector<Range> executable;
  std::uintptr_t eh_frame_hdr = 0;
  std::size_t eh_frame_hdr_size = 0;
};

struct RawSyscallScan {
  std::vector<RawSyscallFunction> functions;
  bool function_table = false;
  std::size_t byte_pairs = 0;
  std::size_t outside_functions = 0;
  std::size_t undecodable = 0;
  std::size_t too_many_stops = 0;
  std::size_t syscalls = 0;
};

RawSyscallScan ScanRawSyscalls(const RawSyscallImage& image);

}

#endif
