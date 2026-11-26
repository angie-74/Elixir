// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// android_crash_detail_* follows the semantics of bionic
// libc/bionic/android_crash_detail.cpp and syslog-free fragments of other
// bionic files (pty.cpp, utmp.cpp, sys_*.cpp, umount.cpp, setpgrp.cpp):
//
//   Copyright (C) 2024 The Android Open Source Project (android_crash_detail.cpp)
//   Copyright (C) 2008 The Android Open Source Project (pty.cpp, setpgrp.cpp, umount.cpp)
//   Copyright (C) 2020 The Android Open Source Project (utmp.cpp)
//   Copyright (C) 2016 The Android Open Source Project (sys_msg.cpp, sys_sem.cpp, sys_shm.cpp)
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in
//      the documentation and/or other materials provided with the
//      distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
//   OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
//   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
//   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
//   SUCH DAMAGE.
//
// nice() follows NetBSD libc/gen/nice.c as shipped in bionic
// (upstream-netbsd):
//
//   Copyright (c) 1983, 1993
//   The Regents of the University of California.  All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//   3. Neither the name of the University nor the names of its contributors
//      may be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
//   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
//   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
//   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
//   SUCH DAMAGE.
//
// ftok() follows OpenBSD libc/gen/ftok.c as shipped in bionic
// (upstream-openbsd):
//
//   Copyright (c) 1994 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//   3. The name of the author may not be used to endorse or promote products
//      derived from this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
//   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
//   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
//   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
//   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
//   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include "bionic/core/process_ext.h"

#include <windows.h>

#include <atomic>
#include <climits>
#include <cstdio>
#include <cstring>
#include <vector>

#include "bionic/core/core.h"
#include "bionic/core/core_internal.h"
#include "bionic/guest_constants.h"
#include "bionic/runtime.h"
#include "bionic/threads.h"

namespace elixir::bionic {

ELIXIR_BIONIC_FN(int, getpriority)(int which, abi::g_uid who) noexcept;
ELIXIR_BIONIC_FN(int, setpriority)(int which, abi::g_uid who, int value) noexcept;
ELIXIR_BIONIC_FN(int, setpgid)(abi::g_pid pid, abi::g_pid group) noexcept;
ELIXIR_BIONIC_FN(abi::g_pid, setsid)() noexcept;

namespace core {
namespace {

struct GuestTimex {
  std::uint32_t modes;
  std::int32_t pad0;
  std::int64_t offset;
  std::int64_t freq;
  std::int64_t maxerror;
  std::int64_t esterror;
  std::int32_t status;
  std::int32_t pad1;
  std::int64_t constant;
  std::int64_t precision;
  std::int64_t tolerance;
  guest::Timeval time;
  std::int64_t tick;
  std::int64_t ppsfreq;
  std::int64_t jitter;
  std::int32_t shift;
  std::int32_t pad2;
  std::int64_t stabil;
  std::int64_t jitcnt;
  std::int64_t calcnt;
  std::int64_t errcnt;
  std::int64_t stbcnt;
  std::int32_t tai;
  std::int32_t reserved[11];
};
static_assert(sizeof(GuestTimex) == 208, "struct timex");
static_assert(offsetof(GuestTimex, status) == 40 && offsetof(GuestTimex, time) == 72 &&
                  offsetof(GuestTimex, tick) == 88 && offsetof(GuestTimex, shift) == 112 &&
                  offsetof(GuestTimex, tai) == 160,
              "struct timex fields");

constexpr std::uint32_t kAdjOffsetReadonly = 0x2000;
constexpr std::uint32_t kAdjAdjtime = 0x8000;
constexpr int kTimeOk = 0;
constexpr std::int64_t kNtpPhaseLimit = 16000000;
constexpr std::int64_t kTolerance = 500 << 16;
constexpr std::int64_t kTickUsec = 10000;

struct GuestCapHeader {
  std::uint32_t version;
  std::int32_t pid;
};
struct GuestCapData {
  std::uint32_t effective;
  std::uint32_t permitted;
  std::uint32_t inheritable;
};
static_assert(sizeof(GuestCapHeader) == 8 && sizeof(GuestCapData) == 12, "linux/capability.h");
constexpr std::uint32_t kCapVersion1 = 0x19980330, kCapVersion2 = 0x20071026, kCapVersion3 = 0x20080522;

constexpr int kMntForce = 1, kMntDetach = 2, kMntExpire = 4, kUmountNofollow = 8;
constexpr int kNoCtty = 0x100;
constexpr abi::g_size kMaxRwCount = 0x7ffff000;
constexpr std::uintptr_t kPageSize = 4096;
constexpr unsigned kSpliceFAll = 0x0f;

bool IsFifo(const guest::Stat& st) noexcept {
  return (st.st_mode & guest::kS_IFMT) == static_cast<std::uint32_t>(guest::kS_IFIFO);
}

bool OwnTask(abi::g_pid pid) noexcept {
  if (pid == CurrentPid()) return true;
  if (pid <= 0) return false;
  HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threads::WindowsThreadIdFromTid(pid));
  if (thread == nullptr) return false;
  DWORD code = 0;
  const bool ours =
      GetProcessIdOfThread(thread) == GetCurrentProcessId() && GetExitCodeThread(thread, &code) && code == STILL_ACTIVE;
  CloseHandle(thread);
  return ours;
}

bool OtherProcessExists(abi::g_pid pid) noexcept {
  if (pid <= 0) return false;
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (process == nullptr) return GetLastError() == ERROR_ACCESS_DENIED;
  DWORD code = 0;
  const bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
  CloseHandle(process);
  return alive;
}

int FdStat(int fd, guest::Stat* st) noexcept {
  const LateServices services = BoundLateServices();
  if (services.fstat == nullptr) return guest::kENOSYS;
  const int saved = GuestErrno();
  const int result = services.fstat(fd, st);
  const int error = result == 0 ? 0 : GuestErrno();
  SetGuestErrno(saved);
  return result == 0 ? 0 : (error != 0 ? error : guest::kEBADF);
}

abi::g_size CopyChecked(void* destination, const void* source, abi::g_size length) noexcept {
  SIZE_T done = 0;
  if (ReadProcessMemory(GetCurrentProcess(), source, destination, length, &done) && done == length) return length;
  abi::g_size copied = 0;
  auto src = reinterpret_cast<std::uintptr_t>(source);
  auto dst = reinterpret_cast<std::uintptr_t>(destination);
  while (copied < length) {
    abi::g_size chunk = length - copied;
    const abi::g_size src_room = kPageSize - (src % kPageSize);
    const abi::g_size dst_room = kPageSize - (dst % kPageSize);
    if (chunk > src_room) chunk = src_room;
    if (chunk > dst_room) chunk = dst_room;
    done = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(src), reinterpret_cast<void*>(dst),
                           chunk, &done) ||
        done != chunk) {
      return copied;
    }
    copied += chunk;
    src += chunk;
    dst += chunk;
  }
  return copied;
}

