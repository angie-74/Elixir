// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/files/files_internal.h"

#include <windows.h>

#include <algorithm>
#include <vector>

namespace elixir::bionic::files {
namespace {

using abi::g_off;
using abi::g_size;
using abi::g_ssize;

constexpr std::size_t kCopyChunk = 64 * 1024;

g_ssize CopyBetween(int in_fd, g_off* in_offset, int out_fd, g_off* out_offset, g_size count) {
  DescriptionPtr in = GetDescription(in_fd);
  if (in == nullptr) return -1;
  DescriptionPtr out = GetDescription(out_fd);
  if (out == nullptr) return -1;
  if ((in_offset != nullptr && *in_offset < 0) || (out_offset != nullptr && *out_offset < 0)) {
    return FailWith(guest::kEINVAL, static_cast<g_ssize>(-1));
  }
  std::vector<char> buffer(static_cast<std::size_t>(std::min<g_size>(count, kCopyChunk)));
  g_ssize total = 0;
  while (static_cast<g_size>(total) < count) {
    const std::size_t want = static_cast<std::size_t>(std::min<g_size>(count - static_cast<g_size>(total), kCopyChunk));
    const std::int64_t got = in_offset != nullptr ? DescPread(*in, buffer.data(), want, *in_offset)
                                                  : DescRead(*in, buffer.data(), want);
    if (got < 0) return total > 0 ? total : -1;
    if (got == 0) break;
    std::int64_t written = 0;
    while (written < got) {
      const std::int64_t put =
          out_offset != nullptr
              ? DescPwrite(*out, buffer.data() + written, static_cast<std::size_t>(got - written), *out_offset + written)
              : DescWrite(*out, buffer.data() + written, static_cast<std::size_t>(got - written));
      if (put <= 0) {
        if (in_offset != nullptr) *in_offset += written;
        if (out_offset != nullptr) *out_offset += written;
        total += written;
        return total > 0 ? total : -1;
      }
      written += put;
    }
    if (in_offset != nullptr) *in_offset += got;
    if (out_offset != nullptr) *out_offset += got;
    total += got;
    if (static_cast<std::size_t>(got) < want) break;
  }
  return total;
}

int NoXattr(const char* path, bool follow) {
  if (path == nullptr) return FailWith(guest::kEFAULT, -1);
  guest::Stat st;
  Resolved r;
  int err = 0;
  if (path[0] == '\0') return FailWith(guest::kENOENT, -1);
  if (!Resolve(path, &r, &err) || !StatResolved(r, follow, &st, &err)) return FailWith(err, -1);
  return FailWith(guest::kENOTSUP, -1);
}

int NoFdXattr(int fd) {
  if (GetDescription(fd) == nullptr) return -1;
  return FailWith(guest::kENOTSUP, -1);
}

}

ELIXIR_BIONIC_FN(abi::g_ssize, sendfile)(int out_fd, int in_fd, abi::g_off* offset, abi::g_size count) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return CopyBetween(in_fd, offset, out_fd, nullptr, count); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, copy_file_range)(int in_fd, abi::g_off* in_offset, int out_fd, abi::g_off* out_offset,
                                                abi::g_size count, std::uint32_t flags) noexcept {
  return Guarded<abi::g_ssize>(-1, [&]() -> abi::g_ssize {
    if (flags != 0) return FailWith(guest::kEINVAL, static_cast<abi::g_ssize>(-1));
    return CopyBetween(in_fd, in_offset, out_fd, out_offset, count);
  });
}
ELIXIR_BIONIC_FN(int, sync_file_range)(int fd, abi::g_off offset, abi::g_off length, std::uint32_t flags) noexcept {
  return Guarded(-1, [&] {
    DescriptionPtr d = GetDescription(fd);
    if (d == nullptr) return -1;
    if ((flags & ~7u) != 0 || offset < 0 || length < 0) return FailWith(guest::kEINVAL, -1);
    if (d->provider != Provider::kHandle || d->kind != vfs::FdKind::kFile) return FailWith(guest::kESPIPE, -1);
    if ((flags & 2u ) != 0) FlushFileBuffers(d->handle);
    return 0;
  });
}

ELIXIR_BIONIC_FN(abi::g_ssize, getxattr)(const char* path, const char*, void*, abi::g_size) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return NoXattr(path, true); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, lgetxattr)(const char* path, const char*, void*, abi::g_size) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return NoXattr(path, false); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, fgetxattr)(int fd, const char*, void*, abi::g_size) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return NoFdXattr(fd); });
}
ELIXIR_BIONIC_FN(int, setxattr)(const char* path, const char*, const void*, abi::g_size, int) noexcept {
  return Guarded(-1, [&] { return NoXattr(path, true); });
}
ELIXIR_BIONIC_FN(int, lsetxattr)(const char* path, const char*, const void*, abi::g_size, int) noexcept {
  return Guarded(-1, [&] { return NoXattr(path, false); });
}
ELIXIR_BIONIC_FN(int, fsetxattr)(int fd, const char*, const void*, abi::g_size, int) noexcept {
  return Guarded(-1, [&] { return NoFdXattr(fd); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, listxattr)(const char* path, char*, abi::g_size) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return NoXattr(path, true); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, llistxattr)(const char* path, char*, abi::g_size) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return NoXattr(path, false); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, flistxattr)(int fd, char*, abi::g_size) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return NoFdXattr(fd); });
}
ELIXIR_BIONIC_FN(int, removexattr)(const char* path, const char*) noexcept {
  return Guarded(-1, [&] { return NoXattr(path, true); });
}
ELIXIR_BIONIC_FN(int, lremovexattr)(const char* path, const char*) noexcept {
  return Guarded(-1, [&] { return NoXattr(path, false); });
}
ELIXIR_BIONIC_FN(int, fremovexattr)(int fd, const char*) noexcept {
  return Guarded(-1, [&] { return NoFdXattr(fd); });
}

void RegisterExtraFileExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("sendfile", &elixir_bionic_sendfile);
  add("sendfile64", &elixir_bionic_sendfile);
  add("copy_file_range", &elixir_bionic_copy_file_range);
  add("sync_file_range", &elixir_bionic_sync_file_range);
  add("getxattr", &elixir_bionic_getxattr);
  add("lgetxattr", &elixir_bionic_lgetxattr);
  add("fgetxattr", &elixir_bionic_fgetxattr);
  add("setxattr", &elixir_bionic_setxattr);
  add("lsetxattr", &elixir_bionic_lsetxattr);
  add("fsetxattr", &elixir_bionic_fsetxattr);
  add("listxattr", &elixir_bionic_listxattr);
  add("llistxattr", &elixir_bionic_llistxattr);
  add("flistxattr", &elixir_bionic_flistxattr);
  add("removexattr", &elixir_bionic_removexattr);
  add("lremovexattr", &elixir_bionic_lremovexattr);
  add("fremovexattr", &elixir_bionic_fremovexattr);
}

}
