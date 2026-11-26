// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_posix_compat.h"

#include <windows.h>

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>

#include <map>
#include <mutex>
#include <new>
#include <string>

namespace {

constexpr std::size_t kPageSize = 4096;

const int g_binary_mode_installed = [] {
  _set_fmode(_O_BINARY);
  return 1;
}();

std::uintptr_t PageDown(std::uintptr_t value) { return value & ~(kPageSize - 1); }
std::uintptr_t PageUp(std::uintptr_t value) { return (value + kPageSize - 1) & ~(kPageSize - 1); }

DWORD ProtectionFor(int prot) {
  const bool read = (prot & (PROT_READ | PROT_WRITE | PROT_EXEC)) != 0;
  const bool write = (prot & PROT_WRITE) != 0;
  const bool exec = (prot & PROT_EXEC) != 0;
  if (!read) return PAGE_NOACCESS;
  if (exec) return write ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
  return write ? PAGE_READWRITE : PAGE_READONLY;
}

void SetErrnoFromWin32(DWORD error) {
  switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_NAME:
      errno = ENOENT;
      break;
    case ERROR_ACCESS_DENIED:
      errno = EACCES;
      break;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
    case ERROR_COMMITMENT_LIMIT:
      errno = ENOMEM;
      break;
    case ERROR_INVALID_ADDRESS:
    case ERROR_INVALID_PARAMETER:
      errno = EINVAL;
      break;
    case ERROR_HANDLE_EOF:
      errno = 0;
      break;
    default:
      errno = EIO;
      break;
  }
}

struct Reservation {
  std::uintptr_t base = 0;
  std::size_t size = 0;
  std::uintptr_t live_begin = 0;
  std::uintptr_t live_end = 0;
};

std::mutex& MappingLock() {
  static std::mutex lock;
  return lock;
}
std::map<std::uintptr_t, Reservation>& Reservations() {
  static auto* reservations = new std::map<std::uintptr_t, Reservation>();
  return *reservations;
}

Reservation* FindReservation(std::uintptr_t address) {
  auto& reservations = Reservations();
  auto it = reservations.upper_bound(address);
  if (it == reservations.begin()) return nullptr;
  --it;
  Reservation& reservation = it->second;
  return address < reservation.base + reservation.size ? &reservation : nullptr;
}

bool ReadExact(int fd, void* destination, std::size_t length, off64_t offset) {
  auto* out = static_cast<char*>(destination);
  while (length > 0) {
    const ssize_t got = pread(fd, out, length, offset);
    if (got == 0) return true;
    if (got < 0) {
      return false;
    }
    out += got;
    offset += got;
    length -= static_cast<std::size_t>(got);
  }
  return true;
}

