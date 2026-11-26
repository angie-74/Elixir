// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "bionic/mm/mm_internal.h"

extern "C" ELIXIR_SYSV int elixir_bionic_fputs(const char* text, elixir::bionic::guest::File* file) noexcept;

namespace elixir::bionic::mm {
namespace {

void* volatile g_malloc_hook = nullptr;
void* volatile g_realloc_hook = nullptr;
void* volatile g_free_hook = nullptr;
void* volatile g_memalign_hook = nullptr;

constexpr std::size_t kScudoMaxSize = std::size_t{1} << 16;

struct GuestBlockHeader {
  std::uint32_t magic;
  std::uint32_t offset;
  std::uint64_t size;
};
constexpr std::uint32_t kGuestBlockMagic = 0xE11C5EEDu;
constexpr std::size_t kMaxHeaderScan = 64 * 1024;

std::int64_t GuestBlockSize(const PROCESS_HEAP_ENTRY& entry) noexcept {
  const auto* base = static_cast<const unsigned char*>(entry.lpData);
  const std::size_t size = entry.cbData;
  const std::size_t scan = size < kMaxHeaderScan ? size : kMaxHeaderScan;
  for (std::size_t offset = 0; offset + sizeof(GuestBlockHeader) <= scan; offset += 16) {
    GuestBlockHeader header;
    std::memcpy(&header, base + offset, sizeof(header));
    if (header.magic != kGuestBlockMagic || header.offset != offset) continue;
    if (header.size > size - offset - sizeof(GuestBlockHeader)) continue;
    return static_cast<std::int64_t>(header.size);
  }
  return -1;
}

}
}

namespace mm = elixir::bionic::mm;
namespace guest = elixir::bionic::guest;

ELIXIR_BIONIC_FN(int, malloc_info)(int options, guest::File* stream) noexcept {
  (void)options;
  try {
    auto* counts = static_cast<std::uint64_t*>(
        VirtualAlloc(nullptr, mm::kScudoMaxSize * sizeof(std::uint64_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (counts != nullptr) {
      HANDLE heap = GetProcessHeap();
      if (HeapLock(heap)) {
        PROCESS_HEAP_ENTRY entry{};
        while (HeapWalk(heap, &entry)) {
          if ((entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) == 0) continue;
          const std::int64_t size = mm::GuestBlockSize(entry);
          if (size >= 0 && static_cast<std::uint64_t>(size) < mm::kScudoMaxSize) ++counts[size];
        }
        HeapUnlock(heap);
      }
    }
    elixir_bionic_fputs("<malloc version=\"scudo-1\">\n", stream);
    if (counts != nullptr) {
      char line[96];
      for (std::size_t size = 0; size < mm::kScudoMaxSize; ++size) {
        if (counts[size] == 0) continue;
        std::snprintf(line, sizeof(line), "<alloc size=\"%zu\" count=\"%" PRIu64 "\"/>\n", size, counts[size]);
        elixir_bionic_fputs(line, stream);
      }
      VirtualFree(counts, 0, MEM_RELEASE);
    }
    elixir_bionic_fputs("</malloc>\n", stream);
    return 0;
  } catch (...) {
    elixir::bionic::SetGuestErrno(guest::kENOMEM);
    return -1;
  }
}

namespace elixir::bionic::mm {

void RegisterMallocExtraExports(ExportTable& table) {
  table.Add(kLibc, "__malloc_hook", const_cast<void**>(&g_malloc_hook));
  table.Add(kLibc, "__realloc_hook", const_cast<void**>(&g_realloc_hook));
  table.Add(kLibc, "__free_hook", const_cast<void**>(&g_free_hook));
  table.Add(kLibc, "__memalign_hook", const_cast<void**>(&g_memalign_hook));
  table.Add(kLibc, "malloc_info", reinterpret_cast<void*>(&elixir_bionic_malloc_info));
}

}
