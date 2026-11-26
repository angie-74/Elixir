// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "platform/windows/guest_unwind_barrier.h"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "platform/windows/crash_reporter.h"

namespace elixir::platform {
namespace {

struct alignas(4) BarrierUnwindInfo {
  std::uint8_t version_and_flags;
  std::uint8_t size_of_prolog;
  std::uint8_t count_of_codes;
  std::uint8_t frame_register_and_offset;
  std::uint32_t handler_rva;
  std::uint32_t handler_data;
};
static_assert(sizeof(BarrierUnwindInfo) == 12, "UNWIND_INFO with zero codes plus handler RVA and data");

constexpr std::uint8_t kUnwindVersion = 1;
constexpr std::uint8_t kUnwFlagEHandler = 1;
constexpr std::uint8_t kUnwFlagUHandler = 2;
constexpr std::size_t kStubOffset = 16;
constexpr std::size_t kPageSize = 4096;
constexpr std::uintptr_t kAllocationGranularity = 64 * 1024;
constexpr std::size_t kMaxBarriers = 512;

struct Barrier {
  std::atomic<std::uintptr_t> start{0};
  std::atomic<std::uintptr_t> end{0};
  RUNTIME_FUNCTION function{};
  void* page = nullptr;
};

Barrier g_barriers[kMaxBarriers];
std::atomic<std::size_t> g_high_water{0};
std::atomic<std::size_t> g_count{0};
std::mutex g_writers;

EXCEPTION_DISPOSITION GuestFrameBarrier(EXCEPTION_RECORD* record, void* establisher_frame, CONTEXT* context,
                                        DISPATCHER_CONTEXT* dispatcher) {
  (void)establisher_frame;
  (void)dispatcher;
  const bool unwinding = (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) != 0;
  const char* reason = unwinding ? "a host unwind (C++ exception or longjmp) tried to cross a guest frame"
                       : record->ExceptionCode == 0xE06D7363
                           ? "a host C++ exception reached a guest frame without being caught"
                           : "unhandled exception below a guest frame (no host handler, no guest signal handler)";
  EXCEPTION_POINTERS pointers{record, context};
  ReportFatalException(&pointers, reason);
  TerminateProcess(GetCurrentProcess(), record->ExceptionCode != 0 ? record->ExceptionCode : 3);
  return ExceptionContinueSearch;
}

PRUNTIME_FUNCTION CALLBACK LookupBarrier(DWORD64 control_pc, PVOID context) {
  auto* barrier = static_cast<Barrier*>(context);
  const std::uintptr_t start = barrier->start.load(std::memory_order_acquire);
  if (start == 0 || control_pc < start || control_pc >= barrier->end.load(std::memory_order_acquire)) return nullptr;
  return &barrier->function;
}

void* AllocateNear(std::uintptr_t start, std::uintptr_t end) noexcept {
  const std::uintptr_t limit = start + 0xFFFF0000ull;
  std::uintptr_t candidate = (end + kAllocationGranularity - 1) & ~(kAllocationGranularity - 1);
  while (candidate + kPageSize <= limit) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<void*>(candidate), &info, sizeof(info)) == 0) break;
    const auto region_end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (info.State == MEM_FREE) {
      void* page = VirtualAlloc(reinterpret_cast<void*>(candidate), kPageSize, MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE);
      if (page != nullptr) return page;
    }
    candidate = (region_end + kAllocationGranularity - 1) & ~(kAllocationGranularity - 1);
  }
  return nullptr;
}

}