bool CommitZeroed(std::uintptr_t start, std::size_t length) {
  VirtualFree(reinterpret_cast<void*>(start), length, MEM_DECOMMIT);
  return VirtualAlloc(reinterpret_cast<void*>(start), length, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

std::wstring Widen(const char* text) {
  if (text == nullptr) return {};
  const int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (needed <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(needed - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), needed);
  return wide;
}

std::string Narrow(const wchar_t* text, int length = -1) {
  const int needed = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
  if (needed <= 0) return {};
  std::string narrow(static_cast<std::size_t>(needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, length, narrow.data(), needed, nullptr, nullptr);
  if (length < 0 && !narrow.empty()) narrow.pop_back();
  return narrow;
}

std::string NormalizeFinalPath(const std::wstring& final_path) {
  std::wstring path = final_path;
  if (path.rfind(L"\\\\?\\UNC\\", 0) == 0) {
    path = L"\\\\" + path.substr(8);
  } else if (path.rfind(L"\\\\?\\", 0) == 0) {
    path = path.substr(4);
  }
  std::string narrow = Narrow(path.c_str());
  for (char& c : narrow) {
    if (c == '\\') c = '/';
  }
  return narrow;
}

}

extern "C" {

void* mmap(void* address, size_t length, int prot, int flags, int fd, off64_t offset) {
  if (length == 0 || (offset & static_cast<off64_t>(kPageSize - 1)) != 0) {
    errno = EINVAL;
    return MAP_FAILED;
  }
  if ((flags & MAP_SHARED) != 0 && (flags & MAP_ANONYMOUS) == 0) {
    errno = ENOTSUP;
    return MAP_FAILED;
  }
  const bool anonymous = (flags & MAP_ANONYMOUS) != 0;
  const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(address);
  const std::size_t span = PageUp(length);
  std::lock_guard<std::mutex> lock(MappingLock());

  if ((flags & MAP_FIXED) != 0) {
    if (start != PageDown(start)) {
      errno = EINVAL;
      return MAP_FAILED;
    }
    Reservation* reservation = FindReservation(start);
    if (reservation == nullptr || start + span > reservation->base + reservation->size) {
      void* placed = VirtualAlloc(address, span, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (placed == nullptr) {
        SetErrnoFromWin32(GetLastError());
        return MAP_FAILED;
      }
      Reservation fresh{start, span, start, start + span};
      Reservations()[start] = fresh;
    } else if (!CommitZeroed(start, span)) {
      SetErrnoFromWin32(GetLastError());
      return MAP_FAILED;
    }
    if (prot == PROT_NONE) {
      VirtualFree(address, span, MEM_DECOMMIT);
      return address;
    }
    if (!anonymous && !ReadExact(fd, address, span, offset)) return MAP_FAILED;
    DWORD previous = 0;
    VirtualProtect(address, span, ProtectionFor(prot), &previous);
    if ((prot & PROT_EXEC) != 0) FlushInstructionCache(GetCurrentProcess(), address, span);
    return address;
  }

  const bool reserve_only = anonymous && prot == PROT_NONE;
  void* placed = VirtualAlloc(nullptr, span, reserve_only ? MEM_RESERVE : (MEM_RESERVE | MEM_COMMIT),
                              reserve_only ? PAGE_NOACCESS : PAGE_READWRITE);
  if (placed == nullptr) {
    SetErrnoFromWin32(GetLastError());
    return MAP_FAILED;
  }
  const auto base = reinterpret_cast<std::uintptr_t>(placed);
  Reservations()[base] = Reservation{base, span, base, base + span};
  if (!reserve_only) {
    if (!anonymous && !ReadExact(fd, placed, span, offset)) {
      VirtualFree(placed, 0, MEM_RELEASE);
      Reservations().erase(base);
      return MAP_FAILED;
    }
    if (prot != (PROT_READ | PROT_WRITE)) {
      DWORD previous = 0;
      VirtualProtect(placed, span, ProtectionFor(prot), &previous);
    }
    if ((prot & PROT_EXEC) != 0) FlushInstructionCache(GetCurrentProcess(), placed, span);
  }
  return placed;
}

void* mmap64(void* address, size_t length, int prot, int flags, int fd, off64_t offset) {
  return mmap(address, length, prot, flags, fd, offset);
}

int munmap(void* address, size_t length) {
  const auto start = reinterpret_cast<std::uintptr_t>(address);
  if (start != PageDown(start) || length == 0) {
    errno = EINVAL;
    return -1;
  }
  const std::uintptr_t end = start + PageUp(length);
  std::lock_guard<std::mutex> lock(MappingLock());
  Reservation* reservation = FindReservation(start);
  if (reservation == nullptr) {
    errno = EINVAL;
    return -1;
  }
  if (start <= reservation->live_begin && end >= reservation->live_end) {
    VirtualFree(reinterpret_cast<void*>(reservation->base), 0, MEM_RELEASE);
    Reservations().erase(reservation->base);
    return 0;
  }
  VirtualFree(address, end - start, MEM_DECOMMIT);
  if (start <= reservation->live_begin) reservation->live_begin = end;
  if (end >= reservation->live_end) reservation->live_end = start;
  return 0;
}

int mprotect(void* address, size_t length, int prot) {
  auto cursor = PageDown(reinterpret_cast<std::uintptr_t>(address));
  const std::uintptr_t end = PageUp(reinterpret_cast<std::uintptr_t>(address) + length);
  const DWORD protection = ProtectionFor(prot);
  while (cursor < end) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) == 0) {
      errno = ENOMEM;
      return -1;
    }
    const std::uintptr_t region_end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    const std::uintptr_t chunk_end = region_end < end ? region_end : end;
    if (info.State != MEM_COMMIT) {
      errno = ENOMEM;
      return -1;
    }
    DWORD previous = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(cursor), chunk_end - cursor, protection, &previous)) {
      SetErrnoFromWin32(GetLastError());
      return -1;
    }
    cursor = chunk_end;
  }
  if ((prot & PROT_EXEC) != 0) {
    FlushInstructionCache(GetCurrentProcess(), address, length);
  }
  return 0;
}

