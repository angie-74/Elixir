// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_FILES_FILES_H_
#define ELIXIR_BIONIC_FILES_FILES_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abi/sysv_abi.h"
#include "bionic/vfs.h"

namespace elixir::bionic::files {

struct FdOps {
  abi::g_ssize (*read)(void* handle, std::uint8_t kind, void* buffer, std::size_t size, bool nonblocking,
                       int* guest_errno) noexcept = nullptr;
  abi::g_ssize (*write)(void* handle, std::uint8_t kind, const void* buffer, std::size_t size, bool nonblocking,
                        int* guest_errno) noexcept = nullptr;
  int (*close)(void* handle, std::uint8_t kind, int* guest_errno) noexcept = nullptr;
  int (*set_nonblocking)(void* handle, std::uint8_t kind, bool nonblocking, int* guest_errno) noexcept = nullptr;
  int (*bytes_available)(void* handle, std::uint8_t kind, int* available, int* guest_errno) noexcept = nullptr;
  std::uint32_t (*poll)(void* handle, std::uint8_t kind, std::uint32_t events) noexcept = nullptr;
};

bool SetKindOps(vfs::FdKind kind, const FdOps& ops) noexcept;

int InstallHookedFd(void* handle, const FdOps* ops, int open_flags, std::string_view name) noexcept;

void* HookedFdHandle(int fd, const FdOps* ops) noexcept;

bool HookedFdReadiness(int fd, std::uint32_t events, std::uint32_t* ready) noexcept;

std::vector<vfs::Mount> DefaultMounts(std::string_view package, std::string_view host_root = {});

bool EnsureMountDirectories(const std::vector<vfs::Mount>& mounts);

struct ContentCacheIoCounters {
  std::uint64_t open = 0;
  std::uint64_t read = 0;
  std::uint64_t write = 0;
  std::uint64_t rename = 0;
  std::uint64_t map = 0;
  std::uint64_t open_hits = 0;
  std::uint64_t open_misses = 0;
};
ContentCacheIoCounters GetContentCacheIoCounters() noexcept;

void RecordContentCacheMapFailure(int fd, const void* handle, int guest_errno) noexcept;

struct MappedSegment {
  std::uintptr_t start = 0;
  std::uintptr_t end = 0;
  std::uint64_t file_offset = 0;
  std::string guest_path;
};
void RegisterMappedSegment(const MappedSegment& segment);
void UnregisterMappedRange(std::uintptr_t start, std::uintptr_t end);

void SetProcTrace(bool enabled) noexcept;

}

#endif
