// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/spawn.cpp (posix_spawnattr_* and
// posix_spawn_file_actions_*), pthread_sigqueue.cpp and clone.cpp:
//
//   Copyright (C) 2017 The Android Open Source Project (spawn.cpp)
//   Copyright (C) 2018 The Android Open Source Project (pthread_sigqueue.cpp)
//   Copyright (C) 2010 The Android Open Source Project (clone.cpp)
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
// execl/execle/execlp/execv/execvp/execvpe/fexecve follow bionic
// libc/bionic/exec.cpp:
//
//   Copyright (c) 1991, 1993
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
#include "bionic/core/process_ext.h"

#include <windows.h>

#include <atomic>
#include <cstring>
#include <string>

#include "abi/sysv_abi.h"
#include "abi/sysv_va_list.h"
#include "abi/variadic_thunk.h"
#include "bionic/core/core.h"
#include "bionic/core/core_internal.h"
#include "bionic/guest_constants.h"
#include "bionic/guest_types.h"
#include "bionic/memory.h"
#include "bionic/runtime.h"
#include "bionic/text/environment.h"
#include "bionic/threads.h"

namespace elixir::bionic {

ELIXIR_BIONIC_FN(char*, getenv)(const char* name) noexcept;
ELIXIR_BIONIC_FN(std::int32_t, pthread_gettid_np)(guest::PthreadT thread) noexcept;

namespace core {
namespace {

constexpr int kPosixSpawnResetIds = 1, kPosixSpawnSetPgroup = 2, kPosixSpawnSetSigDef = 4,
              kPosixSpawnSetSigMask = 8, kPosixSpawnSetSchedParam = 16, kPosixSpawnSetScheduler = 32,
              kPosixSpawnUseVfork = 64, kPosixSpawnSetSid = 128, kPosixSpawnCloexecDefault = 256;
constexpr int kPosixSpawnAllFlags = kPosixSpawnResetIds | kPosixSpawnSetPgroup | kPosixSpawnSetSigDef |
                                    kPosixSpawnSetSigMask | kPosixSpawnSetSchedParam | kPosixSpawnSetScheduler |
                                    kPosixSpawnUseVfork | kPosixSpawnSetSid | kPosixSpawnCloexecDefault;

constexpr int kWNohang = 1, kWUntraced = 2, kWExited = 4, kWContinued = 8;
constexpr int kWNowait = 0x01000000, kWNothread = 0x20000000, kWAll = 0x40000000;
constexpr int kWClone = static_cast<int>(0x80000000u);
constexpr int kPAll = 0, kPPid = 1, kPPgid = 2, kPPidfd = 3;

constexpr unsigned kPidfdNonblock = 0x800;
constexpr unsigned kPidfdThread = 0x80;
constexpr unsigned kPidfdSignalThread = 1, kPidfdSignalThreadGroup = 2, kPidfdSignalProcessGroup = 4;

constexpr std::uint32_t kPerLinux = 0;
constexpr std::uint64_t kPersonalityQuery = 0xffffffffu;

constexpr std::uint64_t kCloneFs = 0x200, kCloneFiles = 0x400, kCloneSighand = 0x800, kCloneThread = 0x10000,
                        kCloneVm = 0x100, kCloneNewns = 0x20000, kCloneSysvsem = 0x40000,
                        kCloneNewcgroup = 0x02000000, kCloneNewuts = 0x04000000, kCloneNewipc = 0x08000000,
                        kCloneNewuser = 0x10000000, kCloneNewpid = 0x20000000, kCloneNewnet = 0x40000000,
                        kCloneNewtime = 0x80;
constexpr std::uint64_t kUnshareFlags = kCloneThread | kCloneFs | kCloneNewns | kCloneSighand | kCloneVm |
                                        kCloneFiles | kCloneSysvsem | kCloneNewuts | kCloneNewipc |
                                        kCloneNewnet | kCloneNewuser | kCloneNewpid | kCloneNewcgroup |
                                        kCloneNewtime;
constexpr std::uint64_t kNamespaceFlags = kCloneNewns | kCloneNewuts | kCloneNewipc | kCloneNewnet |
                                          kCloneNewuser | kCloneNewpid | kCloneNewcgroup | kCloneNewtime;

constexpr char kDefaultPath[] =
    "/product/bin:/apex/com.android.runtime/bin:/apex/com.android.art/bin:/apex/com.android.virt/bin:"
    "/system_ext/bin:/system/bin:/system/xbin:/odm/bin:/vendor/bin:/vendor/xbin";

std::atomic<void*> g_stat{nullptr};
std::atomic<void*> g_fstat{nullptr};
std::atomic<void*> g_write{nullptr};
std::atomic<void*> g_fclose{nullptr};
std::atomic<void*> g_vsnprintf{nullptr};

enum SpawnAction : std::int32_t { kOpen, kClose, kDup2, kChdir, kFchdir };

struct SpawnFileAction {
  SpawnFileAction* next;
  SpawnAction what;
  std::int32_t fd;
  std::int32_t new_fd;
  char* path;
  std::int32_t flags;
  abi::g_mode mode;
};

struct SpawnFileActions {
  SpawnFileAction* head;
  SpawnFileAction* last;
};

struct SpawnAttr {
  std::int16_t flags;
  abi::g_pid pgroup;
  guest::SchedParam schedparam;
  std::int32_t schedpolicy;
  guest::Sigset sigmask;
  guest::Sigset sigdefault;
};
static_assert(sizeof(SpawnAttr) == 32, "bionic __posix_spawnattr on LP64");

using SpawnAttrT = SpawnAttr*;
using SpawnFileActionsT = SpawnFileActions*;

int AddFileAction(SpawnFileActionsT* actions, SpawnAction what, int fd, int new_fd, const char* path, int flags,
                  abi::g_mode mode) noexcept {
  if (actions == nullptr || *actions == nullptr) return guest::kEINVAL;
  auto* action = static_cast<SpawnFileAction*>(GuestMalloc(sizeof(SpawnFileAction)));
  if (action == nullptr) return guest::kENOMEM;
  action->next = nullptr;
  if (what == kOpen || what == kChdir) {
    if (path == nullptr) {
      GuestFree(action);
      return guest::kEFAULT;
    }
    action->path = GuestStrdup(path);
    if (action->path == nullptr) {
      GuestFree(action);
      return guest::kENOMEM;
    }
  } else {
    action->path = nullptr;
  }
  action->what = what;
  action->fd = fd;
  action->new_fd = new_fd;
  action->flags = flags;
  action->mode = mode;
  SpawnFileActions* list = *actions;
  if (list->head == nullptr) {
    list->head = list->last = action;
  } else {
    list->last->next = action;
    list->last = action;
  }
  return 0;
}

bool MayExecute(const guest::Stat& st) noexcept {
  const std::uint32_t mode = st.st_mode;
  if (st.st_uid == CurrentUid()) return (mode & guest::kS_IXUSR) != 0;
  bool in_group = st.st_gid == CurrentGid();
  if (!in_group) {
    try {
      for (const std::uint32_t gid : CurrentGuestIdentity().groups) in_group |= gid == st.st_gid;
    } catch (...) {
    }
  }
  return (mode & (in_group ? guest::kS_IXGRP : guest::kS_IXOTH)) != 0;
}

int ExecveErrorFor(const guest::Stat& st) noexcept {
  if ((st.st_mode & guest::kS_IFMT) != guest::kS_IFREG) return guest::kEACCES;
  if (!MayExecute(st)) return guest::kEACCES;
  return guest::kENOSYS;
}

int ExecveError(const char* path) noexcept {
  if (path == nullptr) return guest::kEFAULT;
  if (*path == '\0') return guest::kENOENT;
  const auto stat_fn = reinterpret_cast<abi::SysvFn<int(const char*, guest::Stat*)>>(g_stat.load());
  if (stat_fn == nullptr) return guest::kENOSYS;
  guest::Stat st{};
  const int saved = GuestErrno();
  if (stat_fn(path, &st) != 0) {
    const int error = GuestErrno();
    SetGuestErrno(saved);
    return error != 0 ? error : guest::kENOENT;
  }
  return ExecveErrorFor(st);
}

int Execve(const char* path) noexcept { return FailWith(ExecveError(path), -1); }

int Execvpe(const char* name) noexcept {
  if (name == nullptr || *name == '\0') return FailWith(guest::kENOENT, -1);
  if (std::strchr(name, '/') != nullptr) return Execve(name);
  const char* path = elixir_bionic_getenv("PATH");
  if (path == nullptr) path = kDefaultPath;
  try {
    const std::string search(path);
    const std::size_t name_length = std::strlen(name);
    bool saw_eacces = false;
    int last_error = guest::kENOENT;
    std::size_t start = 0;
    for (;;) {
      const std::size_t colon = search.find(':', start);
      std::string candidate = search.substr(start, colon == std::string::npos ? std::string::npos : colon - start);
      if (candidate.empty()) candidate = ".";
      candidate.push_back('/');
      candidate.append(name, name_length);
      last_error = ExecveError(candidate.c_str());
      switch (last_error) {
        case guest::kEISDIR:
        case guest::kELOOP:
        case guest::kENAMETOOLONG:
        case guest::kENOENT:
        case guest::kENOTDIR: break;
        case guest::kEACCES: saw_eacces = true; break;
        default: return FailWith(last_error, -1);
      }
      if (colon == std::string::npos) break;
      start = colon + 1;
    }
    return FailWith(saw_eacces ? guest::kEACCES : last_error, -1);
  } catch (...) {
    return FailWith(guest::kENOMEM, -1);
  }
}

bool ThreadOfThisProcess(std::int32_t tid) noexcept {
  if (tid <= 0) return false;
  if (tid == CurrentPid()) return true;
  HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threads::WindowsThreadIdFromTid(tid));
  if (thread == nullptr) return false;
  DWORD exit_code = 0;
  const bool ours = GetProcessIdOfThread(thread) == GetCurrentProcessId() && GetExitCodeThread(thread, &exit_code) &&
                    exit_code == STILL_ACTIVE;
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

int WaitError(abi::g_pid pid, int options) noexcept {
  if ((options & ~(kWNohang | kWUntraced | kWContinued | kWNothread | kWClone | kWAll)) != 0) return guest::kEINVAL;
  if (pid == INT32_MIN) return guest::kESRCH;
  return guest::kECHILD;
}

std::atomic<std::uint32_t> g_personality{kPerLinux};

}

void BindLateServices(const ExportTable& table) noexcept {
  try {
    g_stat.store(table.Find(kLibc, "stat"));
    g_fstat.store(table.Find(kLibc, "fstat"));
    g_write.store(table.Find(kLibc, "write"));
    g_fclose.store(table.Find(kLibc, "fclose"));
    g_vsnprintf.store(table.Find(kLibc, "vsnprintf"));
  } catch (...) {
  }
}

LateServices BoundLateServices() noexcept {
  LateServices services;
  services.stat = reinterpret_cast<decltype(services.stat)>(g_stat.load());
  services.fstat = reinterpret_cast<decltype(services.fstat)>(g_fstat.load());
  services.write = reinterpret_cast<decltype(services.write)>(g_write.load());
  services.fclose = reinterpret_cast<decltype(services.fclose)>(g_fclose.load());
  services.vsnprintf = reinterpret_cast<decltype(services.vsnprintf)>(g_vsnprintf.load());
  return services;
}

}

using core::SpawnAttrT;
using core::SpawnFileActionsT;

ELIXIR_BIONIC_FN(abi::g_pid, fork)() noexcept { return FailWith(guest::kENOSYS, abi::g_pid{-1}); }
ELIXIR_BIONIC_FN(abi::g_pid, vfork)() noexcept { return FailWith(guest::kENOSYS, abi::g_pid{-1}); }
ELIXIR_BIONIC_FN(abi::g_pid, _Fork)() noexcept { return FailWith(guest::kENOSYS, abi::g_pid{-1}); }

ELIXIR_BIONIC_FN(int, clone)(void* fn, void* child_stack, int flags, void* arg) noexcept {
  (void)flags;
  (void)arg;
  if (fn != nullptr && child_stack == nullptr) return FailWith(guest::kEINVAL, -1);
  return FailWith(guest::kENOSYS, -1);
}

ELIXIR_BIONIC_FN(int, execve)(const char* path, char* const* argv, char* const* envp) noexcept {
  (void)argv;
  (void)envp;
  return core::Execve(path);
}

ELIXIR_BIONIC_FN(int, execv)(const char* path, char* const* argv) noexcept {
  return elixir_bionic_execve(path, argv, *text::GuestEnvironLocation());
}

ELIXIR_BIONIC_FN(int, execvpe)(const char* file, char* const* argv, char* const* envp) noexcept {
  (void)argv;
  (void)envp;
  return core::Execvpe(file);
}

ELIXIR_BIONIC_FN(int, execvp)(const char* file, char* const* argv) noexcept {
  return elixir_bionic_execvpe(file, argv, *text::GuestEnvironLocation());
}

ELIXIR_BIONIC_FN(int, fexecve)(int fd, char* const* argv, char* const* envp) noexcept {
  (void)argv;
  (void)envp;
  const core::LateServices services = core::BoundLateServices();
  if (services.fstat == nullptr) return FailWith(guest::kENOSYS, -1);
  guest::Stat st{};
  const int saved = GuestErrno();
  if (services.fstat(fd, &st) != 0) {
    SetGuestErrno(saved);
    return FailWith(guest::kEBADF, -1);
  }
  return FailWith(core::ExecveErrorFor(st), -1);
}

namespace {
enum class ExeclVariant { kExecl, kExecle, kExeclp };

int ExeclCommon(ExeclVariant variant, const char* name, const char* arg0, abi::SysvVaList* args) noexcept {
  (void)arg0;
  abi::SysvVaCursor cursor(args);
  while (cursor.NextPointer<const char*>() != nullptr) {
  }
  char* const* envp = variant == ExeclVariant::kExecle ? cursor.NextPointer<char* const*>()
                                                        : *text::GuestEnvironLocation();
  if (variant == ExeclVariant::kExeclp) return elixir_bionic_execvpe(name, nullptr, envp);
  return elixir_bionic_execve(name, nullptr, envp);
}
}

extern "C" ELIXIR_SYSV ELIXIR_ASM_CALLED int elixir_bionic_execl_va(const char* name, const char* arg0,
                                                                   abi::SysvVaList* args) noexcept {
  return ExeclCommon(ExeclVariant::kExecl, name, arg0, args);
}
extern "C" ELIXIR_SYSV ELIXIR_ASM_CALLED int elixir_bionic_execle_va(const char* name, const char* arg0,
                                                                    abi::SysvVaList* args) noexcept {
  return ExeclCommon(ExeclVariant::kExecle, name, arg0, args);
}
extern "C" ELIXIR_SYSV ELIXIR_ASM_CALLED int elixir_bionic_execlp_va(const char* name, const char* arg0,
                                                                    abi::SysvVaList* args) noexcept {
  return ExeclCommon(ExeclVariant::kExeclp, name, arg0, args);
}
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_execl, elixir_bionic_execl_va, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_execle, elixir_bionic_execle_va, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_execlp, elixir_bionic_execlp_va, 2)