int madvise(void* address, size_t length, int advice) {
  if (advice == MADV_DONTNEED) {
    const auto start = PageDown(reinterpret_cast<std::uintptr_t>(address));
    const std::uintptr_t end = PageUp(reinterpret_cast<std::uintptr_t>(address) + length);
    MEMORY_BASIC_INFORMATION info{};
    VirtualQuery(reinterpret_cast<void*>(start), &info, sizeof(info));
    if (!CommitZeroed(start, end - start)) return -1;
    if (info.Protect != 0 && info.Protect != PAGE_READWRITE) {
      DWORD previous = 0;
      VirtualProtect(reinterpret_cast<void*>(start), end - start, info.Protect, &previous);
    }
  }
  return 0;
}

ssize_t pread(int fd, void* buffer, size_t count, off64_t offset) {
  const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (handle == INVALID_HANDLE_VALUE) {
    errno = EBADF;
    return -1;
  }
  OVERLAPPED overlapped{};
  overlapped.Offset = static_cast<DWORD>(offset & 0xffffffffu);
  overlapped.OffsetHigh = static_cast<DWORD>(static_cast<std::uint64_t>(offset) >> 32);
  const DWORD request = count > 0x7fffffffu ? 0x7fffffffu : static_cast<DWORD>(count);
  DWORD transferred = 0;
  if (!ReadFile(handle, buffer, request, &transferred, &overlapped)) {
    const DWORD error = GetLastError();
    if (error == ERROR_HANDLE_EOF) return 0;
    SetErrnoFromWin32(error);
    return -1;
  }
  return static_cast<ssize_t>(transferred);
}

ssize_t pread64(int fd, void* buffer, size_t count, off64_t offset) { return pread(fd, buffer, count, offset); }

ssize_t pwrite(int fd, const void* buffer, size_t count, off64_t offset) {
  const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (handle == INVALID_HANDLE_VALUE) {
    errno = EBADF;
    return -1;
  }
  OVERLAPPED overlapped{};
  overlapped.Offset = static_cast<DWORD>(offset & 0xffffffffu);
  overlapped.OffsetHigh = static_cast<DWORD>(static_cast<std::uint64_t>(offset) >> 32);
  const DWORD request = count > 0x7fffffffu ? 0x7fffffffu : static_cast<DWORD>(count);
  DWORD transferred = 0;
  if (!WriteFile(handle, buffer, request, &transferred, &overlapped)) {
    SetErrnoFromWin32(GetLastError());
    return -1;
  }
  return static_cast<ssize_t>(transferred);
}

ssize_t pwrite64(int fd, const void* buffer, size_t count, off64_t offset) {
  return pwrite(fd, buffer, count, offset);
}

off64_t lseek64(int fd, off64_t offset, int whence) { return _lseeki64(fd, offset, whence); }

