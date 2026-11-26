// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_MEMORY_H_
#define ELIXIR_BIONIC_MEMORY_H_

#include <cstddef>

namespace elixir::bionic {

void* GuestMalloc(std::size_t size) noexcept;
void* GuestCalloc(std::size_t count, std::size_t size) noexcept;
void* GuestRealloc(void* block, std::size_t size) noexcept;
void* GuestAlignedAlloc(std::size_t alignment, std::size_t size) noexcept;
void GuestFree(void* block) noexcept;
std::size_t GuestUsableSize(const void* block) noexcept;
char* GuestStrdup(const char* text) noexcept;
char* GuestStrndup(const char* text, std::size_t limit) noexcept;

struct GuestHeapStats {
  std::size_t live_blocks;
  std::size_t live_bytes;
};
GuestHeapStats GuestHeapStatistics() noexcept;

}

#endif