ELIXIR_BIONIC_FN(int, posix_spawn)(abi::g_pid* pid, const char* path, const SpawnFileActionsT* actions,
                                   const SpawnAttrT* attr, char* const* argv, char* const* envp) noexcept {
  (void)pid;
  (void)path;
  (void)actions;
  (void)attr;
  (void)argv;
  (void)envp;
  return guest::kENOSYS;
}

ELIXIR_BIONIC_FN(int, posix_spawnp)(abi::g_pid* pid, const char* file, const SpawnFileActionsT* actions,
                                    const SpawnAttrT* attr, char* const* argv, char* const* envp) noexcept {
  return elixir_bionic_posix_spawn(pid, file, actions, attr, argv, envp);
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_init)(SpawnAttrT* attr) noexcept {
  if (attr == nullptr) return guest::kEINVAL;
  *attr = static_cast<core::SpawnAttr*>(GuestCalloc(1, sizeof(core::SpawnAttr)));
  return *attr == nullptr ? guest::kENOMEM : 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_destroy)(SpawnAttrT* attr) noexcept {
  if (attr == nullptr) return guest::kEINVAL;
  GuestFree(*attr);
  *attr = nullptr;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_setflags)(SpawnAttrT* attr, std::int16_t flags) noexcept {
  if (attr == nullptr || *attr == nullptr) return guest::kEINVAL;
  if ((flags & ~core::kPosixSpawnAllFlags) != 0) return guest::kEINVAL;
  (*attr)->flags = flags;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_getflags)(const SpawnAttrT* attr, std::int16_t* flags) noexcept {
  if (attr == nullptr || *attr == nullptr || flags == nullptr) return guest::kEINVAL;
  *flags = (*attr)->flags;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_setpgroup)(SpawnAttrT* attr, abi::g_pid pgroup) noexcept {
  if (attr == nullptr || *attr == nullptr) return guest::kEINVAL;
  (*attr)->pgroup = pgroup;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_getpgroup)(const SpawnAttrT* attr, abi::g_pid* pgroup) noexcept {
  if (attr == nullptr || *attr == nullptr || pgroup == nullptr) return guest::kEINVAL;
  *pgroup = (*attr)->pgroup;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_setsigmask)(SpawnAttrT* attr, const guest::Sigset* mask) noexcept {
  if (attr == nullptr || *attr == nullptr || mask == nullptr) return guest::kEINVAL;
  (*attr)->sigmask = *mask;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_getsigmask)(const SpawnAttrT* attr, guest::Sigset* mask) noexcept {
  if (attr == nullptr || *attr == nullptr || mask == nullptr) return guest::kEINVAL;
  *mask = (*attr)->sigmask;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_setsigdefault)(SpawnAttrT* attr, const guest::Sigset* mask) noexcept {
  if (attr == nullptr || *attr == nullptr || mask == nullptr) return guest::kEINVAL;
  (*attr)->sigdefault = *mask;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_getsigdefault)(const SpawnAttrT* attr, guest::Sigset* mask) noexcept {
  if (attr == nullptr || *attr == nullptr || mask == nullptr) return guest::kEINVAL;
  *mask = (*attr)->sigdefault;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_setschedparam)(SpawnAttrT* attr, const guest::SchedParam* param) noexcept {
  if (attr == nullptr || *attr == nullptr || param == nullptr) return guest::kEINVAL;
  (*attr)->schedparam = *param;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_getschedparam)(const SpawnAttrT* attr, guest::SchedParam* param) noexcept {
  if (attr == nullptr || *attr == nullptr || param == nullptr) return guest::kEINVAL;
  *param = (*attr)->schedparam;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_setschedpolicy)(SpawnAttrT* attr, int policy) noexcept {
  if (attr == nullptr || *attr == nullptr) return guest::kEINVAL;
  (*attr)->schedpolicy = policy;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawnattr_getschedpolicy)(const SpawnAttrT* attr, int* policy) noexcept {
  if (attr == nullptr || *attr == nullptr || policy == nullptr) return guest::kEINVAL;
  *policy = (*attr)->schedpolicy;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawn_file_actions_init)(SpawnFileActionsT* actions) noexcept {
  if (actions == nullptr) return guest::kEINVAL;
  *actions = static_cast<core::SpawnFileActions*>(GuestCalloc(1, sizeof(core::SpawnFileActions)));
  return *actions == nullptr ? guest::kENOMEM : 0;
}