int elixir_posix_fd_path(int fd, char* buffer, size_t size) {
  const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (handle == INVALID_HANDLE_VALUE) {
    errno = EBADF;
    return -1;
  }
  wchar_t wide[32768];
  const DWORD length = GetFinalPathNameByHandleW(handle, wide, 32768, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (length == 0 || length >= 32768) {
    SetErrnoFromWin32(GetLastError());
    return -1;
  }
  const std::string path = NormalizeFinalPath(std::wstring(wide, length));
  if (path.size() + 1 > size) {
    errno = ENAMETOOLONG;
    return -1;
  }
  std::memcpy(buffer, path.c_str(), path.size() + 1);
  return static_cast<int>(path.size());
}

ssize_t readlink(const char* path, char* buffer, size_t size) {
  if (path != nullptr && std::strcmp(path, "/proc/self/exe") == 0) {
    wchar_t wide[32768];
    const DWORD length = GetModuleFileNameW(nullptr, wide, 32768);
    if (length == 0 || length >= 32768) {
      errno = ENAMETOOLONG;
      return -1;
    }
    std::string narrow = NormalizeFinalPath(std::wstring(wide, length));
    const std::size_t copy = narrow.size() < size ? narrow.size() : size;
    std::memcpy(buffer, narrow.data(), copy);
    return static_cast<ssize_t>(copy);
  }
  errno = EINVAL;
  return -1;
}

char* realpath(const char* path, char* resolved) {
  if (path == nullptr) {
    errno = EINVAL;
    return nullptr;
  }
  const std::wstring wide = Widen(path);
  const HANDLE handle = CreateFileW(wide.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    SetErrnoFromWin32(GetLastError());
    return nullptr;
  }
  wchar_t final_path[32768];
  const DWORD length = GetFinalPathNameByHandleW(handle, final_path, 32768, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  CloseHandle(handle);
  if (length == 0 || length >= 32768) {
    SetErrnoFromWin32(GetLastError());
    return nullptr;
  }
  const std::string normalized = NormalizeFinalPath(std::wstring(final_path, length));
  if (resolved == nullptr) return _strdup(normalized.c_str());
  if (normalized.size() >= 4096) {
    errno = ENAMETOOLONG;
    return nullptr;
  }
  std::memcpy(resolved, normalized.c_str(), normalized.size() + 1);
  return resolved;
}

int fsync(int fd) { return _commit(fd); }

int ftruncate(int fd, off64_t length) {
  const errno_t error = _chsize_s(fd, length);
  if (error != 0) {
    errno = error;
    return -1;
  }
  return 0;
}

long sysconf(int name) {
  switch (name) {
    case _SC_PAGESIZE:
      return static_cast<long>(kPageSize);
    case _SC_NPROCESSORS_CONF:
    case _SC_NPROCESSORS_ONLN:
      return static_cast<long>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    case _SC_CLK_TCK:
      return 100;
    case _SC_PHYS_PAGES: {
      MEMORYSTATUSEX status{sizeof(status)};
      GlobalMemoryStatusEx(&status);
      return static_cast<long>(status.ullTotalPhys / kPageSize);
    }
    default:
      errno = EINVAL;
      return -1;
  }
}

int getpagesize(void) { return static_cast<int>(kPageSize); }
pid_t gettid(void) { return static_cast<pid_t>(GetCurrentThreadId()); }

int usleep(useconds_t microseconds) {
  Sleep((microseconds + 999) / 1000);
  return 0;
}

unsigned int sleep(unsigned int seconds) {
  Sleep(seconds * 1000);
  return 0;
}

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }

int strcasecmp(const char* left, const char* right) { return _stricmp(left, right); }
int strncasecmp(const char* left, const char* right, size_t count) { return _strnicmp(left, right, count); }

char* basename(const char* path) {
  if (path == nullptr || *path == '\0') return const_cast<char*>(".");
  const char* last = path;
  for (const char* p = path; *p != '\0'; ++p) {
    if ((*p == '/' || *p == '\\') && p[1] != '\0') last = p + 1;
  }
  return const_cast<char*>(last);
}

char* dirname(char* path) {
  if (path == nullptr || *path == '\0') return const_cast<char*>(".");
  char* last = nullptr;
  for (char* p = path; *p != '\0'; ++p) {
    if (*p == '/' || *p == '\\') last = p;
  }
  if (last == nullptr) return const_cast<char*>(".");
  if (last == path) {
    path[1] = '\0';
  } else {
    *last = '\0';
  }
  return path;
}

ssize_t writev(int fd, const struct iovec* vector, int count) {
  ssize_t total = 0;
  for (int i = 0; i < count; ++i) {
    if (vector[i].iov_len == 0) continue;
    const int written = _write(fd, vector[i].iov_base, static_cast<unsigned>(vector[i].iov_len));
    if (written < 0) return total > 0 ? total : -1;
    total += written;
  }
  return total;
}

char* stpcpy(char* destination, const char* source) {
  const std::size_t length = std::strlen(source);
  std::memcpy(destination, source, length + 1);
  return destination + length;
}

int gettimeofday(struct timeval* tv, struct timezone*) {
  FILETIME now;
  GetSystemTimePreciseAsFileTime(&now);
  const std::uint64_t ticks = (static_cast<std::uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
  const std::uint64_t unix_us = (ticks - 116444736000000000ull) / 10;
  tv->tv_sec = static_cast<long>(unix_us / 1000000);
  tv->tv_usec = static_cast<long>(unix_us % 1000000);
  return 0;
}

struct elixir_posix_dir {
  HANDLE find = INVALID_HANDLE_VALUE;
  std::wstring pattern;
  WIN32_FIND_DATAW data{};
  bool has_pending = false;
  struct dirent entry{};
};

DIR* opendir(const char* path) {
  auto* dir = new (std::nothrow) elixir_posix_dir();
  if (dir == nullptr) {
    errno = ENOMEM;
    return nullptr;
  }
  dir->pattern = Widen(path);
  if (!dir->pattern.empty() && dir->pattern.back() != L'/' && dir->pattern.back() != L'\\') dir->pattern += L'/';
  dir->pattern += L'*';
  dir->find = FindFirstFileExW(dir->pattern.c_str(), FindExInfoBasic, &dir->data, FindExSearchNameMatch, nullptr,
                               FIND_FIRST_EX_LARGE_FETCH);
  if (dir->find == INVALID_HANDLE_VALUE) {
    SetErrnoFromWin32(GetLastError());
    delete dir;
    return nullptr;
  }
  dir->has_pending = true;
  return dir;
}

DIR* fdopendir(int fd) {
  char path[4096];
  if (elixir_posix_fd_path(fd, path, sizeof(path)) < 0) return nullptr;
  DIR* dir = opendir(path);
  if (dir != nullptr) _close(fd);
  return dir;
}

struct dirent* readdir(DIR* dir) {
  if (dir == nullptr) {
    errno = EBADF;
    return nullptr;
  }
  if (!dir->has_pending) {
    if (!FindNextFileW(dir->find, &dir->data)) return nullptr;
  }
  dir->has_pending = false;
  const std::string name = Narrow(dir->data.cFileName);
  std::snprintf(dir->entry.d_name, sizeof(dir->entry.d_name), "%s", name.c_str());
  dir->entry.d_type = (dir->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? DT_DIR : DT_REG;
  dir->entry.d_ino = 0;
  return &dir->entry;
}

void rewinddir(DIR* dir) {
  if (dir == nullptr) return;
  if (dir->find != INVALID_HANDLE_VALUE) FindClose(dir->find);
  dir->find = FindFirstFileExW(dir->pattern.c_str(), FindExInfoBasic, &dir->data, FindExSearchNameMatch, nullptr,
                               FIND_FIRST_EX_LARGE_FETCH);
  dir->has_pending = dir->find != INVALID_HANDLE_VALUE;
}

int closedir(DIR* dir) {
  if (dir == nullptr) {
    errno = EBADF;
    return -1;
  }
  if (dir->find != INVALID_HANDLE_VALUE) FindClose(dir->find);
  delete dir;
  return 0;
}

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
  mutex->srw = nullptr;
  mutex->owner = 0;
  mutex->count = 0;
  mutex->type = attr != nullptr ? attr->type : PTHREAD_MUTEX_NORMAL;
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) { return mutex->count == 0 ? 0 : EBUSY; }

int pthread_mutex_lock(pthread_mutex_t* mutex) {
  const DWORD self = GetCurrentThreadId();
  if (mutex->type != PTHREAD_MUTEX_NORMAL &&
      static_cast<DWORD>(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&mutex->owner), 0, 0)) == self) {
    if (mutex->type == PTHREAD_MUTEX_ERRORCHECK) return EDEADLK;
    ++mutex->count;
    return 0;
  }
  AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&mutex->srw));
  InterlockedExchange(reinterpret_cast<volatile LONG*>(&mutex->owner), static_cast<LONG>(self));
  mutex->count = 1;
  return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
  const DWORD self = GetCurrentThreadId();
  if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == self) {
    ++mutex->count;
    return 0;
  }
  if (!TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&mutex->srw))) return EBUSY;
  InterlockedExchange(reinterpret_cast<volatile LONG*>(&mutex->owner), static_cast<LONG>(self));
  mutex->count = 1;
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
  if (mutex->owner != GetCurrentThreadId()) return EPERM;
  if (--mutex->count != 0) return 0;
  InterlockedExchange(reinterpret_cast<volatile LONG*>(&mutex->owner), 0);
  ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&mutex->srw));
  return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
  attr->type = PTHREAD_MUTEX_NORMAL;
  return 0;
}
int pthread_mutexattr_destroy(pthread_mutexattr_t*) { return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
  if (type < PTHREAD_MUTEX_NORMAL || type > PTHREAD_MUTEX_ERRORCHECK) return EINVAL;
  attr->type = type;
  return 0;
}

