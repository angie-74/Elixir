// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "bionic/files/files_internal.h"
#include "bionic/wake_trace.h"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cstring>

namespace elixir::bionic::files {
namespace {

using abi::g_off;
using abi::g_size;
using abi::g_ssize;

constexpr std::size_t kMaxRwCount = 0x7ffff000;
constexpr DWORD kShareAll = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

constexpr std::uint32_t kICRNL = 0x100, kIXON = 0x400, kIUTF8 = 0x4000;
constexpr std::uint32_t kOPOST = 0x1, kONLCR = 0x4;
constexpr std::uint32_t kB38400 = 0xF, kCS8 = 0x30, kCREAD = 0x80;
constexpr std::uint32_t kISIG = 0x1, kICANON = 0x2, kECHOFLAG = 0x8, kECHOE = 0x10, kECHOK = 0x20, kECHOCTL = 0x200,
                        kECHOKE = 0x800, kIEXTEN = 0x8000;

std::uint32_t StatusFlagsFromOpen(int flags) {
  const int cleared = guest::kO_CREAT | guest::kO_EXCL | guest::kO_NOCTTY | guest::kO_TRUNC | guest::kO_CLOEXEC;
  return static_cast<std::uint32_t>((flags & ~cleared) | guest::kO_LARGEFILE);
}

bool IsSymlinkPath(const std::wstring& host) {
  HANDLE handle = OpenForAttributes(host, false);
  if (handle == INVALID_HANDLE_VALUE) return false;
  FILE_ATTRIBUTE_TAG_INFO tag{};
  const bool ok = GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag)) != FALSE;
  CloseHandle(handle);
  return ok && (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && IsSymlinkTag(tag.ReparseTag);
}

vfs::FdKind KindOfHandle(HANDLE handle, bool directory) {
  if (directory) return vfs::FdKind::kDirectory;
  switch (GetFileType(handle)) {
    case FILE_TYPE_CHAR: {
      DWORD mode = 0;
      return GetConsoleMode(handle, &mode) ? vfs::FdKind::kConsole : vfs::FdKind::kNull;
    }
    case FILE_TYPE_PIPE: return vfs::FdKind::kPipe;
    default: return vfs::FdKind::kFile;
  }
}

DescriptionPtr OpenHostDirectory(const Resolved& r, int flags, int* err) {
  const DWORD extra = (flags & guest::kO_NOFOLLOW) != 0 ? FILE_FLAG_OPEN_REPARSE_POINT : 0;
  HANDLE handle = CreateFileW(r.host.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | FILE_TRAVERSE | SYNCHRONIZE,
                              kShareAll, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | extra, nullptr);
  if (handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED) {
    handle = CreateFileW(r.host.c_str(), FILE_READ_ATTRIBUTES | SYNCHRONIZE, kShareAll, nullptr, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS | extra, nullptr);
  }
  if (handle == INVALID_HANDLE_VALUE) {
    *err = PathErrnoFromWin32(GetLastError(), r.host);
    return nullptr;
  }
  auto d = MakeHandleDescription(handle, vfs::FdKind::kDirectory, static_cast<int>(StatusFlagsFromOpen(flags)));
  d->read_only_mount = r.read_only;
  d->guest_path = r.guest;
  d->host_path = r.host;
  return d;
}

std::wstring RandomName(const wchar_t* prefix) {
  unsigned char bytes[8] = {};
  FillRandom(bytes, sizeof(bytes));
  std::wstring name(prefix);
  static const wchar_t kHex[] = L"0123456789abcdef";
  for (unsigned char b : bytes) {
    name.push_back(kHex[b >> 4]);
    name.push_back(kHex[b & 15]);
  }
  return name;
}

DescriptionPtr OpenTmpfile(const Resolved& r, int flags, std::uint32_t mode, int* err) {
  const int access = flags & guest::kO_ACCMODE;
  if (access != guest::kO_WRONLY && access != guest::kO_RDWR) {
    *err = guest::kEINVAL;
    return nullptr;
  }
  const DWORD attributes = GetFileAttributesW(r.host.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    *err = PathErrnoFromWin32(GetLastError(), r.host);
    return nullptr;
  }
  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    *err = guest::kENOTDIR;
    return nullptr;
  }
  if (r.read_only) {
    *err = guest::kEROFS;
    return nullptr;
  }
  for (int attempt = 0; attempt < 16; ++attempt) {
    std::wstring path = r.host;
    if (path.back() != L'\\') path.push_back(L'\\');
    path += RandomName(L".elixir-tmpfile-");
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, kShareAll, nullptr, CREATE_NEW,
                                FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_FILE_EXISTS) continue;
      *err = PathErrnoFromWin32(GetLastError(), path);
      return nullptr;
    }
    if (DeleteHostPath(path, false) != 0) {
      FILE_DISPOSITION_INFO disposition{TRUE};
      SetFileInformationByHandle(handle, FileDispositionInfo, &disposition, sizeof(disposition));
    }
    (void)mode;
    auto d = MakeHandleDescription(handle, vfs::FdKind::kFile,
                                   static_cast<int>(StatusFlagsFromOpen(flags & ~guest::kO_TMPFILE)) |
                                       (flags & guest::kO_ACCMODE));
    d->guest_path = r.guest + "/#tmpfile (deleted)";
    return d;
  }
  *err = guest::kEEXIST;
  return nullptr;
}