ELIXIR_BIONIC_FN(int, posix_spawn_file_actions_destroy)(SpawnFileActionsT* actions) noexcept {
  if (actions == nullptr) return guest::kEINVAL;
  if (*actions != nullptr) {
    core::SpawnFileAction* action = (*actions)->head;
    while (action != nullptr) {
      core::SpawnFileAction* done = action;
      action = action->next;
      GuestFree(done->path);
      GuestFree(done);
    }
  }
  GuestFree(*actions);
  *actions = nullptr;
  return 0;
}

ELIXIR_BIONIC_FN(int, posix_spawn_file_actions_addopen)(SpawnFileActionsT* actions, int fd, const char* path,
                                                        int flags, abi::g_mode mode) noexcept {
  if (fd < 0) return guest::kEBADF;
  return core::AddFileAction(actions, core::kOpen, -1, fd, path, flags, mode);
}

ELIXIR_BIONIC_FN(int, posix_spawn_file_actions_addclose)(SpawnFileActionsT* actions, int fd) noexcept {
  if (fd < 0) return guest::kEBADF;
  return core::AddFileAction(actions, core::kClose, fd, -1, nullptr, 0, 0);
}

ELIXIR_BIONIC_FN(int, posix_spawn_file_actions_adddup2)(SpawnFileActionsT* actions, int fd, int new_fd) noexcept {
  if (fd < 0 || new_fd < 0) return guest::kEBADF;
  return core::AddFileAction(actions, core::kDup2, fd, new_fd, nullptr, 0, 0);
}

