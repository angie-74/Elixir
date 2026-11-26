// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_MM_MM_H_
#define ELIXIR_BIONIC_MM_MM_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace elixir::bionic::mm {

struct Mapping {
  std::uintptr_t start = 0;
  std::uintptr_t end = 0;
  int prot = 0;
  bool shared = false;
  std::uint64_t offset = 0;
  std::uint64_t dev = 0;
  std::uint64_t ino = 0;
  std::string name;
};

std::vector<Mapping> SnapshotMappings();

bool IsGuestMemory(std::uintptr_t address) noexcept;

bool PrepareGuestBuffer(const void* address, std::size_t length, bool write) noexcept;

void SetLazyCommit(bool lazy) noexcept;
bool LazyCommit() noexcept;

struct Statistics {
  std::size_t arenas = 0;
  std::size_t regions = 0;
  std::uint64_t mapped_bytes = 0;
  std::uint64_t locked_bytes = 0;
  bool page_granular_views = false;
};
Statistics GetStatistics();

}

#endif