static BOOL CALLBACK RunOnce(PINIT_ONCE, PVOID parameter, PVOID*) {
  reinterpret_cast<void (*)(void)>(parameter)();
  return TRUE;
}

int pthread_once(pthread_once_t* once, void (*routine)(void)) {
  InitOnceExecuteOnce(reinterpret_cast<PINIT_ONCE>(&once->init_once), RunOnce,
                      reinterpret_cast<PVOID>(routine), nullptr);
  return 0;
}

pthread_t pthread_self(void) { return static_cast<pthread_t>(GetCurrentThreadId()); }

int pthread_rwlock_init(pthread_rwlock_t* lock, const pthread_rwlockattr_t*) {
  lock->srw = nullptr;
  lock->writer = 0;
  return 0;
}
int pthread_rwlock_destroy(pthread_rwlock_t*) { return 0; }
int pthread_rwlock_rdlock(pthread_rwlock_t* lock) {
  AcquireSRWLockShared(reinterpret_cast<PSRWLOCK>(&lock->srw));
  return 0;
}
int pthread_rwlock_wrlock(pthread_rwlock_t* lock) {
  AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock->srw));
  lock->writer = GetCurrentThreadId();
  return 0;
}
int pthread_rwlock_unlock(pthread_rwlock_t* lock) {
  if (lock->writer == GetCurrentThreadId()) {
    lock->writer = 0;
    ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock->srw));
  } else {
    ReleaseSRWLockShared(reinterpret_cast<PSRWLOCK>(&lock->srw));
  }
  return 0;
}