ELIXIR_BIONIC_FN(int, posix_spawn_file_actions_addchdir_np)(SpawnFileActionsT* actions, const char* path) noexcept {
  return core::AddFileAction(actions, core::kChdir, -1, -1, path, 0, 0);
}

ELIXIR_BIONIC_FN(int, posix_spawn_file_actions_addfchdir_np)(SpawnFileActionsT* actions, int fd) noexcept {
  if (fd < 0) return guest::kEBADF;
  return core::AddFileAction(actions, core::kFchdir, fd, -1, nullptr, 0, 0);
}

ELIXIR_BIONIC_FN(abi::g_pid, wait4)(abi::g_pid pid, int* status, int options, guest::Rusage* usage) noexcept {
  (void)status;
  (void)usage;
  return FailWith(core::WaitError(pid, options), abi::g_pid{-1});
}

ELIXIR_BIONIC_FN(abi::g_pid, waitpid)(abi::g_pid pid, int* status, int options) noexcept {
  return elixir_bionic_wait4(pid, status, options, nullptr);
}

ELIXIR_BIONIC_FN(abi::g_pid, wait)(int* status) noexcept { return elixir_bionic_wait4(-1, status, 0, nullptr); }

ELIXIR_BIONIC_FN(int, waitid)(int idtype, std::uint32_t id, guest::Siginfo* info, int options) noexcept {
  (void)info;
  using namespace core;
  if ((options & ~(kWNohang | kWNowait | kWExited | kWUntraced | kWContinued | kWNothread | kWClone | kWAll)) != 0) {
    return FailWith(guest::kEINVAL, -1);
  }
  if ((options & (kWExited | kWUntraced | kWContinued)) == 0) return FailWith(guest::kEINVAL, -1);
  switch (idtype) {
    case kPAll: break;
    case kPPid:
      if (static_cast<std::int32_t>(id) <= 0) return FailWith(guest::kEINVAL, -1);
      break;
    case kPPgid:
      if (static_cast<std::int32_t>(id) < 0) return FailWith(guest::kEINVAL, -1);
      break;
    case kPPidfd:
      if (static_cast<std::int32_t>(id) < 0) return FailWith(guest::kEINVAL, -1);
      return FailWith(guest::kEBADF, -1);
    default: return FailWith(guest::kEINVAL, -1);
  }
  return FailWith(guest::kECHILD, -1);
}