DescriptionPtr OpenHost(const Resolved& r, int flags, std::uint32_t mode, int* err) {
  const int access = flags & guest::kO_ACCMODE;
  const bool path_only = (flags & guest::kO_PATH) != 0;
  if ((flags & guest::kO_TMPFILE) == guest::kO_TMPFILE) return OpenTmpfile(r, flags, mode, err);
  const bool write = !path_only && (access == guest::kO_WRONLY || access == guest::kO_RDWR || (flags & guest::kO_TRUNC) != 0);
  const bool create = !path_only && (flags & guest::kO_CREAT) != 0;
  const DWORD attributes = GetFileAttributesW(r.host.c_str());
  const DWORD attribute_error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
  const bool exists = attributes != INVALID_FILE_ATTRIBUTES;
  if (exists) {
    if (create && (flags & guest::kO_EXCL) != 0) {
      *err = guest::kEEXIST;
      return nullptr;
    }
    if ((flags & guest::kO_NOFOLLOW) != 0 && !path_only && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
        IsSymlinkPath(r.host)) {
      *err = guest::kELOOP;
      return nullptr;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      if (write || create) {
        *err = guest::kEISDIR;
        return nullptr;
      }
      return OpenHostDirectory(r, flags, err);
    }
    if ((flags & guest::kO_DIRECTORY) != 0 || r.trailing_slash) {
      *err = guest::kENOTDIR;
      return nullptr;
    }
    if (r.read_only && write) {
      *err = guest::kEROFS;
      return nullptr;
    }
  } else if (attribute_error != ERROR_ACCESS_DENIED) {
    if (!create) {
      *err = PathErrnoFromWin32(attribute_error, r.host);
      return nullptr;
    }
    if (r.trailing_slash) {
      *err = guest::kEISDIR;
      return nullptr;
    }
    if ((flags & guest::kO_DIRECTORY) != 0) {
      *err = guest::kEINVAL;
      return nullptr;
    }
    if (r.read_only) {
      *err = guest::kEROFS;
      return nullptr;
    }
  }
  DWORD desired = 0;
  if (path_only) {
    desired = FILE_READ_ATTRIBUTES | SYNCHRONIZE;
  } else {
    if (access != guest::kO_WRONLY) desired |= GENERIC_READ;
    if (write) desired |= GENERIC_WRITE;
    desired |= FILE_READ_ATTRIBUTES;
  }
  const bool try_execute = !path_only && access != guest::kO_WRONLY;
  DWORD disposition = OPEN_EXISTING;
  if (create) disposition = (flags & guest::kO_EXCL) != 0 ? CREATE_NEW : OPEN_ALWAYS;
  DWORD file_attributes = FILE_ATTRIBUTE_NORMAL;
  if (create && !exists && (mode & ~CurrentUmask() & 0222) == 0) file_attributes = FILE_ATTRIBUTE_READONLY;
  DWORD file_flags = FILE_FLAG_BACKUP_SEMANTICS;
  if ((flags & guest::kO_NOFOLLOW) != 0) file_flags |= FILE_FLAG_OPEN_REPARSE_POINT;
  if ((flags & (guest::kO_SYNC | guest::kO_DSYNC)) != 0) file_flags |= FILE_FLAG_WRITE_THROUGH;
  HANDLE handle = CreateFileW(r.host.c_str(), desired | (try_execute ? FILE_EXECUTE : 0), kShareAll, nullptr,
                              disposition, file_attributes | file_flags, nullptr);
  DWORD error = GetLastError();
  if (handle == INVALID_HANDLE_VALUE && try_execute && error == ERROR_ACCESS_DENIED) {
    handle = CreateFileW(r.host.c_str(), desired, kShareAll, nullptr, disposition, file_attributes | file_flags,
                         nullptr);
    error = GetLastError();
  }
  if (handle == INVALID_HANDLE_VALUE) {
    switch (error) {
      case ERROR_FILE_EXISTS:
      case ERROR_ALREADY_EXISTS:
        *err = guest::kEEXIST;
        break;
      case ERROR_ACCESS_DENIED:
        *err = guest::kEACCES;
        break;
      default:
        *err = PathErrnoFromWin32(error, r.host);
        break;
    }
    return nullptr;
  }
  const bool created = disposition == CREATE_NEW || (disposition == OPEN_ALWAYS && error != ERROR_ALREADY_EXISTS);
  FILE_STANDARD_INFO standard{};
  const bool is_directory = GetFileType(handle) == FILE_TYPE_DISK &&
                            GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard)) &&
                            standard.Directory;
  if (is_directory) {
    CloseHandle(handle);
    if (write || create) {
      *err = guest::kEISDIR;
      return nullptr;
    }
    return OpenHostDirectory(r, flags, err);
  }
  const vfs::FdKind kind = KindOfHandle(handle, false);
  if ((flags & guest::kO_TRUNC) != 0 && !created && write && kind == vfs::FdKind::kFile) {
    FILE_END_OF_FILE_INFO eof{};
    if (!SetFileInformationByHandle(handle, FileEndOfFileInfo, &eof, sizeof(eof))) {
      *err = ErrnoFromWin32(GetLastError());
      CloseHandle(handle);
      return nullptr;
    }
  }
  auto d = MakeHandleDescription(handle, kind, static_cast<int>(StatusFlagsFromOpen(flags)));
  d->read_only_mount = r.read_only;
  d->guest_path = r.guest;
  d->host_path = r.host;
  return d;
}

}

int OpenAt(int dirfd, const char* path, int flags, std::uint32_t mode) {
  if (path == nullptr) return FailWith(guest::kEFAULT, -1);
  Resolved r;
  int err = 0;
  const bool resolved = ResolveAt(dirfd, path, &r, &err);
  const char* resource = ProcResourceLabel(r.guest);
  if (!resolved && r.guest.empty() && std::strncmp(path, "/proc", 5) == 0 && (path[5] == '/' || path[5] == '\0')) {
    resource = "proc-other";
  }
  std::string original_guest;
  const auto record = [&](int error) {
    RecordAppStorageQuery(r, "open", error);
    if (resource != nullptr) RecordProcOpenResult(resource, error, original_guest.empty() ? r.guest : original_guest);
    if (error == guest::kENOENT) RecordMissingSystemPath(original_guest.empty() ? r.guest : original_guest, "open", error);
  };
  const auto fail = [&](int error) {
    record(error);
    return FailWith(error, -1);
  };
  if (!resolved) {
    if (err == guest::kENOENT && !r.guest.empty()) RecordContentCacheOpenResult(r.guest, err);
    return fail(err);
  }
  const bool cloexec = (flags & guest::kO_CLOEXEC) != 0;
  SynthNode node;
  if (r.kind == NodeKind::kSynthetic) {
    if (!SynthLookup(r.guest, &node)) return fail(guest::kENOENT);
    if (node.type == SynthType::kLink && (flags & guest::kO_NOFOLLOW) != 0 && (flags & guest::kO_PATH) == 0) {
      return fail(guest::kELOOP);
    }
    if (!FollowSynthetic(&r, &node, &err, resource != nullptr ? &original_guest : nullptr)) return fail(err);
  }
  DescriptionPtr d;
  if (r.kind == NodeKind::kSynthetic) {
    if ((flags & (guest::kO_CREAT | guest::kO_EXCL)) == (guest::kO_CREAT | guest::kO_EXCL)) {
      return fail(guest::kEEXIST);
    }
    if (node.type != SynthType::kDir && ((flags & guest::kO_DIRECTORY) != 0 || r.trailing_slash)) {
      return fail(guest::kENOTDIR);
    }
    d = OpenSyntheticDescription(r.guest, node, flags, &err);
  } else if (r.kind == NodeKind::kVirtualDir) {
    if ((flags & (guest::kO_CREAT | guest::kO_EXCL)) == (guest::kO_CREAT | guest::kO_EXCL)) {
      return fail(guest::kEEXIST);
    }
    if ((flags & guest::kO_ACCMODE) != guest::kO_RDONLY || (flags & guest::kO_CREAT) != 0) {
      return fail(guest::kEISDIR);
    }
    d = MakeHandleDescription(nullptr, vfs::FdKind::kDirectory, static_cast<int>(StatusFlagsFromOpen(flags)));
    d->provider = Provider::kVirtualDir;
    d->guest_path = r.guest;
  } else {
    d = OpenHost(r, flags, mode, &err);
  }
  if (d == nullptr) {
    if (r.kind == NodeKind::kHost) {
      RecordContentCacheOpenResult(r.guest, err);
    }
    return fail(err);
  }
  const int fd = InstallDescription(std::move(d), cloexec);
  if (r.kind == NodeKind::kHost) RecordContentCacheOpenResult(r.guest, fd >= 0 ? 0 : GuestErrno());
  record(fd >= 0 ? 0 : GuestErrno());
  return fd;
}

