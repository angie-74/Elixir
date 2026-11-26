// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elf/stack_guard_rewrite.h"

#include <windows.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

#include "platform/windows/guest_tls.h"

namespace elixir::elf {
namespace {

constexpr std::uintptr_t kAllocationGranularity = 64 * 1024;
constexpr std::intptr_t kReach = 0x7fff0000;

std::atomic<std::size_t> g_total_rewrites{0};
std::mutex g_slots_lock;
std::vector<std::uintptr_t> g_slots;

bool InReach(std::uintptr_t slot, std::uintptr_t begin, std::uintptr_t end) {
  const auto low = static_cast<std::intptr_t>(slot) - static_cast<std::intptr_t>(end);
  const auto high = static_cast<std::intptr_t>(slot) - static_cast<std::intptr_t>(begin);
  return low > -kReach && high < kReach;
}

std::uintptr_t AllocateSlotNear(std::uintptr_t begin, std::uintptr_t end) {
  const std::uintptr_t upward_start = (end + kAllocationGranularity - 1) & ~(kAllocationGranularity - 1);
  const std::uintptr_t downward_start = begin & ~(kAllocationGranularity - 1);
  for (std::uintptr_t step = 0; step < static_cast<std::uintptr_t>(kReach) / 2; step += kAllocationGranularity) {
    for (int direction = 0; direction < 2; ++direction) {
      std::uintptr_t candidate;
      if (direction == 0) {
        candidate = upward_start + step;
      } else {
        if (downward_start < step + kAllocationGranularity) continue;
        candidate = downward_start - step - kAllocationGranularity;
      }
      void* placed = VirtualAlloc(reinterpret_cast<void*>(candidate), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (placed == nullptr) continue;
      const auto slot = reinterpret_cast<std::uintptr_t>(placed);
      if (!InReach(slot, begin, end)) {
        VirtualFree(placed, 0, MEM_RELEASE);
        continue;
      }
      const std::uintptr_t canary = platform::GuestStackGuard();
      std::memcpy(placed, &canary, sizeof(canary));
      DWORD previous = 0;
      VirtualProtect(placed, 4096, PAGE_READONLY, &previous);
      return slot;
    }
  }
  return 0;
}

std::uintptr_t SlotFor(std::uintptr_t begin, std::uintptr_t end) {
  std::lock_guard<std::mutex> guard(g_slots_lock);
  for (std::uintptr_t slot : g_slots) {
    if (InReach(slot, begin, end)) return slot;
  }
  const std::uintptr_t slot = AllocateSlotNear(begin, end);
  if (slot != 0) g_slots.push_back(slot);
  return slot;
}

bool IsRewritableOpcode(std::uint8_t opcode) {
  switch (opcode) {
    case 0x8b:
    case 0x3b:
    case 0x39:
    case 0x33:
    case 0x2b:
    case 0x03:
      return true;
    default:
      return false;
  }
}

}

namespace {
std::atomic<int> g_enabled_override{-1};
}

bool StackGuardRewriteEnabled() {
  const int override_value = g_enabled_override.load(std::memory_order_relaxed);
  if (override_value >= 0) return override_value != 0;
  static const bool enabled = [] {
    char value[8] = {};
    const DWORD length = GetEnvironmentVariableA("ELIXIR_FS_REWRITE", value, sizeof(value));
    return length > 0 && length < sizeof(value) && value[0] == '1';
  }();
  return enabled;
}

void SetStackGuardRewriteEnabled(bool enabled) {
  g_enabled_override.store(enabled ? 1 : 0, std::memory_order_relaxed);
}

std::size_t TotalStackGuardRewrites() { return g_total_rewrites.load(std::memory_order_relaxed); }

RewriteStats RewriteStackGuardLoads(std::uint8_t* code, std::size_t length, std::uintptr_t image_begin,
                                    std::uintptr_t image_end) {
  RewriteStats stats;
  if (!StackGuardRewriteEnabled() || length < 9) return stats;
  std::uintptr_t slot = 0;
  for (std::size_t i = 0; i + 9 <= length; ++i) {
    const std::uint8_t* p = code + i;
    if (p[0] != 0x64 || (p[1] != 0x48 && p[1] != 0x4c) || !IsRewritableOpcode(p[2])) continue;
    if ((p[3] & 0xc7) != 0x04 || p[4] != 0x25) continue;
    if (p[5] != 0x28 || p[6] != 0x00 || p[7] != 0x00 || p[8] != 0x00) continue;
    if (slot == 0) {
      slot = SlotFor(image_begin, image_end);
      if (slot == 0) {
        ++stats.skipped_out_of_range;
        return stats;
      }
    }
    const auto instruction_end = reinterpret_cast<std::uintptr_t>(p) + 7;
    const std::intptr_t displacement = static_cast<std::intptr_t>(slot) - static_cast<std::intptr_t>(instruction_end);
    if (displacement > INT32_MAX || displacement < INT32_MIN) {
      ++stats.skipped_out_of_range;
      continue;
    }
    const std::uint8_t rex = p[1];
    const std::uint8_t opcode = p[2];
    const std::uint8_t modrm = static_cast<std::uint8_t>((p[3] & 0x38) | 0x05);
    const auto disp32 = static_cast<std::int32_t>(displacement);
    std::uint8_t replacement[9] = {rex, opcode, modrm, 0, 0, 0, 0, 0x66, 0x90};
    std::memcpy(replacement + 3, &disp32, 4);
    std::memcpy(code + i, replacement, sizeof(replacement));
    ++stats.rewritten;
    i += 8;
  }
  if (stats.rewritten != 0) {
    g_total_rewrites.fetch_add(stats.rewritten, std::memory_order_relaxed);
    FlushInstructionCache(GetCurrentProcess(), code, length);
  }
  return stats;
}

}