ELIXIR_BIONIC_FN(void*, popen)(const char* command, const char* mode) noexcept {
  (void)command;
  if (mode == nullptr) return FailWith(guest::kEINVAL, static_cast<void*>(nullptr));
  return FailWith(guest::kENOSYS, static_cast<void*>(nullptr));
}

ELIXIR_BIONIC_FN(int, pclose)(void* stream) noexcept {
  const core::LateServices services = core::BoundLateServices();
  if (services.fclose == nullptr) return FailWith(guest::kENOSYS, -1);
  return services.fclose(stream);
}

ELIXIR_BIONIC_FN(int, system)(const char* command) noexcept {
  if (command == nullptr) return 0;
  return FailWith(guest::kENOSYS, -1);
}

ELIXIR_BIONIC_FN(int, daemon)(int nochdir, int noclose) noexcept {
  (void)nochdir;
  (void)noclose;
  return FailWith(guest::kENOSYS, -1);
}

ELIXIR_BIONIC_FN(int, pidfd_open)(abi::g_pid pid, unsigned flags) noexcept {
  using namespace core;
  if ((flags & ~(kPidfdNonblock | kPidfdThread)) != 0) return FailWith(guest::kEINVAL, -1);
  if (pid <= 0) return FailWith(guest::kEINVAL, -1);
  if (pid != CurrentPid()) {
    if (ThreadOfThisProcess(pid)) {
      if ((flags & kPidfdThread) == 0) return FailWith(guest::kEINVAL, -1);
    } else if (!OtherProcessExists(pid)) {
      return FailWith(guest::kESRCH, -1);
    }
  }
  return FailWith(guest::kENOSYS, -1);
}

