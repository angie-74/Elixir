// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"

#include <cstring>

#include "bionic/files/files.h"

extern "C" ELIXIR_SYSV int elixir_bionic_fcntl(int fd, int command, std::uint64_t arg);
extern "C" ELIXIR_SYSV int elixir_bionic_close(int fd);
extern "C" ELIXIR_SYSV elixir::abi::g_ssize elixir_bionic_write(int fd, const void* buffer, elixir::abi::g_size count);

namespace elixir::bionic::net::internal {
namespace {

struct FallbackEntry {
  void* handle = nullptr;
  vfs::FdKind kind = vfs::FdKind::kFile;
  std::atomic<int> flags{0};
  bool cloexec = false;
};

struct FallbackTable {
  std::mutex lock;
  std::vector<std::shared_ptr<FallbackEntry>> entries;
};

FallbackTable& Fallback() {
  static FallbackTable* table = new FallbackTable();
  return *table;
}

constexpr int kFallbackMaxFds = 32768;
constexpr int kFallbackFirstFd = 3;

std::shared_ptr<FallbackEntry> FallbackFind(int fd) {
  if (fd < 0 || fd >= kFallbackMaxFds) return nullptr;
  FallbackTable& table = Fallback();
  std::lock_guard<std::mutex> guard(table.lock);
  if (static_cast<std::size_t>(fd) >= table.entries.size()) return nullptr;
  return table.entries[static_cast<std::size_t>(fd)];
}

}
}

using elixir::bionic::SetGuestErrno;
namespace guest = elixir::bionic::guest;
namespace vfs = elixir::bionic::vfs;
namespace internal = elixir::bionic::net::internal;

extern "C" int elixir_net_fallback_install_handle(void* handle, vfs::FdKind kind, int open_flags) {
  try {
    auto entry = std::make_shared<internal::FallbackEntry>();
    entry->handle = handle;
    entry->kind = kind;
    entry->flags.store(open_flags & ~guest::kO_CLOEXEC);
    entry->cloexec = (open_flags & guest::kO_CLOEXEC) != 0;
    internal::FallbackTable& table = internal::Fallback();
    std::lock_guard<std::mutex> guard(table.lock);
    for (int fd = internal::kFallbackFirstFd; fd < internal::kFallbackMaxFds; ++fd) {
      const auto index = static_cast<std::size_t>(fd);
      if (index >= table.entries.size()) table.entries.resize(index + 1);
      if (table.entries[index] == nullptr) {
        table.entries[index] = std::move(entry);
        return fd;
      }
    }
    SetGuestErrno(guest::kEMFILE);
    return -1;
  } catch (...) {
    SetGuestErrno(guest::kENOMEM);
    return -1;
  }
}

extern "C" void* elixir_net_fallback_handle_of(int fd, vfs::FdKind* kind) {
  try {
    auto entry = internal::FallbackFind(fd);
    if (entry == nullptr) {
      SetGuestErrno(guest::kEBADF);
      return nullptr;
    }
    if (kind != nullptr) *kind = entry->kind;
    return entry->handle;
  } catch (...) {
    SetGuestErrno(guest::kENOMEM);
    return nullptr;
  }
}

extern "C" ELIXIR_SYSV int elixir_net_fallback_fcntl(int fd, int command, std::uint64_t arg) {
  try {
    auto entry = internal::FallbackFind(fd);
    if (entry == nullptr) {
      SetGuestErrno(guest::kEBADF);
      return -1;
    }
    switch (command) {
      case guest::kF_GETFL:
        return entry->flags.load();
      case guest::kF_SETFL: {
        const int changeable = guest::kO_APPEND | guest::kO_NONBLOCK | guest::kO_ASYNC;
        const int previous = entry->flags.load();
        const int next = (previous & ~changeable) | (static_cast<int>(arg) & changeable);
        entry->flags.store(next);
        if (((previous ^ next) & guest::kO_NONBLOCK) != 0 &&
            (entry->kind == vfs::FdKind::kSocket || entry->kind == vfs::FdKind::kEvent)) {
          int error = 0;
          elixir::bionic::net::FdSetNonBlocking(entry->handle, static_cast<std::uint8_t>(entry->kind),
                                                 (next & guest::kO_NONBLOCK) != 0, &error);
        }
        return 0;
      }
      case guest::kF_GETFD:
        return entry->cloexec ? guest::kFD_CLOEXEC : 0;
      case guest::kF_SETFD:
        entry->cloexec = (arg & guest::kFD_CLOEXEC) != 0;
        return 0;
      default:
        SetGuestErrno(guest::kEINVAL);
        return -1;
    }
  } catch (...) {
    SetGuestErrno(guest::kENOMEM);
    return -1;
  }
}