bool ReadIovecs(const guest::Iovec* iov, abi::g_ulong count, std::vector<guest::Iovec>* out) noexcept {
  try {
    out->resize(count);
  } catch (...) {
    return false;
  }
  if (count == 0) return true;
  const abi::g_size bytes = count * sizeof(guest::Iovec);
  return CopyChecked(out->data(), iov, bytes) == bytes;
}

abi::g_ssize ProcessVm(abi::g_pid pid, const guest::Iovec* local_iov, abi::g_ulong liovcnt,
                       const guest::Iovec* remote_iov, abi::g_ulong riovcnt, abi::g_ulong flags, bool write) noexcept {
  if (flags != 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  if (liovcnt > static_cast<abi::g_ulong>(guest::kIOV_MAX)) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  std::vector<guest::Iovec> local;
  if (!ReadIovecs(local_iov, liovcnt, &local)) return FailWith(guest::kEFAULT, abi::g_ssize{-1});
  abi::g_size local_total = 0;
  for (guest::Iovec& segment : local) {
    if (static_cast<abi::g_ssize>(segment.iov_len) < 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
    if (segment.iov_len > kMaxRwCount - local_total) segment.iov_len = kMaxRwCount - local_total;
    local_total += segment.iov_len;
  }
  if (local_total == 0) return 0;
  if (riovcnt > static_cast<abi::g_ulong>(guest::kIOV_MAX)) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  std::vector<guest::Iovec> remote;
  if (!ReadIovecs(remote_iov, riovcnt, &remote)) return FailWith(guest::kEFAULT, abi::g_ssize{-1});
  bool any_remote = false;
  for (const guest::Iovec& segment : remote) {
    if (static_cast<abi::g_ssize>(segment.iov_len) < 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
    any_remote |= segment.iov_len != 0;
  }
  if (!any_remote) return 0;
  if (!OwnTask(pid)) {
    return FailWith(OtherProcessExists(pid) ? guest::kEPERM : guest::kESRCH, abi::g_ssize{-1});
  }
  abi::g_size total = 0;
  std::size_t li = 0;
  abi::g_size loff = 0;
  for (const guest::Iovec& segment : remote) {
    abi::g_size roff = 0;
    while (roff < segment.iov_len) {
      while (li < local.size() && loff == local[li].iov_len) {
        ++li;
        loff = 0;
      }
      if (li == local.size()) return static_cast<abi::g_ssize>(total);
      abi::g_size chunk = segment.iov_len - roff;
      if (chunk > local[li].iov_len - loff) chunk = local[li].iov_len - loff;
      char* remote_address = static_cast<char*>(segment.iov_base) + roff;
      char* local_address = static_cast<char*>(local[li].iov_base) + loff;
      const abi::g_size copied = write ? CopyChecked(remote_address, local_address, chunk)
                                       : CopyChecked(local_address, remote_address, chunk);
      total += copied;
      if (copied != chunk) {
        if (total != 0) return static_cast<abi::g_ssize>(total);
        return FailWith(guest::kEFAULT, abi::g_ssize{-1});
      }
      roff += chunk;
      loff += chunk;
    }
  }
  return static_cast<abi::g_ssize>(total);
}

int Adjtimex(GuestTimex* tx) noexcept {
  if (tx == nullptr) return FailWith(guest::kEFAULT, -1);
  GuestTimex in;
  if (CopyChecked(&in, tx, sizeof(in)) != sizeof(in)) return FailWith(guest::kEFAULT, -1);
  const bool read_only = (in.modes & kAdjAdjtime) != 0 ? (in.modes & kAdjOffsetReadonly) != 0 : in.modes == 0;
  if (!read_only) return FailWith(guest::kEPERM, -1);
  GuestTimex out = in;
  out.offset = 0;
  out.freq = 0;
  DWORD adjustment = 0, increment = 0;
  BOOL disabled = TRUE;
  if (GetSystemTimeAdjustment(&adjustment, &increment, &disabled) && !disabled && increment != 0) {
    out.freq = (static_cast<std::int64_t>(adjustment) - static_cast<std::int64_t>(increment)) * 65536 * 1000000 /
               static_cast<std::int64_t>(increment);
  }
  out.maxerror = kNtpPhaseLimit;
  out.esterror = kNtpPhaseLimit;
  out.status = 0;
  out.constant = 2;
  out.precision = 1;
  out.tolerance = kTolerance;
  const std::int64_t now_ns = RealtimeNanoseconds();
  out.time.tv_sec = now_ns / 1000000000;
  out.time.tv_usec = (now_ns % 1000000000) / 1000;
  out.tick = kTickUsec;
  out.ppsfreq = 0;
  out.jitter = 0;
  out.shift = 0;
  out.stabil = 0;
  out.jitcnt = 0;
  out.calcnt = 0;
  out.errcnt = 0;
  out.stbcnt = 0;
  out.tai = 0;
  if (CopyChecked(tx, &out, sizeof(out)) != sizeof(out)) return FailWith(guest::kEFAULT, -1);
  return kTimeOk;
}

int CapabilityStructs(GuestCapHeader* header, int* count) noexcept {
  switch (header->version) {
    case kCapVersion1: *count = 1; return 0;
    case kCapVersion2:
    case kCapVersion3: *count = 2; return 0;
    default:
      header->version = kCapVersion3;
      return guest::kEINVAL;
  }
}

constexpr std::size_t kNumCrashDetails = 128;

struct CrashDetail {
  std::atomic<const char*> name;
  std::atomic<std::size_t> name_size;
  std::atomic<const char*> data;
  std::atomic<std::size_t> data_size;
  std::atomic<bool> live;
  CrashDetail* next_free;
};

struct CrashDetailPage {
  CrashDetailPage* prev;
  std::atomic<std::size_t> used;
  CrashDetail details[kNumCrashDetails];
};

SRWLOCK g_crash_lock = SRWLOCK_INIT;
std::atomic<CrashDetailPage*> g_crash_page{nullptr};
CrashDetail* g_crash_free = nullptr;

void Populate(CrashDetail* detail, const void* name, std::size_t name_size, const void* data,
              std::size_t data_size) noexcept {
  detail->name.store(static_cast<const char*>(name), std::memory_order_relaxed);
  detail->name_size.store(name_size, std::memory_order_relaxed);
  detail->data.store(static_cast<const char*>(data), std::memory_order_relaxed);
  detail->data_size.store(data_size, std::memory_order_relaxed);
  detail->live.store(true, std::memory_order_release);
}

thread_local char t_ptsname_buffer[32];

}

std::size_t SnapshotCrashDetails(CrashDetailView* out, std::size_t capacity) noexcept {
  std::size_t live = 0;
  for (CrashDetailPage* page = g_crash_page.load(std::memory_order_acquire); page != nullptr; page = page->prev) {
    const std::size_t used = page->used.load(std::memory_order_acquire);
    for (std::size_t i = 0; i < used && i < kNumCrashDetails; ++i) {
      const CrashDetail& detail = page->details[i];
      if (!detail.live.load(std::memory_order_acquire)) continue;
      if (out != nullptr && live < capacity) {
        out[live].name = detail.name.load(std::memory_order_relaxed);
        out[live].name_size = detail.name_size.load(std::memory_order_relaxed);
        out[live].data = detail.data.load(std::memory_order_relaxed);
        out[live].data_size = detail.data_size.load(std::memory_order_relaxed);
      }
      ++live;
    }
  }
  return live;
}

}

using core::CrashDetail;

ELIXIR_BIONIC_FN(CrashDetail*, android_crash_detail_register)(const void* name, abi::g_size name_size,
                                                              const void* data, abi::g_size data_size) noexcept {
  using namespace core;
  AcquireSRWLockExclusive(&g_crash_lock);
  CrashDetail* detail = g_crash_free;
  if (detail != nullptr) {
    g_crash_free = detail->next_free;
    detail->next_free = nullptr;
  } else {
    CrashDetailPage* page = g_crash_page.load(std::memory_order_relaxed);
    if (page == nullptr || page->used.load(std::memory_order_relaxed) == kNumCrashDetails) {
      auto* fresh = static_cast<CrashDetailPage*>(
          VirtualAlloc(nullptr, sizeof(CrashDetailPage), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
      if (fresh == nullptr) {
        ReleaseSRWLockExclusive(&g_crash_lock);
        HostLog(6, "libc", "failed to allocate crash_detail_page");
        return nullptr;
      }
      fresh->prev = page;
      g_crash_page.store(fresh, std::memory_order_release);
      page = fresh;
    }
    const std::size_t index = page->used.load(std::memory_order_relaxed);
    detail = &page->details[index];
    Populate(detail, name, name_size, data, data_size);
    page->used.store(index + 1, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_crash_lock);
    return detail;
  }
  Populate(detail, name, name_size, data, data_size);
  ReleaseSRWLockExclusive(&g_crash_lock);
  return detail;
}

ELIXIR_BIONIC_FN(void, android_crash_detail_unregister)(CrashDetail* detail) noexcept {
  using namespace core;
  if (detail == nullptr) return;
  AcquireSRWLockExclusive(&g_crash_lock);
  if (detail->live.load(std::memory_order_relaxed)) {
    detail->live.store(false, std::memory_order_release);
    detail->name.store(nullptr, std::memory_order_relaxed);
    detail->data.store(nullptr, std::memory_order_relaxed);
    detail->next_free = g_crash_free;
    g_crash_free = detail;
  }
  ReleaseSRWLockExclusive(&g_crash_lock);
}

ELIXIR_BIONIC_FN(void, android_crash_detail_replace_data)(CrashDetail* detail, const void* data,
                                                          abi::g_size data_size) noexcept {
  if (detail == nullptr) return;
  detail->data.store(static_cast<const char*>(data), std::memory_order_relaxed);
  detail->data_size.store(data_size, std::memory_order_release);
}

ELIXIR_BIONIC_FN(void, android_crash_detail_replace_name)(CrashDetail* detail, const void* name,
                                                          abi::g_size name_size) noexcept {
  if (detail == nullptr) return;
  detail->name.store(static_cast<const char*>(name), std::memory_order_relaxed);
  detail->name_size.store(name_size, std::memory_order_release);
}

ELIXIR_BIONIC_FN(void, android_reset_stack_guards)() noexcept {}

ELIXIR_BIONIC_FN(int, acct)(const char* path) noexcept {
  (void)path;
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, chroot)(const char* path) noexcept {
  (void)path;
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, init_module)(void* image, abi::g_ulong length, const char* params) noexcept {
  (void)image;
  (void)length;
  (void)params;
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, delete_module)(const char* name, unsigned flags) noexcept {
  (void)name;
  (void)flags;
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, klogctl)(int type, char* buffer, int length) noexcept {
  (void)type;
  (void)buffer;
  (void)length;
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, mount)(const char* source, const char* target, const char* fstype, abi::g_ulong flags,
                             const void* data) noexcept {
  (void)source;
  (void)target;
  (void)fstype;
  (void)flags;
  (void)data;
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, umount2)(const char* target, int flags) noexcept {
  (void)target;
  using namespace core;
  if ((flags & ~(kMntForce | kMntDetach | kMntExpire | kUmountNofollow)) != 0) return FailWith(guest::kEINVAL, -1);
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, umount)(const char* target) noexcept { return elixir_bionic_umount2(target, 0); }
ELIXIR_BIONIC_FN(int, quotactl)(int command, const char* special, int id, char* address) noexcept {
  (void)command;
  (void)special;
  (void)id;
  (void)address;
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, reboot)(int mode) noexcept {
  (void)mode;
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, swapon)(const char* path, int flags) noexcept {
  (void)path;
  (void)flags;
  return FailWith(guest::kEPERM, -1);
}
ELIXIR_BIONIC_FN(int, swapoff)(const char* path) noexcept {
  (void)path;
  return FailWith(guest::kEPERM, -1);
}

ELIXIR_BIONIC_FN(int, capget)(core::GuestCapHeader* header, core::GuestCapData* data) noexcept {
  using namespace core;
  if (header == nullptr) return FailWith(guest::kEFAULT, -1);
  int count = 0;
  const int error = CapabilityStructs(header, &count);
  if (data == nullptr || error != 0) {
    if (data == nullptr && error == guest::kEINVAL) return 0;
    return error != 0 ? FailWith(error, -1) : 0;
  }
  const abi::g_pid pid = header->pid;
  if (pid < 0) return FailWith(guest::kEINVAL, -1);
  if (pid != 0 && !OwnTask(pid)) {
    return FailWith(OtherProcessExists(pid) ? guest::kEACCES : guest::kESRCH, -1);
  }
  GuestCapData empty[2] = {};
  const abi::g_size bytes = static_cast<abi::g_size>(count) * sizeof(GuestCapData);
  if (CopyChecked(data, empty, bytes) != bytes) return FailWith(guest::kEFAULT, -1);
  return 0;
}

ELIXIR_BIONIC_FN(int, capset)(core::GuestCapHeader* header, const core::GuestCapData* data) noexcept {
  using namespace core;
  if (header == nullptr) return FailWith(guest::kEFAULT, -1);
  int count = 0;
  const int error = CapabilityStructs(header, &count);
  if (error != 0) return FailWith(error, -1);
  if (header->pid != 0 && header->pid != CurrentTid()) return FailWith(guest::kEPERM, -1);
  if (data == nullptr) return FailWith(guest::kEFAULT, -1);
  GuestCapData requested[2] = {};
  const abi::g_size bytes = static_cast<abi::g_size>(count) * sizeof(GuestCapData);
  if (CopyChecked(requested, data, bytes) != bytes) return FailWith(guest::kEFAULT, -1);
  for (int i = 0; i < count; ++i) {
    if (requested[i].effective != 0 || requested[i].permitted != 0 || requested[i].inheritable != 0) {
      return FailWith(guest::kEPERM, -1);
    }
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, adjtimex)(core::GuestTimex* tx) noexcept { return core::Adjtimex(tx); }

ELIXIR_BIONIC_FN(int, clock_adjtime)(std::int32_t clock_id, core::GuestTimex* tx) noexcept {
  if (tx == nullptr) return FailWith(guest::kEFAULT, -1);
  core::GuestTimex probe;
  if (core::CopyChecked(&probe, tx, sizeof(probe)) != sizeof(probe)) return FailWith(guest::kEFAULT, -1);
  if (clock_id == guest::kCLOCK_REALTIME) return core::Adjtimex(tx);
  if (clock_id < 0) {
    return FailWith((clock_id & 7) == 3 ? guest::kEINVAL : guest::kEOPNOTSUPP, -1);
  }
  switch (clock_id) {
    case guest::kCLOCK_MONOTONIC:
    case guest::kCLOCK_PROCESS_CPUTIME_ID:
    case guest::kCLOCK_THREAD_CPUTIME_ID:
    case guest::kCLOCK_MONOTONIC_RAW:
    case guest::kCLOCK_REALTIME_COARSE:
    case guest::kCLOCK_MONOTONIC_COARSE:
    case guest::kCLOCK_BOOTTIME:
    case guest::kCLOCK_REALTIME_ALARM:
    case guest::kCLOCK_BOOTTIME_ALARM:
    case guest::kCLOCK_TAI: return FailWith(guest::kEOPNOTSUPP, -1);
    default: return FailWith(guest::kEINVAL, -1);
  }
}

ELIXIR_BIONIC_FN(int, nice)(int increment) noexcept {
  SetGuestErrno(0);
  const int priority = elixir_bionic_getpriority(guest::kPRIO_PROCESS, 0);
  if (priority == -1 && GuestErrno() != 0) return -1;
  std::int64_t wanted = static_cast<std::int64_t>(priority) + increment;
  if (wanted > INT_MAX) wanted = INT_MAX;
  if (wanted < INT_MIN) wanted = INT_MIN;
  if (elixir_bionic_setpriority(guest::kPRIO_PROCESS, 0, static_cast<int>(wanted)) == -1) {
    if (GuestErrno() == guest::kEACCES) SetGuestErrno(guest::kEPERM);
    return -1;
  }
  return elixir_bionic_getpriority(guest::kPRIO_PROCESS, 0);
}

ELIXIR_BIONIC_FN(int, setpgrp)() noexcept { return elixir_bionic_setpgid(0, 0); }

ELIXIR_BIONIC_FN(abi::g_ssize, process_vm_readv)(abi::g_pid pid, const guest::Iovec* local_iov,
                                                 abi::g_ulong liovcnt, const guest::Iovec* remote_iov,
                                                 abi::g_ulong riovcnt, abi::g_ulong flags) noexcept {
  return core::ProcessVm(pid, local_iov, liovcnt, remote_iov, riovcnt, flags, false);
}

ELIXIR_BIONIC_FN(abi::g_ssize, process_vm_writev)(abi::g_pid pid, const guest::Iovec* local_iov,
                                                  abi::g_ulong liovcnt, const guest::Iovec* remote_iov,
                                                  abi::g_ulong riovcnt, abi::g_ulong flags) noexcept {
  return core::ProcessVm(pid, local_iov, liovcnt, remote_iov, riovcnt, flags, true);
}

ELIXIR_BIONIC_FN(abi::g_ssize, splice)(int fd_in, abi::g_off* off_in, int fd_out, abi::g_off* off_out,
                                       abi::g_size length, unsigned flags) noexcept {
  using namespace core;
  if (length == 0) return 0;
  if ((flags & ~kSpliceFAll) != 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  guest::Stat in{};
  guest::Stat out{};
  int error = FdStat(fd_in, &in);
  if (error == 0) error = FdStat(fd_out, &out);
  if (error == guest::kENOSYS) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  if (error != 0) return FailWith(guest::kEBADF, abi::g_ssize{-1});
  if ((IsFifo(in) && off_in != nullptr) || (IsFifo(out) && off_out != nullptr)) {
    return FailWith(guest::kESPIPE, abi::g_ssize{-1});
  }
  return FailWith(guest::kEINVAL, abi::g_ssize{-1});
}

ELIXIR_BIONIC_FN(abi::g_ssize, tee)(int fd_in, int fd_out, abi::g_size length, unsigned flags) noexcept {
  using namespace core;
  if ((flags & ~kSpliceFAll) != 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  if (length == 0) return 0;
  guest::Stat st{};
  int error = FdStat(fd_in, &st);
  if (error == 0) error = FdStat(fd_out, &st);
  if (error != 0 && error != guest::kENOSYS) return FailWith(guest::kEBADF, abi::g_ssize{-1});
  return FailWith(guest::kEINVAL, abi::g_ssize{-1});
}

ELIXIR_BIONIC_FN(abi::g_ssize, vmsplice)(int fd, const guest::Iovec* iov, abi::g_size count, unsigned flags) noexcept {
  using namespace core;
  if ((flags & ~kSpliceFAll) != 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  guest::Stat st{};
  const int error = FdStat(fd, &st);
  if (error != 0 && error != guest::kENOSYS) return FailWith(guest::kEBADF, abi::g_ssize{-1});
  if (count > static_cast<abi::g_size>(guest::kIOV_MAX)) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  std::vector<guest::Iovec> segments;
  if (!ReadIovecs(iov, count, &segments)) return FailWith(guest::kEFAULT, abi::g_ssize{-1});
  bool any = false;
  for (const guest::Iovec& segment : segments) {
    if (static_cast<abi::g_ssize>(segment.iov_len) < 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
    any |= segment.iov_len != 0;
  }
  if (!any) return 0;
  if (error == 0 && !IsFifo(st)) return FailWith(guest::kEBADF, abi::g_ssize{-1});
  return FailWith(guest::kEINVAL, abi::g_ssize{-1});
}

ELIXIR_BIONIC_FN(abi::g_ssize, readahead)(int fd, abi::g_off offset, abi::g_size count) noexcept {
  (void)offset;
  guest::Stat st{};
  const int error = core::FdStat(fd, &st);
  if (error == guest::kENOSYS) return FailWith(guest::kENOSYS, abi::g_ssize{-1});
  if (error != 0) return FailWith(guest::kEBADF, abi::g_ssize{-1});
  const std::uint32_t type = st.st_mode & guest::kS_IFMT;
  if (type != guest::kS_IFREG && type != guest::kS_IFBLK) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  if (static_cast<abi::g_ssize>(count) < 0) return FailWith(guest::kEINVAL, abi::g_ssize{-1});
  return 0;
}

ELIXIR_BIONIC_FN(int, msgctl)(int id, int command, void* buffer) noexcept {
  (void)id;
  (void)command;
  (void)buffer;
  return FailWith(guest::kENOSYS, -1);
}
ELIXIR_BIONIC_FN(int, msgget)(std::int32_t key, int flags) noexcept {
  (void)key;
  (void)flags;
  return FailWith(guest::kENOSYS, -1);
}
ELIXIR_BIONIC_FN(abi::g_ssize, msgrcv)(int id, void* message, abi::g_size size, abi::g_long type, int flags) noexcept {
  (void)id;
  (void)message;
  (void)size;
  (void)type;
  (void)flags;
  return FailWith(guest::kENOSYS, abi::g_ssize{-1});
}
ELIXIR_BIONIC_FN(int, msgsnd)(int id, const void* message, abi::g_size size, int flags) noexcept {
  (void)id;
  (void)message;
  (void)size;
  (void)flags;
  return FailWith(guest::kENOSYS, -1);
}

ELIXIR_BIONIC_FN(int, semctl)(int id, int number, int command) noexcept {
  (void)id;
  (void)number;
  (void)command;
  return FailWith(guest::kENOSYS, -1);
}
ELIXIR_BIONIC_FN(int, semget)(std::int32_t key, int count, int flags) noexcept {
  (void)key;
  (void)count;
  (void)flags;
  return FailWith(guest::kENOSYS, -1);
}
ELIXIR_BIONIC_FN(int, semtimedop)(int id, void* operations, abi::g_size count, const guest::Timespec* timeout) noexcept {
  (void)id;
  (void)operations;
  (void)count;
  (void)timeout;
  return FailWith(guest::kENOSYS, -1);
}
ELIXIR_BIONIC_FN(int, semop)(int id, void* operations, abi::g_size count) noexcept {
  return elixir_bionic_semtimedop(id, operations, count, nullptr);
}
ELIXIR_BIONIC_FN(void*, shmat)(int id, const void* address, int flags) noexcept {
  (void)id;
  (void)address;
  (void)flags;
  return FailWith(guest::kENOSYS, reinterpret_cast<void*>(-1));
}
ELIXIR_BIONIC_FN(int, shmctl)(int id, int command, void* buffer) noexcept {
  (void)id;
  (void)command;
  (void)buffer;
  return FailWith(guest::kENOSYS, -1);
}
ELIXIR_BIONIC_FN(int, shmdt)(const void* address) noexcept {
  (void)address;
  return FailWith(guest::kENOSYS, -1);
}
ELIXIR_BIONIC_FN(int, shmget)(std::int32_t key, abi::g_size size, int flags) noexcept {
  (void)key;
  (void)size;
  (void)flags;
  return FailWith(guest::kENOSYS, -1);
}

ELIXIR_BIONIC_FN(std::int32_t, ftok)(const char* path, int id) noexcept {
  const core::LateServices services = core::BoundLateServices();
  if (services.stat == nullptr) return FailWith(guest::kENOSYS, std::int32_t{-1});
  guest::Stat st{};
  if (services.stat(path, &st) == -1) return -1;
  const auto u_id = static_cast<std::uint32_t>(id);
  return static_cast<std::int32_t>((u_id & 0xff) << 24 | static_cast<std::uint32_t>(st.st_dev & 0xff) << 16 |
                                   static_cast<std::uint32_t>(st.st_ino & 0xffff));
}

ELIXIR_BIONIC_FN(void, setutent)() noexcept {}
ELIXIR_BIONIC_FN(void, endutent)() noexcept {}
ELIXIR_BIONIC_FN(void*, getutent)() noexcept { return nullptr; }
ELIXIR_BIONIC_FN(void*, pututline)(const void* entry) noexcept {
  (void)entry;
  return nullptr;
}
ELIXIR_BIONIC_FN(int, utmpname)(const char* path) noexcept {
  (void)path;
  return FailWith(guest::kENOTSUP, -1);
}

ELIXIR_BIONIC_FN(int, posix_openpt)(int flags) noexcept {
  (void)flags;
  return FailWith(guest::kENOENT, -1);
}

ELIXIR_BIONIC_FN(int, getpt)() noexcept { return elixir_bionic_posix_openpt(guest::kO_RDWR | core::kNoCtty); }

ELIXIR_BIONIC_FN(int, grantpt)(int fd) noexcept {
  (void)fd;
  return 0;
}

ELIXIR_BIONIC_FN(int, unlockpt)(int fd) noexcept {
  guest::Stat st{};
  const int error = core::FdStat(fd, &st);
  if (error != 0 && error != guest::kENOSYS) return FailWith(guest::kEBADF, -1);
  return FailWith(guest::kENOTTY, -1);
}

ELIXIR_BIONIC_FN(int, ptsname_r)(int fd, char* buffer, abi::g_size length) noexcept {
  (void)fd;
  (void)length;
  if (buffer == nullptr) return FailWith(guest::kEINVAL, guest::kEINVAL);
  return FailWith(guest::kENOTTY, guest::kENOTTY);
}

ELIXIR_BIONIC_FN(char*, ptsname)(int fd) noexcept {
  char* buffer = core::t_ptsname_buffer;
  const int error = elixir_bionic_ptsname_r(fd, buffer, sizeof(core::t_ptsname_buffer));
  return error == 0 ? buffer : nullptr;
}

ELIXIR_BIONIC_FN(int, openpty)(int* pty, int* tty, char* name, const guest::Termios* termios,
                               const guest::Winsize* winsize) noexcept {
  (void)tty;
  (void)name;
  (void)termios;
  (void)winsize;
  const int master = elixir_bionic_getpt();
  if (pty != nullptr) *pty = master;
  return -1;
}

ELIXIR_BIONIC_FN(int, forkpty)(int* parent_pty, char* child_tty_name, const guest::Termios* termios,
                               const guest::Winsize* winsize) noexcept {
  int pty = -1;
  int tty = -1;
  (void)parent_pty;
  return elixir_bionic_openpty(&pty, &tty, child_tty_name, termios, winsize);
}

ELIXIR_BIONIC_FN(int, login_tty)(int fd) noexcept {
  const int saved = GuestErrno();
  elixir_bionic_setsid();
  SetGuestErrno(saved);
  guest::Stat st{};
  const int error = core::FdStat(fd, &st);
  if (error != 0 && error != guest::kENOSYS) return FailWith(guest::kEBADF, -1);
  return FailWith(guest::kENOTTY, -1);
}

ELIXIR_BIONIC_FN(int, signalfd)(int fd, const guest::Sigset* mask, int flags) noexcept {
  (void)fd;
  (void)mask;
  (void)flags;
  return FailWith(guest::kENOSYS, -1);
}

namespace core {

void RegisterSysmiscExports(ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(android_crash_detail_register);
  ELIXIR_ADD(android_crash_detail_unregister);
  ELIXIR_ADD(android_crash_detail_replace_data);
  ELIXIR_ADD(android_crash_detail_replace_name);
  ELIXIR_ADD(android_reset_stack_guards);
  ELIXIR_ADD(acct);
  ELIXIR_ADD(chroot);
  ELIXIR_ADD(init_module);
  ELIXIR_ADD(delete_module);
  ELIXIR_ADD(klogctl);
  ELIXIR_ADD(mount);
  ELIXIR_ADD(umount);
  ELIXIR_ADD(umount2);
  ELIXIR_ADD(quotactl);
  ELIXIR_ADD(reboot);
  ELIXIR_ADD(swapon);
  ELIXIR_ADD(swapoff);
  ELIXIR_ADD(capget);
  ELIXIR_ADD(capset);
  ELIXIR_ADD(adjtimex);
  ELIXIR_ADD(clock_adjtime);
  ELIXIR_ADD(nice);
  ELIXIR_ADD(setpgrp);
  ELIXIR_ADD(process_vm_readv);
  ELIXIR_ADD(process_vm_writev);
  ELIXIR_ADD(splice);
  ELIXIR_ADD(tee);
  ELIXIR_ADD(vmsplice);
  ELIXIR_ADD(readahead);
  ELIXIR_ADD(msgctl);
  ELIXIR_ADD(msgget);
  ELIXIR_ADD(msgrcv);
  ELIXIR_ADD(msgsnd);
  ELIXIR_ADD(semctl);
  ELIXIR_ADD(semget);
  ELIXIR_ADD(semop);
  ELIXIR_ADD(semtimedop);
  ELIXIR_ADD(shmat);
  ELIXIR_ADD(shmctl);
  ELIXIR_ADD(shmdt);
  ELIXIR_ADD(shmget);
  ELIXIR_ADD(ftok);
  ELIXIR_ADD(setutent);
  ELIXIR_ADD(endutent);
  ELIXIR_ADD(getutent);
  ELIXIR_ADD(pututline);
  ELIXIR_ADD(utmpname);
  ELIXIR_ADD(posix_openpt);
  ELIXIR_ADD(getpt);
  ELIXIR_ADD(grantpt);
  ELIXIR_ADD(unlockpt);
  ELIXIR_ADD(ptsname);
  ELIXIR_ADD(ptsname_r);
  ELIXIR_ADD(openpty);
  ELIXIR_ADD(forkpty);
  ELIXIR_ADD(login_tty);
  ELIXIR_ADD(signalfd);
  table.Add(kLibc, "signalfd64", reinterpret_cast<void*>(&elixir_bionic_signalfd));
#undef ELIXIR_ADD
}

}
}