ELIXIR_BIONIC_FN(int, pidfd_getfd)(int pidfd, int targetfd, unsigned flags) noexcept {
  (void)pidfd;
  (void)targetfd;
  if (flags != 0) return FailWith(guest::kEINVAL, -1);
  return FailWith(guest::kEBADF, -1);
}

ELIXIR_BIONIC_FN(int, pidfd_send_signal)(int pidfd, int sig, guest::Siginfo* info, unsigned flags) noexcept {
  (void)pidfd;
  (void)sig;
  (void)info;
  using namespace core;
  constexpr unsigned kAll = kPidfdSignalThread | kPidfdSignalThreadGroup | kPidfdSignalProcessGroup;
  if ((flags & ~kAll) != 0) return FailWith(guest::kEINVAL, -1);
  if ((flags & (flags - 1)) != 0) return FailWith(guest::kEINVAL, -1);
  return FailWith(guest::kEBADF, -1);
}

ELIXIR_BIONIC_FN(abi::g_long, ptrace)(int request) noexcept {
  (void)request;
  return FailWith(guest::kEPERM, abi::g_long{-1});
}

ELIXIR_BIONIC_FN(int, personality)(abi::g_ulong persona) noexcept {
  const auto value = static_cast<std::uint32_t>(persona);
  if (value == core::kPersonalityQuery) return static_cast<int>(core::g_personality.load());
  return static_cast<int>(core::g_personality.exchange(value));
}