extern "C" ELIXIR_SYSV int elixir_net_fallback_close(int fd) {
  try {
    std::shared_ptr<internal::FallbackEntry> entry;
    {
      if (fd < 0 || fd >= internal::kFallbackMaxFds) {
        SetGuestErrno(guest::kEBADF);
        return -1;
      }
      internal::FallbackTable& table = internal::Fallback();
      std::lock_guard<std::mutex> guard(table.lock);
      const auto index = static_cast<std::size_t>(fd);
      if (index >= table.entries.size() || table.entries[index] == nullptr) {
        SetGuestErrno(guest::kEBADF);
        return -1;
      }
      entry = std::move(table.entries[index]);
    }
    int error = 0;
    if (entry->kind == vfs::FdKind::kSocket || entry->kind == vfs::FdKind::kEvent) {
      if (elixir::bionic::net::FdClose(entry->handle, static_cast<std::uint8_t>(entry->kind), &error) != 0) {
        SetGuestErrno(error);
        return -1;
      }
    } else if (entry->handle != nullptr) {
      CloseHandle(entry->handle);
    }
    return 0;
  } catch (...) {
    SetGuestErrno(guest::kENOMEM);
    return -1;
  }
}

extern "C" bool elixir_net_fallback_set_kind_ops(vfs::FdKind, const elixir::bionic::files::FdOps&) noexcept {
  return false;
}
extern "C" bool elixir_net_fallback_hooked_readiness(int, std::uint32_t, std::uint32_t*) noexcept { return false; }

extern "C" ELIXIR_SYSV elixir::abi::g_ssize elixir_net_fallback_write(int fd, const void* buffer,
                                                                       elixir::abi::g_size count) {
  if (fd != 1 && fd != 2) {
    SetGuestErrno(guest::kEBADF);
    return -1;
  }
  HANDLE out = GetStdHandle(fd == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
  DWORD written = 0;
  if (out == nullptr || out == INVALID_HANDLE_VALUE ||
      !WriteFile(out, buffer, static_cast<DWORD>(count > 0x7fffffff ? 0x7fffffff : count), &written, nullptr)) {
    SetGuestErrno(guest::kEIO);
    return -1;
  }
  return static_cast<elixir::abi::g_ssize>(written);
}

#pragma comment(linker, "/alternatename:?InstallHandle@vfs@bionic@elixir@@YAHPEAXW4FdKind@123@H@Z=elixir_net_fallback_install_handle")
#pragma comment(linker, "/alternatename:?HandleOf@vfs@bionic@elixir@@YAPEAXHPEAW4FdKind@123@@Z=elixir_net_fallback_handle_of")
#pragma comment(linker, "/alternatename:elixir_bionic_fcntl=elixir_net_fallback_fcntl")
#pragma comment(linker, "/alternatename:elixir_bionic_close=elixir_net_fallback_close")
#pragma comment(linker, "/alternatename:elixir_bionic_write=elixir_net_fallback_write")
#pragma comment(linker, "/alternatename:?SetKindOps@files@bionic@elixir@@YA_NW4FdKind@vfs@23@AEBUFdOps@123@@Z=elixir_net_fallback_set_kind_ops")
#pragma comment(linker, "/alternatename:?HookedFdReadiness@files@bionic@elixir@@YA_NHIPEAI@Z=elixir_net_fallback_hooked_readiness")

namespace elixir::bionic::net::internal {

int GuestStatusFlags(int fd) noexcept { return elixir_bionic_fcntl(fd, guest::kF_GETFL, 0); }

int GuestSetStatusFlags(int fd, int flags) noexcept {
  return elixir_bionic_fcntl(fd, guest::kF_SETFL, static_cast<std::uint64_t>(static_cast<std::uint32_t>(flags)));
}

int GuestClose(int fd) noexcept { return elixir_bionic_close(fd); }

void GuestWriteStderr(const char* data, std::size_t length) noexcept {
  const int saved = GuestErrno();
  std::size_t done = 0;
  while (done < length) {
    const g_ssize put = elixir_bionic_write(2, data + done, length - done);
    if (put <= 0) break;
    done += static_cast<std::size_t>(put);
  }
  SetGuestErrno(saved);
}

bool UsingFallbackFdTable() noexcept {
  return reinterpret_cast<void*>(&elixir_bionic_fcntl) == reinterpret_cast<void*>(&elixir_net_fallback_fcntl);
}

void InstallFilesHooks() noexcept {
  static std::once_flag once;
  std::call_once(once, [] {
    files::FdOps ops;
    ops.read = &FdRead;
    ops.write = &FdWrite;
    ops.close = &FdClose;
    ops.set_nonblocking = &FdSetNonBlocking;
    ops.bytes_available = &FdBytesAvailable;
    ops.poll = &FdPoll;
    files::SetKindOps(vfs::FdKind::kEvent, ops);
    files::SetKindOps(vfs::FdKind::kSocket, ops);
  });
}

bool HookedReadiness(int fd, std::uint32_t events, std::uint32_t* ready) noexcept {
  return files::HookedFdReadiness(fd, events, ready);
}

}