namespace {

g_ssize ReadImpl(int fd, void* buffer, g_size count) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  const auto requested = std::min<g_size>(count, kMaxRwCount);
  if (buffer == nullptr && count > 0) {
    const g_ssize result = FailWith(guest::kEFAULT, -1);
    RecordProcReadResult(*d, false, requested, result);
    return result;
  }
  return DescRead(*d, buffer, requested);
}

g_ssize WriteImpl(int fd, const void* buffer, g_size count) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  if (buffer == nullptr && count > 0) return FailWith(guest::kEFAULT, -1);
  return DescWrite(*d, buffer, std::min<g_size>(count, kMaxRwCount));
}

g_ssize PreadImpl(int fd, void* buffer, g_size count, g_off offset) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  return DescPread(*d, buffer, std::min<g_size>(count, kMaxRwCount), offset);
}

g_ssize PwriteImpl(int fd, const void* buffer, g_size count, g_off offset) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  return DescPwrite(*d, buffer, std::min<g_size>(count, kMaxRwCount), offset);
}

bool ValidIov(const guest::Iovec* iov, int count, g_size* total) {
  if (count < 0 || count > guest::kIOV_MAX) return FailWith(guest::kEINVAL, false);
  if (count > 0 && iov == nullptr) return FailWith(guest::kEFAULT, false);
  g_size sum = 0;
  for (int i = 0; i < count; ++i) {
    if (iov[i].iov_len > static_cast<g_size>(INT64_MAX) - sum) return FailWith(guest::kEINVAL, false);
    if (iov[i].iov_len > 0 && iov[i].iov_base == nullptr) return FailWith(guest::kEFAULT, false);
    sum += iov[i].iov_len;
  }
  *total = sum;
  return true;
}

int RwfError(int flags) {
  if ((flags & ~kRWF_SUPPORTED) != 0) return guest::kEOPNOTSUPP;
  if ((flags & kRWF_NOWAIT) != 0) return guest::kEOPNOTSUPP;
  return 0;
}

bool CanTransfer(const Description& d, bool write) {
  if (const FdOps* hooked = HookOps(d); hooked != nullptr) {
    return write ? hooked->write != nullptr : hooked->read != nullptr;
  }
  return !(d.provider == Provider::kHandle && d.kind == vfs::FdKind::kEvent);
}

DescriptionPtr VectorTarget(int fd, const guest::Iovec* iov, int count, g_off offset, bool write, const int* rwf,
                            g_size* total) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return nullptr;
  const auto log_failure = [&](std::optional<std::size_t> requested) {
    if (!write) RecordProcReadResult(*d, offset >= 0, requested, -1);
    return DescriptionPtr{};
  };
  const auto fail = [&](int error, std::optional<std::size_t> requested) {
    SetGuestErrno(error);
    return log_failure(requested);
  };
  if (IsPathOnly(*d)) return fail(guest::kEBADF, std::nullopt);
  if (offset >= 0 && (!DescIsSeekable(*d) || d->use_os_pointer)) return fail(guest::kESPIPE, std::nullopt);
  if (!ValidIov(iov, count, total)) return log_failure(std::nullopt);
  if (AccessMode(*d) == (write ? guest::kO_RDONLY : guest::kO_WRONLY)) return fail(guest::kEBADF, *total);
  if (!CanTransfer(*d, write)) return fail(guest::kEINVAL, *total);
  if (*total > 0 && rwf != nullptr) {
    if (const int error = RwfError(*rwf); error != 0) return fail(error, *total);
  }
  return d;
}

constexpr g_size kGatherLimit = g_size{1} << 20;

g_ssize VectorRead(int fd, const guest::Iovec* iov, int count, g_off offset, const int* rwf = nullptr) {
  g_size total = 0;
  DescriptionPtr d = VectorTarget(fd, iov, count, offset, false, rwf, &total);
  if (d == nullptr) return -1;
  if (total == 0) {
    RecordProcReadResult(*d, offset >= 0, 0, 0);
    return 0;
  }
  if (offset < 0 && !DescIsSeekable(*d) && d->ops == nullptr) {
    int first = 0;
    while (iov[first].iov_len == 0) ++first;
    const g_size want = std::min<g_size>(std::min<g_size>(total, kMaxRwCount), kGatherLimit);
    if (iov[first].iov_len >= want) {
      return DescRead(*d, iov[first].iov_base, std::min<g_size>(iov[first].iov_len, kMaxRwCount));
    }
    std::unique_ptr<unsigned char[]> gathered(new unsigned char[want]);
    const g_ssize got = DescRead(*d, gathered.get(), want);
    if (got <= 0) return got;
    g_size copied = 0;
    for (int i = first; i < count && copied < static_cast<g_size>(got); ++i) {
      const g_size n = std::min<g_size>(iov[i].iov_len, static_cast<g_size>(got) - copied);
      if (n > 0) std::memcpy(iov[i].iov_base, gathered.get() + copied, n);
      copied += n;
    }
    return got;
  }
  g_ssize done = 0;
  for (int i = 0; i < count; ++i) {
    if (iov[i].iov_len == 0) continue;
    const g_ssize got = offset < 0 ? DescRead(*d, iov[i].iov_base, iov[i].iov_len)
                                   : DescPread(*d, iov[i].iov_base, iov[i].iov_len, offset + done);
    if (got < 0) return done > 0 ? done : -1;
    done += got;
    if (static_cast<g_size>(got) < iov[i].iov_len) break;
  }
  return done;
}