ELIXIR_BIONIC_FN(int, unshare)(int flags) noexcept {
  using namespace core;
  std::uint64_t bits = static_cast<std::uint32_t>(flags);
  if ((bits & ~kUnshareFlags) != 0) return FailWith(guest::kEINVAL, -1);
  if ((bits & kCloneNewuser) != 0) bits |= kCloneThread | kCloneFs;
  if ((bits & kCloneVm) != 0) bits |= kCloneSighand;
  if ((bits & kCloneSighand) != 0) bits |= kCloneThread;
  if ((bits & kCloneNewns) != 0) bits |= kCloneFs;
  if ((bits & kCloneNewipc) != 0) bits |= kCloneSysvsem;
  if ((bits & (kCloneThread | kCloneSighand | kCloneVm)) != 0) return FailWith(guest::kEINVAL, -1);
  if ((bits & kNamespaceFlags) != 0) return FailWith(guest::kEPERM, -1);
  bits &= ~kCloneSysvsem;
  if (bits != 0) return FailWith(guest::kEPERM, -1);
  return 0;
}

ELIXIR_BIONIC_FN(int, setns)(int fd, int nstype) noexcept {
  (void)nstype;
  const core::LateServices services = core::BoundLateServices();
  if (services.fstat != nullptr) {
    guest::Stat st{};
    const int saved = GuestErrno();
    if (services.fstat(fd, &st) != 0) {
      SetGuestErrno(saved);
      return FailWith(guest::kEBADF, -1);
    }
  } else if (fd < 0) {
    return FailWith(guest::kEBADF, -1);
  }
  return FailWith(guest::kEINVAL, -1);
}

