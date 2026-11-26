// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_VFS_H_
#define ELIXIR_BIONIC_VFS_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace elixir::bionic::vfs {

struct Mount {
  std::string guest_prefix;
  std::string host_path;
  bool read_only = false;
  std::string overlay_host_path;
  bool (*overlay_filter)(std::string_view relative) = nullptr;
};

void SetMounts(const Mount* mounts, std::size_t count);
bool ResolveToHost(std::string_view guest_path, std::string* host_path, int* guest_errno,
                   bool* read_only = nullptr);
std::string HostToGuest(std::string_view host_path);
std::uint64_t OverlayLookupCount() noexcept;

enum class FdKind : std::uint8_t { kFile, kDirectory, kPipe, kSocket, kConsole, kNull, kEvent };
int InstallHandle(void* handle, FdKind kind, int open_flags);
void* HandleOf(int fd, FdKind* kind = nullptr);

}

#endif