g_ssize WriteOnce(Description& d, const void* data, std::size_t size, g_off offset, bool append) {
  if (append) return DescWriteAppend(d, data, size, offset);
  return offset < 0 ? DescWrite(d, data, size) : DescPwrite(d, data, size, offset);
}

bool SyncAfterWrite(Description& d) {
  if (d.provider != Provider::kHandle || d.kind != vfs::FdKind::kFile) return true;
  if (FlushFileBuffers(d.handle)) return true;
  return FailWith(ErrnoFromWin32(GetLastError()), false);
}

g_ssize VectorWrite(int fd, const guest::Iovec* iov, int count, g_off offset, const int* rwf = nullptr) {
  g_size total = 0;
  DescriptionPtr d = VectorTarget(fd, iov, count, offset, true, rwf, &total);
  if (d == nullptr) return -1;
  if (total == 0) return 0;
  const int flags = rwf != nullptr ? *rwf : 0;
  const bool append = (flags & kRWF_APPEND) != 0;
  g_ssize done = 0;
  if (total <= (1u << 20)) {
    std::string gathered;
    gathered.reserve(static_cast<std::size_t>(total));
    for (int i = 0; i < count; ++i) {
      if (iov[i].iov_len > 0) gathered.append(static_cast<const char*>(iov[i].iov_base), iov[i].iov_len);
    }
    done = WriteOnce(*d, gathered.data(), gathered.size(), offset, append);
    if (done < 0) return -1;
  } else {
    for (int i = 0; i < count; ++i) {
      if (iov[i].iov_len == 0) continue;
      const g_ssize put =
          WriteOnce(*d, iov[i].iov_base, iov[i].iov_len, offset < 0 || append ? offset : offset + done, append);
      if (put < 0) {
        if (done == 0) return -1;
        break;
      }
      done += put;
      if (static_cast<g_size>(put) < iov[i].iov_len) break;
    }
  }
  if (done > 0 && (flags & (kRWF_DSYNC | kRWF_SYNC)) != 0 && !SyncAfterWrite(*d)) return -1;
  return done;
}

int PipeImpl(int* fds, int flags) {
  if (fds == nullptr) return FailWith(guest::kEFAULT, -1);
  if ((flags & ~(guest::kO_CLOEXEC | guest::kO_NONBLOCK | guest::kO_DIRECT)) != 0) return FailWith(guest::kEINVAL, -1);
  HANDLE read_end = nullptr;
  HANDLE write_end = nullptr;
  if (!CreatePipe(&read_end, &write_end, nullptr, 65536)) return FailWith(ErrnoFromWin32(GetLastError()), -1);
  const int status = flags & guest::kO_NONBLOCK;
  auto reader = MakeHandleDescription(read_end, vfs::FdKind::kPipe, guest::kO_RDONLY | status);
  auto writer = MakeHandleDescription(write_end, vfs::FdKind::kPipe, guest::kO_WRONLY | status);
  if (status != 0) {
    DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    SetNamedPipeHandleState(write_end, &mode, nullptr, nullptr);
  }
  const bool cloexec = (flags & guest::kO_CLOEXEC) != 0;
  const int r = InstallDescription(std::move(reader), cloexec);
  if (r < 0) return -1;
  const int w = InstallDescription(std::move(writer), cloexec);
  if (w < 0) {
    CloseFd(r);
    return -1;
  }
  fds[0] = r;
  fds[1] = w;
  return 0;
}

bool ConsoleInputReady(Description& d) {
  {
    std::lock_guard<std::mutex> guard(d.lock);
    if (!d.console_in_pending.empty()) return true;
  }
  DWORD count = 0;
  if (!GetNumberOfConsoleInputEvents(d.handle, &count) || count == 0) return false;
  INPUT_RECORD records[128];
  DWORD read = 0;
  if (!PeekConsoleInputW(d.handle, records, 128, &read)) return false;
  for (DWORD i = 0; i < read; ++i) {
    if (records[i].EventType == KEY_EVENT && records[i].Event.KeyEvent.bKeyDown &&
        records[i].Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
      return true;
    }
  }
  return false;
}

short Readiness(Description& d, short events) {
  const short in = static_cast<short>(guest::kPOLLIN | guest::kPOLLRDNORM);
  const short out = static_cast<short>(guest::kPOLLOUT | guest::kPOLLWRNORM);
  const short always = static_cast<short>(guest::kPOLLERR | guest::kPOLLHUP | guest::kPOLLNVAL);
  short ready = 0;
  if (d.provider == Provider::kHandle) {
    switch (d.kind) {
      case vfs::FdKind::kPipe:
        if (AccessMode(d) == guest::kO_RDONLY) {
          DWORD available = 0;
          if (!PeekNamedPipe(d.handle, nullptr, 0, nullptr, &available, nullptr)) {
            ready = static_cast<short>(guest::kPOLLHUP | in);
          } else if (available > 0) {
            ready = in;
          }
        } else {
          ready = out;
        }
        break;
      case vfs::FdKind::kConsole:
        if (AccessMode(d) == guest::kO_RDONLY) {
          if (ConsoleInputReady(d)) ready = in;
        } else {
          ready = out;
        }
        break;
      case vfs::FdKind::kEvent:
        if (const FdOps* hooked = HookOps(d); hooked != nullptr) {
          ready = static_cast<short>(HookedPoll(d, *hooked, static_cast<std::uint16_t>(events)));
        }
        break;
      default:
        ready = static_cast<short>(in | out);
        break;
    }
  } else {
    ready = static_cast<short>(in | out);
  }
  return static_cast<short>(ready & (events | always));
}

short ToWsaEvents(short events) {
  short out = 0;
  if ((events & (guest::kPOLLIN | guest::kPOLLRDNORM)) != 0) out |= POLLRDNORM;
  if ((events & guest::kPOLLRDBAND) != 0) out |= POLLRDBAND;
  if ((events & (guest::kPOLLOUT | guest::kPOLLWRNORM)) != 0) out |= POLLWRNORM;
  return out;
}

short FromWsaEvents(short revents, short requested) {
  short out = 0;
  if ((revents & (POLLRDNORM | POLLRDBAND)) != 0) out |= static_cast<short>(guest::kPOLLIN | guest::kPOLLRDNORM);
  if ((revents & POLLWRNORM) != 0) out |= static_cast<short>(guest::kPOLLOUT | guest::kPOLLWRNORM);
  if ((revents & POLLERR) != 0) out |= guest::kPOLLERR;
  if ((revents & POLLHUP) != 0) out |= guest::kPOLLHUP;
  if ((revents & POLLNVAL) != 0) out |= guest::kPOLLNVAL;
  return static_cast<short>(out & (requested | guest::kPOLLERR | guest::kPOLLHUP | guest::kPOLLNVAL));
}

