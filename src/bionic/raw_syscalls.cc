// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/raw_syscalls.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include "bionic/runtime.h"
#include "platform/windows/raw_syscall_trap.h"

namespace elixir::bionic {
namespace {

using LibcFunction = ELIXIR_SYSV std::int64_t (*)(std::uint64_t, ...);
using LibcSyscall = ELIXIR_SYSV std::int64_t (*)(std::int64_t, std::int64_t, std::int64_t, std::int64_t,
                                                 std::int64_t, std::int64_t, std::int64_t);
using LibcGetcwd = ELIXIR_SYSV char* (*)(char*, std::uint64_t);

enum class Width : std::uint8_t { kInt, kLong };

struct Call {
  std::uint16_t number;
  const char* name;
  const char* function;
  std::uint8_t arguments;
  Width width;
  std::int8_t path;
};

constexpr Call kCalls[] = {
    {0, "read", "read", 3, Width::kLong, -1},
    {1, "write", "write", 3, Width::kLong, -1},
    {2, "open", "open", 3, Width::kInt, 0},
    {3, "close", "close", 1, Width::kInt, -1},
    {4, "stat", "stat", 2, Width::kInt, 0},
    {5, "fstat", "fstat", 2, Width::kInt, -1},
    {6, "lstat", "lstat", 2, Width::kInt, 0},
    {8, "lseek", "lseek", 3, Width::kLong, -1},
    {9, "mmap", "mmap", 6, Width::kLong, -1},
    {10, "mprotect", "mprotect", 3, Width::kInt, -1},
    {11, "munmap", "munmap", 2, Width::kInt, -1},
    {16, "ioctl", "ioctl", 3, Width::kInt, -1},
    {17, "pread64", "pread64", 4, Width::kLong, -1},
    {18, "pwrite64", "pwrite64", 4, Width::kLong, -1},
    {19, "readv", "readv", 3, Width::kLong, -1},
    {20, "writev", "writev", 3, Width::kLong, -1},
    {21, "access", "access", 2, Width::kInt, 0},
    {22, "pipe", "pipe", 1, Width::kInt, -1},
    {28, "madvise", "madvise", 3, Width::kInt, -1},
    {32, "dup", "dup", 1, Width::kInt, -1},
    {33, "dup2", "dup2", 2, Width::kInt, -1},
    {72, "fcntl", "fcntl", 3, Width::kInt, -1},
    {74, "fsync", "fsync", 1, Width::kInt, -1},
    {75, "fdatasync", "fdatasync", 1, Width::kInt, -1},
    {77, "ftruncate", "ftruncate", 2, Width::kInt, -1},
    {82, "rename", "rename", 2, Width::kInt, 0},
    {83, "mkdir", "mkdir", 2, Width::kInt, 0},
    {84, "rmdir", "rmdir", 1, Width::kInt, 0},
    {87, "unlink", "unlink", 1, Width::kInt, 0},
    {89, "readlink", "readlink", 3, Width::kLong, 0},
    {137, "statfs", "statfs", 2, Width::kInt, 0},
    {138, "fstatfs", "fstatfs", 2, Width::kInt, -1},
    {257, "openat", "openat", 4, Width::kInt, 1},
    {258, "mkdirat", "mkdirat", 3, Width::kInt, 1},
    {262, "newfstatat", "fstatat", 4, Width::kInt, 1},
    {263, "unlinkat", "unlinkat", 3, Width::kInt, 1},
    {267, "readlinkat", "readlinkat", 4, Width::kLong, 1},
    {269, "faccessat", "faccessat", 3, Width::kInt, 1},
    {292, "dup3", "dup3", 3, Width::kInt, -1},
    {293, "pipe2", "pipe2", 2, Width::kInt, -1},
    {332, "statx", "statx", 5, Width::kInt, 1},
    {439, "faccessat2", "faccessat", 4, Width::kInt, 1},
};
constexpr std::size_t kCallCount = sizeof(kCalls) / sizeof(kCalls[0]);

struct Name {
  std::uint16_t number;
  const char* name;
};
constexpr Name kOtherNames[] = {
    {13, "rt_sigaction"}, {14, "rt_sigprocmask"}, {15, "rt_sigreturn"}, {24, "sched_yield"}, {35, "nanosleep"},
    {39, "getpid"}, {56, "clone"}, {57, "fork"}, {59, "execve"}, {60, "exit"}, {61, "wait4"}, {62, "kill"},
    {63, "uname"}, {79, "getcwd"}, {96, "gettimeofday"}, {97, "getrlimit"}, {98, "getrusage"}, {99, "sysinfo"},
    {101, "ptrace"}, {102, "getuid"}, {104, "getgid"}, {107, "geteuid"}, {108, "getegid"}, {110, "getppid"},
    {127, "rt_sigpending"}, {130, "rt_sigsuspend"}, {131, "sigaltstack"}, {140, "getpriority"},
    {141, "setpriority"}, {157, "prctl"}, {158, "arch_prctl"}, {160, "setrlimit"}, {186, "gettid"},
    {200, "tkill"}, {201, "time"}, {202, "futex"}, {204, "sched_getaffinity"}, {217, "getdents64"},
    {227, "clock_settime"}, {228, "clock_gettime"}, {229, "clock_getres"}, {230, "clock_nanosleep"},
    {231, "exit_group"}, {234, "tgkill"}, {302, "prlimit64"}, {309, "getcpu"}, {318, "getrandom"},
    {324, "membarrier"},
};
constexpr std::uint64_t kGetcwd = 79;

std::atomic<LibcFunction> g_functions[kCallCount];
std::atomic<LibcSyscall> g_syscall{nullptr};
std::atomic<LibcGetcwd> g_getcwd{nullptr};

const Call* FindCall(std::uint64_t number, std::size_t* index) noexcept {
  for (std::size_t i = 0; i < kCallCount; ++i) {
    if (kCalls[i].number == number) {
      *index = i;
      return &kCalls[i];
    }
  }
  return nullptr;
}

constexpr std::uint64_t kLoggedCalls = 48;
std::atomic<std::uint64_t> g_calls{0};
std::mutex g_counts_lock;
std::map<std::uint64_t, std::uint64_t>* g_answered = nullptr;
std::map<std::uint64_t, std::uint64_t>* g_unimplemented = nullptr;

bool ReadGuestString(std::uint64_t address, char* out, std::size_t capacity) noexcept {
  if (address == 0 || capacity == 0) return false;
  std::size_t length = 0;
  while (length + 1 < capacity) {
    char byte = 0;
    SIZE_T copied = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address + length), &byte, 1, &copied) ||
        copied != 1) {
      return false;
    }
    if (byte == '\0') break;
    out[length++] = (byte >= 0x20 && byte < 0x7f) ? byte : '?';
  }
  out[length] = '\0';
  return true;
}