ELIXIR_BIONIC_FN(int, pthread_sigqueue)(guest::PthreadT thread, int sig, std::uint64_t value) noexcept {
  const int saved = GuestErrno();
  const std::int32_t tid = elixir_bionic_pthread_gettid_np(thread);
  SetGuestErrno(saved);
  if (tid == -1) return guest::kESRCH;
  if (tid == 0) return guest::kEINVAL;
  return core::QueueGuestSignalToThread(tid, sig, value);
}

namespace core {

void RegisterProcessExtExports(ExportTable& table) {
  BindLateServices(table);
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(fork);
  ELIXIR_ADD(vfork);
  ELIXIR_ADD(_Fork);
  ELIXIR_ADD(clone);
  ELIXIR_ADD(execve);
  ELIXIR_ADD(execv);
  ELIXIR_ADD(execvp);
  ELIXIR_ADD(execvpe);
  ELIXIR_ADD(fexecve);
  ELIXIR_ADD(execl);
  ELIXIR_ADD(execle);
  ELIXIR_ADD(execlp);
  ELIXIR_ADD(posix_spawn);
  ELIXIR_ADD(posix_spawnp);
  ELIXIR_ADD(posix_spawnattr_init);
  ELIXIR_ADD(posix_spawnattr_destroy);
  ELIXIR_ADD(posix_spawnattr_setflags);
  ELIXIR_ADD(posix_spawnattr_getflags);
  ELIXIR_ADD(posix_spawnattr_setpgroup);
  ELIXIR_ADD(posix_spawnattr_getpgroup);
  ELIXIR_ADD(posix_spawnattr_setsigmask);
  ELIXIR_ADD(posix_spawnattr_getsigmask);
  ELIXIR_ADD(posix_spawnattr_setsigdefault);
  ELIXIR_ADD(posix_spawnattr_getsigdefault);
  table.Add(kLibc, "posix_spawnattr_setsigmask64", reinterpret_cast<void*>(&elixir_bionic_posix_spawnattr_setsigmask));
  table.Add(kLibc, "posix_spawnattr_getsigmask64", reinterpret_cast<void*>(&elixir_bionic_posix_spawnattr_getsigmask));
  table.Add(kLibc, "posix_spawnattr_setsigdefault64",
            reinterpret_cast<void*>(&elixir_bionic_posix_spawnattr_setsigdefault));
  table.Add(kLibc, "posix_spawnattr_getsigdefault64",
            reinterpret_cast<void*>(&elixir_bionic_posix_spawnattr_getsigdefault));
  ELIXIR_ADD(posix_spawnattr_setschedparam);
  ELIXIR_ADD(posix_spawnattr_getschedparam);
  ELIXIR_ADD(posix_spawnattr_setschedpolicy);
  ELIXIR_ADD(posix_spawnattr_getschedpolicy);
  ELIXIR_ADD(posix_spawn_file_actions_init);
  ELIXIR_ADD(posix_spawn_file_actions_destroy);
  ELIXIR_ADD(posix_spawn_file_actions_addopen);
  ELIXIR_ADD(posix_spawn_file_actions_addclose);
  ELIXIR_ADD(posix_spawn_file_actions_adddup2);
  ELIXIR_ADD(posix_spawn_file_actions_addchdir_np);
  ELIXIR_ADD(posix_spawn_file_actions_addfchdir_np);
  ELIXIR_ADD(wait);
  ELIXIR_ADD(waitpid);
  ELIXIR_ADD(wait4);
  ELIXIR_ADD(waitid);
  ELIXIR_ADD(popen);
  ELIXIR_ADD(pclose);
  ELIXIR_ADD(system);
  ELIXIR_ADD(daemon);
  ELIXIR_ADD(pidfd_open);
  ELIXIR_ADD(pidfd_getfd);
  ELIXIR_ADD(pidfd_send_signal);
  ELIXIR_ADD(ptrace);
  ELIXIR_ADD(personality);
  ELIXIR_ADD(unshare);
  ELIXIR_ADD(setns);
  ELIXIR_ADD(pthread_sigqueue);
#undef ELIXIR_ADD
}

}
}