int PollImpl(guest::Pollfd* fds, std::uint64_t count, int timeout_ms) {
  if (count > static_cast<std::uint64_t>(kMaxFds)) return FailWith(guest::kEINVAL, -1);
  if (count > 0 && fds == nullptr) return FailWith(guest::kEFAULT, -1);
  std::vector<DescriptionPtr> descriptions(static_cast<std::size_t>(count));
  bool all_sockets = count > 0;
  for (std::size_t i = 0; i < count; ++i) {
    if (fds[i].fd >= 0) descriptions[i] = FindDescription(fds[i].fd);
    if (descriptions[i] == nullptr || descriptions[i]->kind != vfs::FdKind::kSocket) all_sockets = false;
  }
  const ULONGLONG start = GetTickCount64();
  DWORD sleep_ms = 1;
  for (;;) {
    int ready = 0;
    std::vector<WSAPOLLFD> sockets;
    std::vector<std::size_t> socket_index;
    for (std::size_t i = 0; i < count; ++i) {
      fds[i].revents = 0;
      if (fds[i].fd < 0) continue;
      if (descriptions[i] == nullptr) {
        fds[i].revents = guest::kPOLLNVAL;
        ++ready;
        continue;
      }
      if (descriptions[i]->kind == vfs::FdKind::kSocket) {
        WSAPOLLFD pfd{};
        pfd.fd = reinterpret_cast<SOCKET>(descriptions[i]->handle);
        pfd.events = ToWsaEvents(fds[i].events);
        sockets.push_back(pfd);
        socket_index.push_back(i);
        continue;
      }
      fds[i].revents = Readiness(*descriptions[i], fds[i].events);
      if (fds[i].revents != 0) ++ready;
    }
    int remaining = -1;
    if (timeout_ms >= 0) {
      const ULONGLONG elapsed = GetTickCount64() - start;
      remaining = elapsed >= static_cast<ULONGLONG>(timeout_ms) ? 0 : static_cast<int>(timeout_ms - elapsed);
    }
    if (!sockets.empty()) {
      const int wait = (ready > 0 || !all_sockets) ? 0 : remaining;
      const int result = WSAPoll(sockets.data(), static_cast<ULONG>(sockets.size()), wait);
      if (result < 0) return FailWith(ErrnoFromWinsock(WSAGetLastError()), -1);
      for (std::size_t k = 0; k < sockets.size(); ++k) {
        const std::size_t i = socket_index[k];
        fds[i].revents = FromWsaEvents(sockets[k].revents, fds[i].events);
        if (fds[i].revents != 0) ++ready;
      }
      if (all_sockets) return ready;
    }
    if (ready > 0 || remaining == 0) return ready;
    DWORD nap = sleep_ms;
    if (remaining > 0 && static_cast<DWORD>(remaining) < nap) nap = static_cast<DWORD>(remaining);
    Sleep(nap);
    sleep_ms = std::min<DWORD>(sleep_ms * 2, 10);
  }
}

int TimespecToTimeout(const guest::Timespec* ts) {
  if (ts == nullptr) return -1;
  if (ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000) return FailWith(guest::kEINVAL, -2);
  const std::int64_t ms = ts->tv_sec * 1000 + (ts->tv_nsec + 999999) / 1000000;
  return ms > 0x7fffffff ? 0x7fffffff : static_cast<int>(ms);
}

HANDLE ConsoleOutputFor(Description& d) {
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (GetConsoleScreenBufferInfo(d.handle, &info)) return d.handle;
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (GetConsoleScreenBufferInfo(out, &info)) return out;
  return GetStdHandle(STD_ERROR_HANDLE);
}

int IoctlImpl(int fd, std::uint32_t request, std::uint64_t arg) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  void* pointer = reinterpret_cast<void*>(static_cast<std::uintptr_t>(arg));
  const bool console = d->provider == Provider::kHandle && d->kind == vfs::FdKind::kConsole;
  switch (request) {
    case static_cast<std::uint32_t>(guest::kFIONBIO): {
      if (pointer == nullptr) return FailWith(guest::kEFAULT, -1);
      int value;
      std::memcpy(&value, pointer, sizeof(value));
      const int flags = d->status_flags.load();
      DescSetStatusFlags(*d, value != 0 ? (flags | guest::kO_NONBLOCK) : (flags & ~guest::kO_NONBLOCK));
      return 0;
    }
    case static_cast<std::uint32_t>(guest::kFIONREAD): {
      if (pointer == nullptr) return FailWith(guest::kEFAULT, -1);
      if (IsDirectoryDescription(*d)) return FailWith(guest::kENOTTY, -1);
      const std::int64_t available = DescAvailable(*d);
      if (available < 0) return HookOps(*d) != nullptr ? -1 : FailWith(guest::kEINVAL, -1);
      const int value = static_cast<int>(std::min<std::int64_t>(available, 0x7fffffff));
      std::memcpy(pointer, &value, sizeof(value));
      return 0;
    }
    case static_cast<std::uint32_t>(guest::kFIOCLEX):
      SetCloexec(fd, true);
      return 0;
    case static_cast<std::uint32_t>(guest::kFIONCLEX):
      SetCloexec(fd, false);
      return 0;
    case static_cast<std::uint32_t>(guest::kTCGETS): {
      if (!console) return FailWith(guest::kENOTTY, -1);
      if (pointer == nullptr) return FailWith(guest::kEFAULT, -1);
      guest::Termios t{};
      t.c_iflag = kICRNL | kIXON | kIUTF8;
      t.c_oflag = kOPOST | kONLCR;
      t.c_cflag = kB38400 | kCS8 | kCREAD;
      t.c_lflag = kISIG | kICANON | kECHOFLAG | kECHOE | kECHOK | kECHOCTL | kECHOKE | kIEXTEN;
      const std::uint8_t cc[17] = {3, 0x1c, 0x7f, 0x15, 4, 0, 1, 0, 0x11, 0x13, 0x1a, 0, 0x12, 0x0f, 0x17, 0x16, 0};
      std::memcpy(t.c_cc, cc, sizeof(cc));
      std::memcpy(pointer, &t, sizeof(t));
      return 0;
    }
    case static_cast<std::uint32_t>(guest::kTCSETS):
    case static_cast<std::uint32_t>(guest::kTCSETS) + 1:
    case static_cast<std::uint32_t>(guest::kTCSETS) + 2:
    case static_cast<std::uint32_t>(guest::kTIOCSWINSZ):
      if (!console) return FailWith(guest::kENOTTY, -1);
      return 0;
    case static_cast<std::uint32_t>(guest::kTIOCGWINSZ): {
      if (!console) return FailWith(guest::kENOTTY, -1);
      if (pointer == nullptr) return FailWith(guest::kEFAULT, -1);
      guest::Winsize size{24, 80, 0, 0};
      CONSOLE_SCREEN_BUFFER_INFO info;
      if (GetConsoleScreenBufferInfo(ConsoleOutputFor(*d), &info)) {
        size.ws_row = static_cast<std::uint16_t>(info.srWindow.Bottom - info.srWindow.Top + 1);
        size.ws_col = static_cast<std::uint16_t>(info.srWindow.Right - info.srWindow.Left + 1);
      }
      std::memcpy(pointer, &size, sizeof(size));
      return 0;
    }
    case static_cast<std::uint32_t>(guest::kTIOCGPGRP): {
      if (!console) return FailWith(guest::kENOTTY, -1);
      if (pointer == nullptr) return FailWith(guest::kEFAULT, -1);
      const std::int32_t pgrp = static_cast<std::int32_t>(GetCurrentProcessId());
      std::memcpy(pointer, &pgrp, sizeof(pgrp));
      return 0;
    }
    default:
      return FailWith(guest::kENOTTY, -1);
  }
}