void Record(const platform::RawSyscallRequest& request, std::int64_t result) noexcept {
  constexpr std::int64_t kNoSuchCall = -38;
  const std::uint64_t ordinal = g_calls.fetch_add(1, std::memory_order_relaxed);
  try {
    std::lock_guard<std::mutex> lock(g_counts_lock);
    if (g_answered == nullptr) g_answered = new std::map<std::uint64_t, std::uint64_t>();
    if (g_unimplemented == nullptr) g_unimplemented = new std::map<std::uint64_t, std::uint64_t>();
    const bool first_unimplemented = result == kNoSuchCall && g_unimplemented->count(request.number) == 0;
    ++(*(result == kNoSuchCall ? g_unimplemented : g_answered))[request.number];
    if (ordinal >= kLoggedCalls && !first_unimplemented) return;
  } catch (...) {
    return;
  }
  const DWORD saved_error = GetLastError();
  const char* const name = RawSyscallName(request.number);
  char path[200] = {};
  std::size_t index = 0;
  const Call* const call = FindCall(request.number, &index);
  const bool has_path =
      call != nullptr && call->path >= 0 && ReadGuestString(request.args[call->path], path, sizeof(path));
  const char* image = request.image != nullptr ? request.image : "";
  if (const char* slash = std::strrchr(image, '/')) image = slash + 1;
  std::fprintf(stderr, "[elixir][raw-syscall] %s%s%s%s number=%llu result=%lld at=%s+0x%llx%s\n",
               name != nullptr ? name : "syscall", has_path ? "(\"" : "", has_path ? path : "",
               has_path ? "\")" : "", static_cast<unsigned long long>(request.number),
               static_cast<long long>(result), image,
               static_cast<unsigned long long>(request.site - request.image_base),
               result == kNoSuchCall ? " (not implemented)" : "");
  SetLastError(saved_error);
}

