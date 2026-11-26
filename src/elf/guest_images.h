// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_ELF_GUEST_IMAGES_H_
#define ELIXIR_ELF_GUEST_IMAGES_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace elixir::elf {

struct GuestSegment {
  std::uintptr_t start = 0;
  std::uintptr_t end = 0;
  std::uint64_t file_offset = 0;
  std::uint64_t file_bytes = 0;
  const char* path = nullptr;
  bool executable = false;
};

using GuestSegmentObserver = void (*)(const GuestSegment& segment, bool loaded);
void SetGuestSegmentObserver(GuestSegmentObserver observer) noexcept;

void RegisterGuestSegment(const GuestSegment& segment) noexcept;
void UnregisterGuestRange(std::uintptr_t start, std::uintptr_t end) noexcept;

void OnGuestImageLoaded(const char* path, std::uintptr_t load_bias, const void* program_headers,
                        std::size_t count) noexcept;

bool IsGuestCode(std::uintptr_t address) noexcept;
std::size_t GuestCodeRangeCount() noexcept;

using GuestNameHook = bool (*)(const char* host_path, std::string* guest_path);
using HostPathHook = bool (*)(const char* guest_path, std::string* host_path);
void SetGuestPathHooks(GuestNameHook to_guest, HostPathHook to_host) noexcept;
const char* GuestNameOf(const char* host_path) noexcept;
bool HostPathOf(const char* guest_path, std::string* host_path) noexcept;

}

#endif