int FcntlImpl(int fd, int command, std::uint64_t arg) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  const int int_arg = static_cast<int>(arg);
  switch (command) {
    case guest::kF_DUPFD:
    case guest::kF_DUPFD_CLOEXEC:
      if (int_arg < 0 || int_arg >= kMaxFds) return FailWith(guest::kEINVAL, -1);
      return InstallDescription(std::move(d), command == guest::kF_DUPFD_CLOEXEC, int_arg);
    case guest::kF_GETFD: {
      bool cloexec = false;
      if (!GetCloexec(fd, &cloexec)) return FailWith(guest::kEBADF, -1);
      return cloexec ? guest::kFD_CLOEXEC : 0;
    }
    case guest::kF_SETFD:
      if (!SetCloexec(fd, (int_arg & guest::kFD_CLOEXEC) != 0)) return FailWith(guest::kEBADF, -1);
      return 0;
    case guest::kF_GETFL:
      return d->status_flags.load();
    case guest::kF_SETFL:
      DescSetStatusFlags(*d, int_arg);
      return 0;
    case guest::kF_GETLK:
    case guest::kF_SETLK:
    case guest::kF_SETLKW:
    case guest::kF_OFD_GETLK:
    case guest::kF_OFD_SETLK:
    case guest::kF_OFD_SETLKW: {
      auto* lock = reinterpret_cast<GuestFlock*>(static_cast<std::uintptr_t>(arg));
      if (lock == nullptr) return FailWith(guest::kEFAULT, -1);
      if (lock->l_type != guest::kF_RDLCK && lock->l_type != guest::kF_WRLCK && lock->l_type != guest::kF_UNLCK) {
        return FailWith(guest::kEINVAL, -1);
      }
      if (lock->l_whence < guest::kSEEK_SET || lock->l_whence > guest::kSEEK_END) return FailWith(guest::kEINVAL, -1);
      const int access = AccessMode(*d);
      if (command != guest::kF_GETLK && command != guest::kF_OFD_GETLK) {
        if ((lock->l_type == guest::kF_RDLCK && access == guest::kO_WRONLY) ||
            (lock->l_type == guest::kF_WRLCK && access == guest::kO_RDONLY)) {
          return FailWith(guest::kEBADF, -1);
        }
        return 0;
      }
      lock->l_type = static_cast<std::int16_t>(guest::kF_UNLCK);
      return 0;
    }
    case guest::kF_GETPIPE_SZ:
      if (d->kind != vfs::FdKind::kPipe) return FailWith(guest::kEBADF, -1);
      return 65536;
    case guest::kF_SETPIPE_SZ:
      if (d->kind != vfs::FdKind::kPipe) return FailWith(guest::kEBADF, -1);
      if (int_arg < 0) return FailWith(guest::kEINVAL, -1);
      if (int_arg > 65536) return FailWith(guest::kEPERM, -1);
      return 65536;
    default:
      return FailWith(guest::kEINVAL, -1);
  }
}

}

ELIXIR_BIONIC_FN(int, open)(const char* path, int flags, std::uint32_t mode) noexcept {
  return Guarded(-1, [&] { return OpenAt(guest::kAT_FDCWD, path, flags, mode); });
}
ELIXIR_BIONIC_FN(int, openat)(int dirfd, const char* path, int flags, std::uint32_t mode) noexcept {
  return Guarded(-1, [&] { return OpenAt(dirfd, path, flags, mode); });
}
ELIXIR_BIONIC_FN(int, __open_2)(const char* path, int flags) noexcept {
  if ((flags & guest::kO_CREAT) != 0 || (flags & guest::kO_TMPFILE) == guest::kO_TMPFILE) {
    FortifyFatal("open", "called with O_CREAT or O_TMPFILE, but missing mode");
  }
  return Guarded(-1, [&] { return OpenAt(guest::kAT_FDCWD, path, flags, 0); });
}
ELIXIR_BIONIC_FN(int, __openat_2)(int dirfd, const char* path, int flags) noexcept {
  if ((flags & guest::kO_CREAT) != 0 || (flags & guest::kO_TMPFILE) == guest::kO_TMPFILE) {
    FortifyFatal("openat", "called with O_CREAT or O_TMPFILE, but missing mode");
  }
  return Guarded(-1, [&] { return OpenAt(dirfd, path, flags, 0); });
}
ELIXIR_BIONIC_FN(int, creat)(const char* path, std::uint32_t mode) noexcept {
  return Guarded(-1, [&] {
    return OpenAt(guest::kAT_FDCWD, path, guest::kO_CREAT | guest::kO_WRONLY | guest::kO_TRUNC, mode);
  });
}