static int WalkTree(const std::string& path, int (*callback)(const char*, const struct stat*, int, struct FTW*),
                    int depth) {
  const std::wstring pattern = Widen(path.c_str()) + L"/*";
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, 0);
  if (find != INVALID_HANDLE_VALUE) {
    do {
      if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
      const std::string child = path + "/" + Narrow(data.cFileName);
      if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        const int nested = WalkTree(child, callback, depth + 1);
        if (nested != 0) {
          FindClose(find);
          return nested;
        }
      } else {
        struct stat info {};
        stat(child.c_str(), &info);
        struct FTW position {
          static_cast<int>(child.find_last_of('/') + 1), depth + 1
        };
        const int result = callback(child.c_str(), &info, FTW_F, &position);
        if (result != 0) {
          FindClose(find);
          return result;
        }
      }
    } while (FindNextFileW(find, &data));
    FindClose(find);
  }
  struct stat info {};
  stat(path.c_str(), &info);
  struct FTW position {
    static_cast<int>(path.find_last_of('/') + 1), depth
  };
  return callback(path.c_str(), &info, FTW_DP, &position);
}

int nftw(const char* path, int (*callback)(const char*, const struct stat*, int, struct FTW*), int, int flags) {
  if ((flags & FTW_DEPTH) == 0) {
    errno = ENOTSUP;
    return -1;
  }
  return WalkTree(path, callback, 0);
}

}