bool InstallGuestUnwindBarrier(std::uintptr_t start, std::uintptr_t end) noexcept {
  if (end <= start || end - start >= 0xFFFF0000ull) return false;
  std::lock_guard<std::mutex> lock(g_writers);
  Barrier* slot = nullptr;
  std::size_t index = 0;
  for (; index < kMaxBarriers; ++index) {
    if (g_barriers[index].start.load(std::memory_order_relaxed) == 0) {
      slot = &g_barriers[index];
      break;
    }
  }
  if (slot == nullptr) return false;
  void* page = AllocateNear(start, end);
  if (page == nullptr) return false;
  const auto page_address = reinterpret_cast<std::uintptr_t>(page);

  BarrierUnwindInfo info{};
  info.version_and_flags = static_cast<std::uint8_t>(kUnwindVersion | ((kUnwFlagEHandler | kUnwFlagUHandler) << 3));
  info.handler_rva = static_cast<std::uint32_t>(page_address + kStubOffset - start);
  std::memcpy(page, &info, sizeof(info));
  auto* stub = static_cast<std::uint8_t*>(page) + kStubOffset;
  const std::uint8_t jump[6] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
  std::memcpy(stub, jump, sizeof(jump));
  const auto target = reinterpret_cast<std::uintptr_t>(&GuestFrameBarrier);
  std::memcpy(stub + sizeof(jump), &target, sizeof(target));
  DWORD old_protect = 0;
  if (!VirtualProtect(page, kPageSize, PAGE_EXECUTE_READ, &old_protect)) {
    VirtualFree(page, 0, MEM_RELEASE);
    return false;
  }
  FlushInstructionCache(GetCurrentProcess(), page, kPageSize);

  slot->function.BeginAddress = 0;
  slot->function.EndAddress = static_cast<DWORD>(end - start);
  slot->function.UnwindData = static_cast<DWORD>(page_address - start);
  slot->page = page;
  slot->end.store(end, std::memory_order_release);
  slot->start.store(start, std::memory_order_release);
  if (!RtlInstallFunctionTableCallback(static_cast<DWORD64>(start) | 3, start, static_cast<DWORD>(end - start),
                                       &LookupBarrier, slot, nullptr)) {
    slot->start.store(0, std::memory_order_release);
    slot->end.store(0, std::memory_order_release);
    VirtualFree(page, 0, MEM_RELEASE);
    slot->page = nullptr;
    return false;
  }
  if (index + 1 > g_high_water.load(std::memory_order_relaxed)) g_high_water.store(index + 1, std::memory_order_release);
  g_count.fetch_add(1, std::memory_order_relaxed);
  return true;
}

void RemoveGuestUnwindBarriers(std::uintptr_t start, std::uintptr_t end) noexcept {
  std::lock_guard<std::mutex> lock(g_writers);
  const std::size_t used = g_high_water.load(std::memory_order_relaxed);
  for (std::size_t i = 0; i < used; ++i) {
    Barrier& barrier = g_barriers[i];
    const std::uintptr_t s = barrier.start.load(std::memory_order_relaxed);
    if (s == 0) continue;
    const std::uintptr_t e = barrier.end.load(std::memory_order_relaxed);
    if (!(s < end && start < e)) continue;
    RtlDeleteFunctionTable(reinterpret_cast<PRUNTIME_FUNCTION>(static_cast<DWORD64>(s) | 3));
    barrier.start.store(0, std::memory_order_release);
    barrier.end.store(0, std::memory_order_release);
    if (barrier.page != nullptr) VirtualFree(barrier.page, 0, MEM_RELEASE);
    barrier.page = nullptr;
    g_count.fetch_sub(1, std::memory_order_relaxed);
  }
}

bool IsBehindGuestUnwindBarrier(std::uintptr_t address) noexcept {
  const std::size_t used = g_high_water.load(std::memory_order_acquire);
  for (std::size_t i = 0; i < used; ++i) {
    const std::uintptr_t s = g_barriers[i].start.load(std::memory_order_acquire);
    if (s == 0 || address < s) continue;
    if (address < g_barriers[i].end.load(std::memory_order_acquire)) return true;
  }
  return false;
}

std::size_t GuestUnwindBarrierCount() noexcept { return g_count.load(std::memory_order_relaxed); }

}