ELIXIR_BIONIC_FN(int, close)(int fd) noexcept {
  return Guarded(-1, [&] { return FdsanClose(fd, 0); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, read)(int fd, void* buffer, abi::g_size count) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return ReadImpl(fd, buffer, count); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, write)(int fd, const void* buffer, abi::g_size count) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return WriteImpl(fd, buffer, count); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, pread)(int fd, void* buffer, abi::g_size count, abi::g_off offset) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return PreadImpl(fd, buffer, count, offset); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, pwrite)(int fd, const void* buffer, abi::g_size count, abi::g_off offset) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return PwriteImpl(fd, buffer, count, offset); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, readv)(int fd, const guest::Iovec* iov, int count) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return VectorRead(fd, iov, count, -1); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, writev)(int fd, const guest::Iovec* iov, int count) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return VectorWrite(fd, iov, count, -1); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, preadv)(int fd, const guest::Iovec* iov, int count, abi::g_off offset) noexcept {
  return Guarded<abi::g_ssize>(-1, [&]() -> abi::g_ssize {
    if (offset < 0) return FailWith(guest::kEINVAL, -1);
    return VectorRead(fd, iov, count, offset);
  });
}
ELIXIR_BIONIC_FN(abi::g_ssize, pwritev)(int fd, const guest::Iovec* iov, int count, abi::g_off offset) noexcept {
  return Guarded<abi::g_ssize>(-1, [&]() -> abi::g_ssize {
    if (offset < 0) return FailWith(guest::kEINVAL, -1);
    return VectorWrite(fd, iov, count, offset);
  });
}

ELIXIR_BIONIC_FN(abi::g_ssize, preadv2)(int fd, const guest::Iovec* iov, int count, abi::g_off offset,
                                        int flags) noexcept {
  return Guarded<abi::g_ssize>(-1, [&]() -> abi::g_ssize {
    if (offset < -1) return FailWith(guest::kEINVAL, -1);
    return VectorRead(fd, iov, count, offset, &flags);
  });
}
ELIXIR_BIONIC_FN(abi::g_ssize, pwritev2)(int fd, const guest::Iovec* iov, int count, abi::g_off offset,
                                         int flags) noexcept {
  return Guarded<abi::g_ssize>(-1, [&]() -> abi::g_ssize {
    if (offset < -1) return FailWith(guest::kEINVAL, -1);
    return VectorWrite(fd, iov, count, offset, &flags);
  });
}
ELIXIR_BIONIC_FN(abi::g_off, lseek)(int fd, abi::g_off offset, int whence) noexcept {
  return Guarded<abi::g_off>(-1, [&]() -> abi::g_off {
    DescriptionPtr d = GetDescription(fd);
    if (d == nullptr) return -1;
    return DescSeek(*d, offset, whence);
  });
}
ELIXIR_BIONIC_FN(int, dup)(int fd) noexcept {
  return Guarded(-1, [&] {
    DescriptionPtr d = GetDescription(fd);
    if (d == nullptr) return -1;
    return InstallDescription(std::move(d), false);
  });
}
ELIXIR_BIONIC_FN(int, dup2)(int old_fd, int new_fd) noexcept {
  return Guarded(-1, [&] {
    DescriptionPtr d = GetDescription(old_fd);
    if (d == nullptr) return -1;
    if (old_fd == new_fd) return new_fd;
    return InstallDescriptionAt(std::move(d), new_fd, false);
  });
}
ELIXIR_BIONIC_FN(int, dup3)(int old_fd, int new_fd, int flags) noexcept {
  return Guarded(-1, [&] {
    if ((flags & ~guest::kO_CLOEXEC) != 0 || old_fd == new_fd) return FailWith(guest::kEINVAL, -1);
    DescriptionPtr d = GetDescription(old_fd);
    if (d == nullptr) return -1;
    return InstallDescriptionAt(std::move(d), new_fd, (flags & guest::kO_CLOEXEC) != 0);
  });
}
ELIXIR_BIONIC_FN(int, fcntl)(int fd, int command, std::uint64_t arg) noexcept {
  return Guarded(-1, [&] { return FcntlImpl(fd, command, arg); });
}
ELIXIR_BIONIC_FN(int, ioctl)(int fd, int request, std::uint64_t arg) noexcept {
  return Guarded(-1, [&] { return IoctlImpl(fd, static_cast<std::uint32_t>(request), arg); });
}
ELIXIR_BIONIC_FN(int, pipe)(int* fds) noexcept {
  return Guarded(-1, [&] { return PipeImpl(fds, 0); });
}
ELIXIR_BIONIC_FN(int, pipe2)(int* fds, int flags) noexcept {
  return Guarded(-1, [&] { return PipeImpl(fds, flags); });
}
ELIXIR_BIONIC_FN(int, poll)(guest::Pollfd* fds, std::uint64_t count, int timeout) noexcept {
  const std::int64_t traced_from = wake_trace::Enabled() ? wake_trace::NowNs() : 0;
  const int result = Guarded(-1, [&] { return PollImpl(fds, count, timeout); });
  if (traced_from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kIoReady, nullptr, traced_from);
  return result;
}
ELIXIR_BIONIC_FN(int, ppoll)(guest::Pollfd* fds, std::uint64_t count, const guest::Timespec* timeout,
                             const void* sigmask) noexcept {
  return Guarded(-1, [&] {
    const int ms = TimespecToTimeout(timeout);
    if (ms == -2) return -1;
    return PollImpl(fds, count, ms);
  });
}
ELIXIR_BIONIC_FN(int, __poll_chk)(guest::Pollfd* fds, std::uint64_t count, int timeout, abi::g_size fds_size) noexcept {
  if (fds_size / sizeof(guest::Pollfd) < count) FortifyFatal("poll", "fd count > fd buffer size");
  return Guarded(-1, [&] { return PollImpl(fds, count, timeout); });
}
ELIXIR_BIONIC_FN(int, __ppoll_chk)(guest::Pollfd* fds, std::uint64_t count, const guest::Timespec* timeout,
                                   const void* sigmask, abi::g_size fds_size) noexcept {
  if (fds_size / sizeof(guest::Pollfd) < count) FortifyFatal("ppoll", "fd count > fd buffer size");
  return Guarded(-1, [&] {
    const int ms = TimespecToTimeout(timeout);
    if (ms == -2) return -1;
    return PollImpl(fds, count, ms);
  });
}
ELIXIR_BIONIC_FN(int, isatty)(int fd) noexcept {
  return Guarded(0, [&] {
    DescriptionPtr d = GetDescription(fd);
    if (d == nullptr) return 0;
    if (d->provider == Provider::kHandle && d->kind == vfs::FdKind::kConsole) return 1;
    return FailWith(guest::kENOTTY, 0);
  });
}
ELIXIR_BIONIC_FN(int, ttyname_r)(int fd, char* buffer, abi::g_size length) noexcept {
  return Guarded(guest::kENOMEM, [&] {
    DescriptionPtr d = FindDescription(fd);
    if (d == nullptr) return guest::kEBADF;
    if (d->provider != Provider::kHandle || d->kind != vfs::FdKind::kConsole) return guest::kENOTTY;
    static const char kName[] = "/dev/pts/0";
    if (buffer == nullptr || length < sizeof(kName)) return guest::kERANGE;
    std::memcpy(buffer, kName, sizeof(kName));
    return 0;
  });
}
ELIXIR_BIONIC_FN(char*, ttyname)(int fd) noexcept {
  static thread_local char buffer[64];
  const int error = elixir_bionic_ttyname_r(fd, buffer, sizeof(buffer));
  if (error != 0) return FailWith(error, static_cast<char*>(nullptr));
  return buffer;
}
ELIXIR_BIONIC_FN(int, flock)(int fd, int operation) noexcept {
  return Guarded(-1, [&] {
    if (GetDescription(fd) == nullptr) return -1;
    const int base = operation & ~kLOCK_NB;
    if (base != kLOCK_SH && base != kLOCK_EX && base != kLOCK_UN) return FailWith(guest::kEINVAL, -1);
    return 0;
  });
}