std::int64_t Handle(const platform::RawSyscallRequest& request) {
  const std::int64_t result = RawSyscall(request.number, request.args);
  Record(request, result);
  return result;
}

}

const char* RawSyscallName(std::uint64_t number) noexcept {
  std::size_t index = 0;
  if (const Call* call = FindCall(number, &index)) return call->name;
  for (const Name& other : kOtherNames) {
    if (other.number == number) return other.name;
  }
  return nullptr;
}

std::int64_t RawSyscall(std::uint64_t number, const std::uint64_t arguments[6]) noexcept {
  int* const error = GuestErrnoLocation();
  const int saved = *error;
  *error = 0;
  std::int64_t result = -1;
  std::size_t index = 0;
  const Call* const call = FindCall(number, &index);
  const LibcFunction function = call != nullptr ? g_functions[index].load(std::memory_order_acquire) : nullptr;
  if (function != nullptr) {
    std::uint64_t a[6] = {};
    for (std::size_t i = 0; i < call->arguments; ++i) a[i] = arguments[i];
    result = function(a[0], a[1], a[2], a[3], a[4], a[5]);
    if (call->width == Width::kInt) result = static_cast<std::int32_t>(result);
  } else if (number == kGetcwd && g_getcwd.load(std::memory_order_acquire) != nullptr) {
    char* const buffer = reinterpret_cast<char*>(arguments[0]);
    if (g_getcwd.load(std::memory_order_acquire)(buffer, arguments[1]) != nullptr) {
      result = static_cast<std::int64_t>(std::strlen(buffer)) + 1;
    }
  } else if (const LibcSyscall libc_syscall = g_syscall.load(std::memory_order_acquire)) {
    result = libc_syscall(static_cast<std::int64_t>(number), static_cast<std::int64_t>(arguments[0]),
                          static_cast<std::int64_t>(arguments[1]), static_cast<std::int64_t>(arguments[2]),
                          static_cast<std::int64_t>(arguments[3]), static_cast<std::int64_t>(arguments[4]),
                          static_cast<std::int64_t>(arguments[5]));
  } else {
    *error = guest::kENOSYS;
  }
  if (result == -1 && *error != 0) result = -static_cast<std::int64_t>(*error);
  *error = saved;
  return result;
}

void InstallRawSyscalls(const ExportTable& table) {
  for (std::size_t i = 0; i < kCallCount; ++i) {
    g_functions[i].store(reinterpret_cast<LibcFunction>(table.Find(kLibc, kCalls[i].function)),
                         std::memory_order_release);
  }
  g_syscall.store(reinterpret_cast<LibcSyscall>(table.Find(kLibc, "syscall")), std::memory_order_release);
  g_getcwd.store(reinterpret_cast<LibcGetcwd>(table.Find(kLibc, "getcwd")), std::memory_order_release);
  platform::SetRawSyscallHandler(&Handle);
}

std::string RawSyscallSummary() {
  std::vector<std::pair<std::uint64_t, std::uint64_t>> answered, unimplemented;
  {
    std::lock_guard<std::mutex> lock(g_counts_lock);
    if (g_answered != nullptr) {
      for (const auto& [number, count] : *g_answered) answered.emplace_back(count, number);
    }
    if (g_unimplemented != nullptr) {
      for (const auto& [number, count] : *g_unimplemented) unimplemented.emplace_back(count, number);
    }
  }
  if (answered.empty() && unimplemented.empty()) return {};
  const auto list = [](std::vector<std::pair<std::uint64_t, std::uint64_t>>& calls) {
    std::sort(calls.begin(), calls.end(), [](const auto& a, const auto& b) {
      return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    std::string text;
    std::uint64_t total = 0;
    for (const auto& [count, number] : calls) {
      total += count;
      if (!text.empty()) text += ", ";
      const char* const name = RawSyscallName(number);
      text += name != nullptr ? std::string(name) : "number " + std::to_string(number);
      text += " " + std::to_string(count);
    }
    return std::to_string(total) + " (" + text + ")";
  };
  std::string line = "direct system calls: ";
  line += answered.empty() ? std::string("0 answered") : list(answered) + " answered";
  if (!unimplemented.empty()) line += "; not implemented: " + list(unimplemented);
  return line;
}

}
