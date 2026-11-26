// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/memory.h"
#include "bionic/runtime.h"

#include <windows.h>

#include <cstdio>
#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstring>

namespace elixir::bionic {
namespace {

struct alignas(16) BlockHeader {
  std::uint32_t magic;
  std::uint32_t offset;
  std::uint64_t size;
};
static_assert(sizeof(BlockHeader) == 16, "header keeps 16-byte alignment");

constexpr std::uint32_t kMagic = 0xE11C5EEDu;
constexpr std::uint32_t kFreedMagic = 0xDEADB10Cu;

HANDLE Heap() noexcept {
  static const HANDLE heap = [] {
    HANDLE process_heap = GetProcessHeap();
    ULONG low_fragmentation = 2;
    HeapSetInformation(process_heap, HeapCompatibilityInformation, &low_fragmentation,
                       sizeof(low_fragmentation));
    return process_heap;
  }();
  return heap;
}

std::atomic<std::size_t> g_live_blocks{0};
std::atomic<std::size_t> g_live_bytes{0};

BlockHeader* HeaderOf(const void* block) noexcept {
  return reinterpret_cast<BlockHeader*>(const_cast<char*>(static_cast<const char*>(block)) - sizeof(BlockHeader));
}

void* Finish(void* base, std::size_t offset, std::size_t size) noexcept {
  auto* header = reinterpret_cast<BlockHeader*>(static_cast<char*>(base) + offset);
  header->magic = kMagic;
  header->offset = static_cast<std::uint32_t>(offset);
  header->size = size;
  g_live_blocks.fetch_add(1, std::memory_order_relaxed);
  g_live_bytes.fetch_add(size, std::memory_order_relaxed);
  return header + 1;
}

void* AllocateAligned(std::size_t alignment, std::size_t size, DWORD flags) noexcept {
  if (alignment < 16) alignment = 16;
  if ((alignment & (alignment - 1)) != 0) return nullptr;
  if (size > (static_cast<std::size_t>(1) << 47)) return nullptr;
  const std::size_t extra = alignment > 16 ? alignment : 0;
  void* base = HeapAlloc(Heap(), flags, size + sizeof(BlockHeader) + extra);
  if (base == nullptr) return nullptr;
  std::size_t offset = 0;
  if (extra != 0) {
    const auto user = reinterpret_cast<std::uintptr_t>(base) + sizeof(BlockHeader);
    const std::uintptr_t aligned = (user + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
    offset = aligned - sizeof(BlockHeader) - reinterpret_cast<std::uintptr_t>(base);
  }
  return Finish(base, offset, size);
}

}

void* GuestMalloc(std::size_t size) noexcept { return AllocateAligned(16, size, 0); }

void* GuestCalloc(std::size_t count, std::size_t size) noexcept {
  if (size != 0 && count > static_cast<std::size_t>(-1) / size) return nullptr;
  return AllocateAligned(16, count * size, HEAP_ZERO_MEMORY);
}

void* GuestAlignedAlloc(std::size_t alignment, std::size_t size) noexcept {
  return AllocateAligned(alignment, size, 0);
}

namespace {
[[noreturn]] void InvalidBlock(const char* function, const void* block, const void* caller) noexcept {
  char message[160];
  const std::uint32_t magic = HeaderOf(block)->magic;
  std::snprintf(message, sizeof(message), "%s(): invalid pointer %p (%s), called from %p", function, block,
                magic == kFreedMagic ? "double free" : "not allocated by the guest heap", caller);
  HostLog(7, "libc", message);
  RaiseFailFastException(nullptr, nullptr, 0);
  for (;;) {
  }
}
}

void GuestFree(void* block) noexcept {
  if (block == nullptr) return;
  BlockHeader* header = HeaderOf(block);
  if (header->magic != kMagic) {
    InvalidBlock("free", block, _ReturnAddress());
  }
  header->magic = kFreedMagic;
  g_live_blocks.fetch_sub(1, std::memory_order_relaxed);
  g_live_bytes.fetch_sub(static_cast<std::size_t>(header->size), std::memory_order_relaxed);
  HeapFree(Heap(), 0, reinterpret_cast<char*>(header) - header->offset);
}

void* GuestRealloc(void* block, std::size_t size) noexcept {
  if (block == nullptr) return GuestMalloc(size);
  BlockHeader* header = HeaderOf(block);
  if (header->magic != kMagic) InvalidBlock("realloc", block, _ReturnAddress());
  const std::size_t old_size = static_cast<std::size_t>(header->size);
  if (header->offset == 0) {
    void* base = HeapReAlloc(Heap(), 0, header, size + sizeof(BlockHeader));
    if (base == nullptr) return nullptr;
    auto* moved = static_cast<BlockHeader*>(base);
    moved->size = size;
    g_live_bytes.fetch_add(size, std::memory_order_relaxed);
    g_live_bytes.fetch_sub(old_size, std::memory_order_relaxed);
    return moved + 1;
  }
  std::size_t alignment = 32;
  while ((reinterpret_cast<std::uintptr_t>(block) & (alignment * 2 - 1)) == 0 && alignment < 4096) alignment *= 2;
  void* replacement = AllocateAligned(alignment, size, 0);
  if (replacement == nullptr) return nullptr;
  std::memcpy(replacement, block, old_size < size ? old_size : size);
  GuestFree(block);
  return replacement;
}

std::size_t GuestUsableSize(const void* block) noexcept {
  if (block == nullptr) return 0;
  const BlockHeader* header = HeaderOf(block);
  return header->magic == kMagic ? static_cast<std::size_t>(header->size) : 0;
}

char* GuestStrdup(const char* text) noexcept {
  if (text == nullptr) return nullptr;
  const std::size_t length = std::strlen(text);
  auto* copy = static_cast<char*>(GuestMalloc(length + 1));
  if (copy != nullptr) std::memcpy(copy, text, length + 1);
  return copy;
}

char* GuestStrndup(const char* text, std::size_t limit) noexcept {
  if (text == nullptr) return nullptr;
  std::size_t length = 0;
  while (length < limit && text[length] != '\0') ++length;
  auto* copy = static_cast<char*>(GuestMalloc(length + 1));
  if (copy == nullptr) return nullptr;
  std::memcpy(copy, text, length);
  copy[length] = '\0';
  return copy;
}

GuestHeapStats GuestHeapStatistics() noexcept {
  return {g_live_blocks.load(std::memory_order_relaxed), g_live_bytes.load(std::memory_order_relaxed)};
}

}