namespace {
int SyncImpl(int fd) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  if (d->provider != Provider::kHandle) return d->provider == Provider::kVirtualDir ? 0 : FailWith(guest::kEINVAL, -1);
  if (d->kind == vfs::FdKind::kDirectory) return 0;
  if (d->kind != vfs::FdKind::kFile) return FailWith(guest::kEINVAL, -1);
  if (!FlushFileBuffers(d->handle)) {
    const DWORD error = GetLastError();
    if (error == ERROR_ACCESS_DENIED || error == ERROR_INVALID_HANDLE) return 0;
    return FailWith(ErrnoFromWin32(error), -1);
  }
  return 0;
}
}

ELIXIR_BIONIC_FN(int, fsync)(int fd) noexcept {
  return Guarded(-1, [&] { return SyncImpl(fd); });
}
ELIXIR_BIONIC_FN(int, fdatasync)(int fd) noexcept {
  return Guarded(-1, [&] { return SyncImpl(fd); });
}
ELIXIR_BIONIC_FN(int, syncfs)(int fd) noexcept {
  return Guarded(-1, [&] {
    if (GetDescription(fd) == nullptr) return -1;
    return 0;
  });
}
ELIXIR_BIONIC_FN(void, sync)() noexcept {}

ELIXIR_BIONIC_FN(abi::g_ssize, __read_chk)(int fd, void* buffer, abi::g_size count, abi::g_size buffer_size) noexcept {
  if (count > buffer_size) FortifyFatal("read", "prevented write past end of buffer");
  if (count > static_cast<abi::g_size>(INT64_MAX)) FortifyFatal("read", "count > SSIZE_MAX");
  return elixir_bionic_read(fd, buffer, count);
}
ELIXIR_BIONIC_FN(abi::g_ssize, __write_chk)(int fd, const void* buffer, abi::g_size count,
                                            abi::g_size buffer_size) noexcept {
  if (count > buffer_size) FortifyFatal("write", "prevented read past end of buffer");
  if (count > static_cast<abi::g_size>(INT64_MAX)) FortifyFatal("write", "count > SSIZE_MAX");
  return elixir_bionic_write(fd, buffer, count);
}
ELIXIR_BIONIC_FN(abi::g_ssize, __pread_chk)(int fd, void* buffer, abi::g_size count, abi::g_off offset,
                                            abi::g_size buffer_size) noexcept {
  if (count > buffer_size) FortifyFatal("pread", "prevented write past end of buffer");
  if (count > static_cast<abi::g_size>(INT64_MAX)) FortifyFatal("pread", "count > SSIZE_MAX");
  return elixir_bionic_pread(fd, buffer, count, offset);
}
ELIXIR_BIONIC_FN(abi::g_ssize, __pwrite_chk)(int fd, const void* buffer, abi::g_size count, abi::g_off offset,
                                             abi::g_size buffer_size) noexcept {
  if (count > buffer_size) FortifyFatal("pwrite", "prevented read past end of buffer");
  if (count > static_cast<abi::g_size>(INT64_MAX)) FortifyFatal("pwrite", "count > SSIZE_MAX");
  return elixir_bionic_pwrite(fd, buffer, count, offset);
}

void RegisterFdExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("open", &elixir_bionic_open);
  add("open64", &elixir_bionic_open);
  add("openat", &elixir_bionic_openat);
  add("openat64", &elixir_bionic_openat);
  add("__open_2", &elixir_bionic___open_2);
  add("__openat_2", &elixir_bionic___openat_2);
  add("creat", &elixir_bionic_creat);
  add("creat64", &elixir_bionic_creat);
  add("close", &elixir_bionic_close);
  add("read", &elixir_bionic_read);
  add("write", &elixir_bionic_write);
  add("pread", &elixir_bionic_pread);
  add("pread64", &elixir_bionic_pread);
  add("pwrite", &elixir_bionic_pwrite);
  add("pwrite64", &elixir_bionic_pwrite);
  add("readv", &elixir_bionic_readv);
  add("writev", &elixir_bionic_writev);
  add("preadv", &elixir_bionic_preadv);
  add("preadv64", &elixir_bionic_preadv);
  add("pwritev", &elixir_bionic_pwritev);
  add("pwritev64", &elixir_bionic_pwritev);
  add("preadv2", &elixir_bionic_preadv2);
  add("preadv64v2", &elixir_bionic_preadv2);
  add("pwritev2", &elixir_bionic_pwritev2);
  add("pwritev64v2", &elixir_bionic_pwritev2);
  add("lseek", &elixir_bionic_lseek);
  add("lseek64", &elixir_bionic_lseek);
  add("dup", &elixir_bionic_dup);
  add("dup2", &elixir_bionic_dup2);
  add("dup3", &elixir_bionic_dup3);
  add("fcntl", &elixir_bionic_fcntl);
  add("ioctl", &elixir_bionic_ioctl);
  add("pipe", &elixir_bionic_pipe);
  add("pipe2", &elixir_bionic_pipe2);
  add("isatty", &elixir_bionic_isatty);
  add("ttyname", &elixir_bionic_ttyname);
  add("ttyname_r", &elixir_bionic_ttyname_r);
  add("flock", &elixir_bionic_flock);
  add("fsync", &elixir_bionic_fsync);
  add("fdatasync", &elixir_bionic_fdatasync);
  add("syncfs", &elixir_bionic_syncfs);
  add("sync", &elixir_bionic_sync);
  add("__read_chk", &elixir_bionic___read_chk);
  add("__write_chk", &elixir_bionic___write_chk);
  add("__pread_chk", &elixir_bionic___pread_chk);
  add("__pread64_chk", &elixir_bionic___pread_chk);
  add("__pwrite_chk", &elixir_bionic___pwrite_chk);
  add("__pwrite64_chk", &elixir_bionic___pwrite_chk);
}

}
