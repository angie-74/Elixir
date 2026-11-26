// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/files/files_internal.h"

#include <atomic>

namespace elixir::bionic::files {
namespace {

std::atomic<const FdOps*> g_kind_ops[2];

int KindSlot(vfs::FdKind kind) noexcept {
  switch (kind) {
    case vfs::FdKind::kEvent: return 0;
    case vfs::FdKind::kSocket: return 1;
    default: return -1;
  }
}

std::uint8_t KindByte(const Description& d) noexcept { return static_cast<std::uint8_t>(d.kind); }
bool NonBlocking(const Description& d) noexcept { return (d.status_flags.load() & guest::kO_NONBLOCK) != 0; }

constexpr std::uint32_t kAlwaysReported =
    static_cast<std::uint32_t>(guest::kPOLLERR | guest::kPOLLHUP | guest::kPOLLNVAL);

}

const FdOps* HookOps(const Description& d) noexcept {
  if (d.ops != nullptr) return d.ops;
  if (d.provider != Provider::kHandle) return nullptr;
  const int slot = KindSlot(d.kind);
  return slot < 0 ? nullptr : g_kind_ops[slot].load(std::memory_order_acquire);
}

std::int64_t HookedRead(Description& d, const FdOps& ops, void* buffer, std::size_t count) {
  if (ops.read == nullptr) return FailWith(guest::kEINVAL, std::int64_t{-1});
  int error = 0;
  const abi::g_ssize got = ops.read(d.handle, KindByte(d), buffer, count, NonBlocking(d), &error);
  if (got < 0) return FailWith(error != 0 ? error : guest::kEIO, std::int64_t{-1});
  return got;
}

std::int64_t HookedWrite(Description& d, const FdOps& ops, const void* buffer, std::size_t count) {
  if (ops.write == nullptr) return FailWith(guest::kEINVAL, std::int64_t{-1});
  int error = 0;
  const abi::g_ssize put = ops.write(d.handle, KindByte(d), buffer, count, NonBlocking(d), &error);
  if (put < 0) return FailWith(error != 0 ? error : guest::kEIO, std::int64_t{-1});
  return put;
}

int HookedBytesAvailable(Description& d, const FdOps& ops, int* available) {
  if (ops.bytes_available == nullptr) return FailWith(guest::kENOTTY, -1);
  int error = 0;
  if (ops.bytes_available(d.handle, KindByte(d), available, &error) != 0) {
    return FailWith(error != 0 ? error : guest::kEINVAL, -1);
  }
  return 0;
}

void HookedSetNonBlocking(Description& d, const FdOps& ops, bool nonblocking) noexcept {
  if (ops.set_nonblocking == nullptr) return;
  int error = 0;
  ops.set_nonblocking(d.handle, KindByte(d), nonblocking, &error);
}

std::uint32_t HookedPoll(Description& d, const FdOps& ops, std::uint32_t events) noexcept {
  if (ops.poll == nullptr) return 0;
  return ops.poll(d.handle, KindByte(d), events) & (events | kAlwaysReported);
}

void HookedClose(Description& d, const FdOps& ops) noexcept {
  if (ops.close == nullptr) return;
  const int saved = GuestErrno();
  int error = 0;
  ops.close(d.handle, KindByte(d), &error);
  SetGuestErrno(saved);
}

bool SetKindOps(vfs::FdKind kind, const FdOps& ops) noexcept {
  const int slot = KindSlot(kind);
  if (slot < 0) return false;
  if (ops.read == nullptr && ops.write == nullptr && ops.close == nullptr && ops.set_nonblocking == nullptr &&
      ops.bytes_available == nullptr && ops.poll == nullptr) {
    g_kind_ops[slot].store(nullptr, std::memory_order_release);
    return true;
  }
  auto* copy = new (std::nothrow) FdOps(ops);
  if (copy == nullptr) return false;
  g_kind_ops[slot].store(copy, std::memory_order_release);
  return true;
}

int InstallHookedFd(void* handle, const FdOps* ops, int open_flags, std::string_view name) noexcept {
  if (ops == nullptr) return FailWith(guest::kEINVAL, -1);
  try {
    auto d = std::make_shared<Description>();
    d->kind = vfs::FdKind::kEvent;
    d->provider = Provider::kHandle;
    d->handle = handle;
    d->status_flags.store(open_flags & ~guest::kO_CLOEXEC);
    d->guest_path.assign(name);
    d->ops = ops;
    int fd = -1;
    try {
      fd = InstallDescription(d, (open_flags & guest::kO_CLOEXEC) != 0);
    } catch (...) {
      fd = FailWith(guest::kENOMEM, -1);
    }
    if (fd < 0) {
      d->ops = nullptr;
      d->handle = nullptr;
      d->kind = vfs::FdKind::kNull;
    }
    return fd;
  } catch (...) {
    return FailWith(guest::kENOMEM, -1);
  }
}

void* HookedFdHandle(int fd, const FdOps* ops) noexcept {
  try {
    DescriptionPtr d = FindDescription(fd);
    if (d == nullptr || ops == nullptr || d->ops != ops) return nullptr;
    return d->handle;
  } catch (...) {
    return nullptr;
  }
}

bool HookedFdReadiness(int fd, std::uint32_t events, std::uint32_t* ready) noexcept {
  try {
    DescriptionPtr d = FindDescription(fd);
    if (d == nullptr || ready == nullptr) return false;
    const FdOps* ops = HookOps(*d);
    if (ops == nullptr || ops->poll == nullptr) return false;
    *ready = HookedPoll(*d, *ops, events);
    return true;
  } catch (...) {
    return false;
  }
}

}
