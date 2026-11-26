// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "bionic/files/files_internal.h"

#include <windows.h>
#include <winioctl.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "platform/trace_clock.h"

namespace elixir::bionic::files {
namespace {

constexpr DWORD kShareAll = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
std::atomic<std::uint32_t> g_umask{077};

bool IsProcMountPath(std::string_view path) noexcept { return path == "/proc" || path.compare(0, 6, "/proc/") == 0; }

struct StatBasicInformation {
  LARGE_INTEGER FileId;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER EndOfFile;
  DWORD FileAttributes;
  DWORD ReparseTag;
  DWORD NumberOfLinks;
  DWORD DeviceType;
  DWORD DeviceCharacteristics;
  DWORD Reserved;
  LARGE_INTEGER VolumeSerialNumber;
  FILE_ID_128 FileId128;
};
using GetFileInformationByNameFn = BOOL(WINAPI*)(PCWSTR, int, PVOID, ULONG);
constexpr int kFileStatBasicByNameInfo = 3;

GetFileInformationByNameFn StatByName() {
  static const GetFileInformationByNameFn fn = [] {
    for (const wchar_t* module : {L"kernelbase.dll", L"kernel32.dll"}) {
      HMODULE handle = GetModuleHandleW(module);
      if (handle == nullptr) continue;
      if (auto* proc = GetProcAddress(handle, "GetFileInformationByName")) {
        return reinterpret_cast<GetFileInformationByNameFn>(reinterpret_cast<void*>(proc));
      }
    }
    return static_cast<GetFileInformationByNameFn>(nullptr);
  }();
  return fn;
}

bool HasExecutableSuffix(std::string_view name) {
  auto ends = [&](std::string_view suffix) {
    return name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
  };
  return ends(".so") || ends(".sh") || ends(".exe") || ends(".dll") || name.find(".so.") != std::string_view::npos;
}

std::uint64_t PseudoInode(const void* pointer) {
  return HashString(std::to_string(reinterpret_cast<std::uintptr_t>(pointer))) | 1;
}

void FillSpecial(guest::Stat* st, std::uint32_t mode, std::uint64_t rdev, std::uint64_t ino, std::uint64_t dev) {
  std::memset(st, 0, sizeof(*st));
  st->st_mode = mode;
  st->st_rdev = rdev;
  st->st_ino = ino;
  st->st_dev = dev;
  st->st_nlink = 1;
  st->st_uid = kGuestUid;
  st->st_gid = kGuestGid;
  st->st_blksize = 4096;
  guest::Timespec now;
  FileTimeToTimespec(NowFileTime(), &now);
  st->st_atim = st->st_mtim = st->st_ctim = now;
}

bool StatHostPath(const Resolved& r, bool follow, guest::Stat* st, int* err) {
  HostFileInfo info;
  bool have = false;
  if (GetFileInformationByNameFn by_name = StatByName()) {
    StatBasicInformation basic{};
    if (by_name(r.host.c_str(), kFileStatBasicByNameInfo, &basic, sizeof(basic))) {
      if ((basic.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
        info.attributes = basic.FileAttributes;
        info.volume_serial = static_cast<std::uint64_t>(basic.VolumeSerialNumber.QuadPart);
        std::memcpy(&info.file_id, basic.FileId128.Identifier, sizeof(info.file_id));
        info.links = basic.NumberOfLinks;
        info.size = basic.EndOfFile.QuadPart;
        info.allocation = basic.AllocationSize.QuadPart;
        info.creation_time = basic.CreationTime.QuadPart;
        info.access_time = basic.LastAccessTime.QuadPart;
        info.write_time = basic.LastWriteTime.QuadPart;
        info.change_time = basic.ChangeTime.QuadPart != 0 ? basic.ChangeTime.QuadPart : basic.LastWriteTime.QuadPart;
        have = true;
      }
    } else {
      const DWORD error = GetLastError();
      if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_DIRECTORY) {
        *err = PathErrnoFromWin32(error, r.host);
        return false;
      }
    }
  }
  if (!have) {
    HANDLE handle = OpenForAttributes(r.host, follow);
    if (handle == INVALID_HANDLE_VALUE) {
      *err = PathErrnoFromWin32(GetLastError(), r.host);
      return false;
    }
    const bool ok = QueryHandleInfo(handle, &info);
    const DWORD error = GetLastError();
    CloseHandle(handle);
    if (!ok) {
      *err = ErrnoFromWin32(error);
      return false;
    }
  }
  const bool is_link = !follow && (info.attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && IsSymlinkTag(info.reparse_tag);
  if (r.trailing_slash && (info.attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && !is_link) {
    *err = guest::kENOTDIR;
    return false;
  }
  FillStatFromInfo(info, r.guest, st);
  if (is_link) {
    std::string target;
    int ignored = 0;
    if (ReadHostLink(r.host, &target, &ignored)) st->st_size = static_cast<std::int64_t>(target.size());
  }
  return true;
}

bool ConvertTimes(const guest::Timespec* times, FILETIME* atime, FILETIME* mtime, bool* set_atime, bool* set_mtime) {
  const std::int64_t now = NowFileTime();
  auto convert = [&](const guest::Timespec* t, FILETIME* out, bool* set) {
    std::int64_t value = now;
    if (t != nullptr) {
      if (t->tv_nsec == kUTIME_OMIT) {
        *set = false;
        return true;
      }
      if (t->tv_nsec != kUTIME_NOW) {
        if (t->tv_nsec < 0 || t->tv_nsec >= 1000000000) return false;
        value = TimespecToFileTime(t->tv_sec, t->tv_nsec);
      }
    }
    if (value < 0) value = 0;
    out->dwLowDateTime = static_cast<DWORD>(value);
    out->dwHighDateTime = static_cast<DWORD>(static_cast<std::uint64_t>(value) >> 32);
    *set = true;
    return true;
  };
  return convert(times != nullptr ? &times[0] : nullptr, atime, set_atime) &&
         convert(times != nullptr ? &times[1] : nullptr, mtime, set_mtime);
}

bool SetHandleTimes(HANDLE handle, const FILETIME& atime, const FILETIME& mtime, bool set_atime, bool set_mtime) {
  return SetFileTime(handle, nullptr, set_atime ? &atime : nullptr, set_mtime ? &mtime : nullptr) != FALSE;
}

template <typename Apply>
int WithWritableAttributes(Description& d, Apply apply) {
  if (d.provider != Provider::kHandle || (d.kind != vfs::FdKind::kFile && d.kind != vfs::FdKind::kDirectory)) {
    return guest::kEPERM;
  }
  if (apply(d.handle)) return 0;
  DWORD error = GetLastError();
  if (error != ERROR_ACCESS_DENIED) return ErrnoFromWin32(error);
  HANDLE reopened = ReOpenFile(d.handle, FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES, kShareAll,
                               FILE_FLAG_BACKUP_SEMANTICS);
  if (reopened == INVALID_HANDLE_VALUE && !d.host_path.empty()) {
    reopened = OpenForAttributes(d.host_path, true, FILE_WRITE_ATTRIBUTES);
  }
  if (reopened == INVALID_HANDLE_VALUE) return ErrnoFromWin32(GetLastError());
  const bool ok = apply(reopened);
  error = GetLastError();
  CloseHandle(reopened);
  return ok ? 0 : ErrnoFromWin32(error);
}

bool SetReadOnly(HANDLE handle, bool read_only) {
  FILE_BASIC_INFO basic{};
  if (!GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic))) return false;
  if ((basic.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return true;
  DWORD attributes = basic.FileAttributes & ~FILE_ATTRIBUTE_NORMAL;
  attributes = read_only ? (attributes | FILE_ATTRIBUTE_READONLY) : (attributes & ~FILE_ATTRIBUTE_READONLY);
  if (attributes == basic.FileAttributes) return true;
  FILE_BASIC_INFO update{};
  update.FileAttributes = attributes == 0 ? FILE_ATTRIBUTE_NORMAL : attributes;
  return SetFileInformationByHandle(handle, FileBasicInfo, &update, sizeof(update)) != FALSE;
}

bool ResolveMeta(int dirfd, const char* path, bool follow, Resolved* r, SynthNode* node, int* err,
                 bool* proc_query = nullptr) {
  if (path == nullptr) {
    *err = guest::kEFAULT;
    return false;
  }
  if (path[0] == '\0') {
    *err = guest::kENOENT;
    return false;
  }
  const bool resolved = ResolveAt(dirfd, path, r, err);
  if (proc_query != nullptr) {
    *proc_query = IsProcMountPath(r->guest) ||
                  (!resolved && r->guest.empty() && std::strncmp(path, "/proc", 5) == 0 &&
                   (path[5] == '/' || path[5] == '\0'));
  }
  if (!resolved) return false;
  if (r->kind == NodeKind::kSynthetic) {
    if (follow) return FollowSynthetic(r, node, err);
    if (!SynthLookup(r->guest, node)) {
      *err = guest::kENOENT;
      return false;
    }
  }
  return true;
}

int StatAt(int dirfd, const char* path, guest::Stat* st, int flags) {
  if (st == nullptr) return FailWith(guest::kEFAULT, -1);
  if ((flags & ~(guest::kAT_SYMLINK_NOFOLLOW | guest::kAT_EMPTY_PATH | guest::kAT_NO_AUTOMOUNT |
                 kAT_STATX_SYNC_MASK)) != 0) {
    return FailWith(guest::kEINVAL, -1);
  }
  if (path != nullptr && path[0] == '\0' && (flags & guest::kAT_EMPTY_PATH) != 0) {
    if (dirfd == guest::kAT_FDCWD) path = ".";
    else {
      DescriptionPtr d = GetDescription(dirfd);
      if (d == nullptr) return -1;
      int err = 0;
      if (!DescStat(*d, st, &err)) return FailWith(err, -1);
      return 0;
    }
  }
  Resolved r;
  int err = 0;
  if (path == nullptr) return FailWith(guest::kEFAULT, -1);
  if (path[0] == '\0') return FailWith(guest::kENOENT, -1);
  if (!ResolveAt(dirfd, path, &r, &err)) {
    RecordMissingSystemPath(r.guest, "stat", err);
    return FailWith(err, -1);
  }
  if (!StatResolved(r, (flags & guest::kAT_SYMLINK_NOFOLLOW) == 0, st, &err)) {
    RecordMissingSystemPath(r.guest, "stat", err);
    return FailWith(err, -1);
  }
  return 0;
}

int AccessAt(int dirfd, const char* path, int mode, int flags) {
  if ((mode & ~(guest::kR_OK | guest::kW_OK | guest::kX_OK)) != 0) return FailWith(guest::kEINVAL, -1);
  if ((flags & ~(guest::kAT_EACCESS | guest::kAT_SYMLINK_NOFOLLOW | guest::kAT_EMPTY_PATH)) != 0) {
    return FailWith(guest::kEINVAL, -1);
  }
  guest::Stat st;
  if (StatAt(dirfd, path, &st, flags & (guest::kAT_SYMLINK_NOFOLLOW | guest::kAT_EMPTY_PATH)) != 0) return -1;
  Resolved r;
  int err = 0;
  const bool read_only_mount = path != nullptr && path[0] != '\0' && ResolveAt(dirfd, path, &r, &err) && r.read_only &&
                               r.kind == NodeKind::kHost;
  const std::uint32_t type = st.st_mode & guest::kS_IFMT;
  if ((mode & guest::kW_OK) != 0) {
    if (read_only_mount && type != static_cast<std::uint32_t>(guest::kS_IFCHR)) return FailWith(guest::kEROFS, -1);
    if ((st.st_mode & 0200) == 0) return FailWith(guest::kEACCES, -1);
  }
  if ((mode & guest::kX_OK) != 0 && (st.st_mode & 0100) == 0) return FailWith(guest::kEACCES, -1);
  if ((mode & guest::kR_OK) != 0 && (st.st_mode & 0400) == 0) return FailWith(guest::kEACCES, -1);
  return 0;
}

int ChmodAt(int dirfd, const char* path, std::uint32_t mode, bool follow) {
  Resolved r;
  SynthNode node;
  int err = 0;
  if (!ResolveMeta(dirfd, path, follow, &r, &node, &err)) return FailWith(err, -1);
  if (r.kind != NodeKind::kHost) return FailWith(guest::kEPERM, -1);
  if (r.read_only) return FailWith(guest::kEROFS, -1);
  HANDLE handle = OpenForAttributes(r.host, follow, FILE_WRITE_ATTRIBUTES);
  if (handle == INVALID_HANDLE_VALUE) return FailWith(PathErrnoFromWin32(GetLastError(), r.host), -1);
  if (!follow) {
    FILE_ATTRIBUTE_TAG_INFO tag{};
    if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag)) &&
        (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && IsSymlinkTag(tag.ReparseTag)) {
      CloseHandle(handle);
      return FailWith(guest::kEOPNOTSUPP, -1);
    }
  }
  const bool ok = SetReadOnly(handle, (mode & 0222) == 0);
  const DWORD error = GetLastError();
  CloseHandle(handle);
  return ok ? 0 : FailWith(ErrnoFromWin32(error), -1);
}

int ChownAt(int dirfd, const char* path, int flags) {
  if ((flags & ~(guest::kAT_SYMLINK_NOFOLLOW | guest::kAT_EMPTY_PATH)) != 0) return FailWith(guest::kEINVAL, -1);
  guest::Stat st;
  if (StatAt(dirfd, path, &st, flags) != 0) return -1;
  return 0;
}

int UtimensAt(int dirfd, const char* path, const guest::Timespec* times, int flags) {
  if ((flags & ~guest::kAT_SYMLINK_NOFOLLOW) != 0) return FailWith(guest::kEINVAL, -1);
  FILETIME atime{}, mtime{};
  bool set_atime = false, set_mtime = false;
  if (!ConvertTimes(times, &atime, &mtime, &set_atime, &set_mtime)) return FailWith(guest::kEINVAL, -1);
  if (path == nullptr) {
    DescriptionPtr d = GetDescription(dirfd);
    if (d == nullptr) return -1;
    if (!set_atime && !set_mtime) return 0;
    const int error = WithWritableAttributes(*d, [&](HANDLE h) { return SetHandleTimes(h, atime, mtime, set_atime, set_mtime); });
    return error == 0 ? 0 : FailWith(error, -1);
  }
  const bool follow = (flags & guest::kAT_SYMLINK_NOFOLLOW) == 0;
  Resolved r;
  SynthNode node;
  int err = 0;
  if (!ResolveMeta(dirfd, path, follow, &r, &node, &err)) return FailWith(err, -1);
  if (r.kind != NodeKind::kHost) return FailWith(guest::kEACCES, -1);
  if (r.read_only) return FailWith(guest::kEROFS, -1);
  HANDLE handle = OpenForAttributes(r.host, follow, FILE_WRITE_ATTRIBUTES);
  if (handle == INVALID_HANDLE_VALUE) return FailWith(PathErrnoFromWin32(GetLastError(), r.host), -1);
  const bool ok = !(set_atime || set_mtime) || SetHandleTimes(handle, atime, mtime, set_atime, set_mtime);
  const DWORD error = GetLastError();
  CloseHandle(handle);
  return ok ? 0 : FailWith(ErrnoFromWin32(error), -1);
}

bool TimevalsToTimespecs(const guest::Timeval* tv, guest::Timespec out[2]) {
  for (int i = 0; i < 2; ++i) {
    if (tv[i].tv_usec < 0 || tv[i].tv_usec >= 1000000) return false;
    out[i].tv_sec = tv[i].tv_sec;
    out[i].tv_nsec = tv[i].tv_usec * 1000;
  }
  return true;
}

int TruncatePath(const char* path, std::int64_t length) {
  if (length < 0) return FailWith(guest::kEINVAL, -1);
  Resolved r;
  SynthNode node;
  int err = 0;
  if (!ResolveMeta(guest::kAT_FDCWD, path, true, &r, &node, &err)) return FailWith(err, -1);
  if (r.kind == NodeKind::kVirtualDir || (r.kind == NodeKind::kSynthetic && node.type == SynthType::kDir)) {
    return FailWith(guest::kEISDIR, -1);
  }
  if (r.kind != NodeKind::kHost) return FailWith(guest::kEACCES, -1);
  const DWORD attributes = GetFileAttributesW(r.host.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) return FailWith(PathErrnoFromWin32(GetLastError(), r.host), -1);
  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return FailWith(guest::kEISDIR, -1);
  if (r.read_only) return FailWith(guest::kEROFS, -1);
  HANDLE handle = CreateFileW(r.host.c_str(), GENERIC_WRITE, kShareAll, nullptr, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return FailWith(PathErrnoFromWin32(GetLastError(), r.host), -1);
  FILE_END_OF_FILE_INFO eof{};
  eof.EndOfFile.QuadPart = length;
  const bool ok = SetFileInformationByHandle(handle, FileEndOfFileInfo, &eof, sizeof(eof)) != FALSE;
  const DWORD error = GetLastError();
  CloseHandle(handle);
  if (!ok) return FailWith(error == ERROR_USER_MAPPED_FILE ? guest::kEBUSY : ErrnoFromWin32(error), -1);
  return 0;
}

int TruncateFd(int fd, std::int64_t length) {
  if (length < 0) return FailWith(guest::kEINVAL, -1);
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  if (d->provider != Provider::kHandle || d->kind != vfs::FdKind::kFile || AccessMode(*d) == guest::kO_RDONLY ||
      IsPathOnly(*d)) {
    return FailWith(guest::kEINVAL, -1);
  }
  FILE_END_OF_FILE_INFO eof{};
  eof.EndOfFile.QuadPart = length;
  if (!SetFileInformationByHandle(d->handle, FileEndOfFileInfo, &eof, sizeof(eof))) {
    const DWORD error = GetLastError();
    return FailWith(error == ERROR_USER_MAPPED_FILE ? guest::kEBUSY : ErrnoFromWin32(error), -1);
  }
  return 0;
}

int FallocateImpl(int fd, int mode, std::int64_t offset, std::int64_t length) {
  DescriptionPtr d = FindDescription(fd);
  if (d == nullptr) return guest::kEBADF;
  if (offset < 0 || length <= 0) return guest::kEINVAL;
  if (offset > INT64_MAX - length) return guest::kEFBIG;
  if (IsDirectoryDescription(*d)) return guest::kEISDIR;
  if (d->provider == Provider::kHandle && (d->kind == vfs::FdKind::kPipe || d->kind == vfs::FdKind::kSocket)) {
    return guest::kESPIPE;
  }
  if (d->provider != Provider::kHandle || d->kind != vfs::FdKind::kFile) return guest::kENODEV;
  if (AccessMode(*d) == guest::kO_RDONLY || IsPathOnly(*d)) return guest::kEBADF;
  const int supported = kFALLOC_FL_KEEP_SIZE | kFALLOC_FL_PUNCH_HOLE | 0x10 ;
  if ((mode & ~supported) != 0) return guest::kEOPNOTSUPP;
  if ((mode & kFALLOC_FL_PUNCH_HOLE) != 0 && (mode & kFALLOC_FL_KEEP_SIZE) == 0) return guest::kEOPNOTSUPP;
  const std::int64_t end = offset + length;
  LARGE_INTEGER size;
  if (!GetFileSizeEx(d->handle, &size)) return ErrnoFromWin32(GetLastError());
  if ((mode & (kFALLOC_FL_PUNCH_HOLE | 0x10)) != 0) {
    FILE_ZERO_DATA_INFORMATION zero{};
    zero.FileOffset.QuadPart = offset;
    zero.BeyondFinalZero.QuadPart = std::min<std::int64_t>(end, size.QuadPart);
    DWORD returned = 0;
    if (zero.BeyondFinalZero.QuadPart > zero.FileOffset.QuadPart &&
        !DeviceIoControl(d->handle, FSCTL_SET_ZERO_DATA, &zero, sizeof(zero), nullptr, 0, &returned, nullptr)) {
      return ErrnoFromWin32(GetLastError());
    }
  }
  if ((mode & kFALLOC_FL_KEEP_SIZE) == 0 && end > size.QuadPart) {
    FILE_END_OF_FILE_INFO eof{};
    eof.EndOfFile.QuadPart = end;
    if (!SetFileInformationByHandle(d->handle, FileEndOfFileInfo, &eof, sizeof(eof))) {
      const DWORD error = GetLastError();
      return error == ERROR_DISK_FULL ? guest::kENOSPC : ErrnoFromWin32(error);
    }
  }
  return 0;
}

struct VolumeInfo {
  std::uint64_t type = 0xEF53;
  std::uint64_t block = 4096;
  std::uint64_t blocks = 0, free = 0, available = 0;
  std::uint64_t serial = 0;
  std::uint64_t name_max = 255;
  std::uint64_t flags = 0;
};

constexpr std::uint64_t kStatfsValid = 0x20;

void LogProcVolumeQuery(const char* operation, bool reports_type, const VolumeInfo* volume, int error) noexcept {
  const DWORD saved_last_error = GetLastError();
  const int saved_host_errno = errno;
  const int saved_guest_errno = GuestErrno();
  const long long utc_ms = DiagnosticUtcMillis();
  if (volume == nullptr) {
    std::fprintf(stderr, "[elixir][fs-metadata] operation=%s mount=/proc result=error errno=%d utc_ms=%lld\n",
                 operation, error, utc_ms);
  } else {
    const auto block_size = static_cast<unsigned long long>(volume->block);
    const auto name_max = static_cast<unsigned long long>(volume->name_max);
    if (reports_type) {
      std::fprintf(stderr,
                   "[elixir][fs-metadata] operation=%s mount=/proc result=ok type=0x%llx block_size=%llu"
                   " name_max=%llu flags=0x%llx utc_ms=%lld\n",
                   operation, static_cast<unsigned long long>(volume->type), block_size, name_max,
                   static_cast<unsigned long long>(volume->flags | kStatfsValid), utc_ms);
    } else {
      std::fprintf(stderr,
                   "[elixir][fs-metadata] operation=%s mount=/proc result=ok block_size=%llu"
                   " name_max=%llu flags=0x%llx utc_ms=%lld\n",
                   operation, block_size, name_max, static_cast<unsigned long long>(volume->flags), utc_ms);
    }
  }
  SetGuestErrno(saved_guest_errno);
  errno = saved_host_errno;
  SetLastError(saved_last_error);
}

bool HostVolume(const std::wstring& host, bool read_only, VolumeInfo* out, int* err) {
  wchar_t root[MAX_PATH + 8];
  if (!GetVolumePathNameW(host.c_str(), root, MAX_PATH + 8)) {
    *err = PathErrnoFromWin32(GetLastError(), host);
    return false;
  }
  ULARGE_INTEGER available{}, total{}, free{};
  if (!GetDiskFreeSpaceExW(root, &available, &total, &free)) {
    *err = ErrnoFromWin32(GetLastError());
    return false;
  }
  DWORD sectors_per_cluster = 0, bytes_per_sector = 0, free_clusters = 0, total_clusters = 0;
  if (GetDiskFreeSpaceW(root, &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters) &&
      sectors_per_cluster != 0 && bytes_per_sector != 0) {
    out->block = static_cast<std::uint64_t>(sectors_per_cluster) * bytes_per_sector;
  }
  DWORD serial = 0, max_component = 255, fs_flags = 0;
  GetVolumeInformationW(root, nullptr, 0, &serial, &max_component, &fs_flags, nullptr, 0);
  out->blocks = total.QuadPart / out->block;
  out->free = free.QuadPart / out->block;
  out->available = available.QuadPart / out->block;
  out->serial = serial;
  out->name_max = max_component;
  out->flags = kST_NOSUID | kST_NODEV;
  if (read_only || (fs_flags & FILE_READ_ONLY_VOLUME) != 0) out->flags |= kST_RDONLY;
  return true;
}

bool VolumeForResolved(const Resolved& r, VolumeInfo* out, int* err) {
  if (r.kind == NodeKind::kHost) {
    if (GetFileAttributesW(r.host.c_str()) == INVALID_FILE_ATTRIBUTES) {
      *err = PathErrnoFromWin32(GetLastError(), r.host);
      return false;
    }
    return HostVolume(r.host, r.read_only, out, err);
  }
  out->type = SynthFsType(r.guest);
  out->flags = kST_NOSUID | kST_NODEV | (r.kind == NodeKind::kVirtualDir ? kST_RDONLY : 0);
  out->serial = HashString(r.guest.substr(0, r.guest.find('/', 1)));
  return true;
}

bool VolumeForFd(int fd, VolumeInfo* out, int* err, bool* proc_query) {
  DescriptionPtr d = FindDescription(fd);
  if (d == nullptr) {
    *err = guest::kEBADF;
    return false;
  }
  *proc_query = IsProcMountPath(d->guest_path);
  if (d->provider == Provider::kHandle && (d->kind == vfs::FdKind::kFile || d->kind == vfs::FdKind::kDirectory)) {
    std::wstring host = d->host_path;
    if (host.empty() && d->handle != nullptr) {
      wchar_t buffer[1024];
      const DWORD length = GetFinalPathNameByHandleW(d->handle, buffer, 1024, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
      if (length > 0 && length < 1024) host.assign(buffer, length);
    }
    if (!host.empty()) return HostVolume(host, d->read_only_mount, out, err);
  }
  if (d->provider == Provider::kHandle && d->kind == vfs::FdKind::kPipe) {
    out->type = 0x50495045;
    out->flags = 0;
    return true;
  }
  if (d->provider == Provider::kHandle && d->kind == vfs::FdKind::kSocket) {
    out->type = 0x534F434B;
    out->flags = 0;
    return true;
  }
  out->type = SynthFsType(d->guest_path);
  out->flags = kST_NOSUID | kST_NODEV;
  return true;
}

void FillStatfs(const VolumeInfo& v, guest::Statfs* out) {
  std::memset(out, 0, sizeof(*out));
  out->f_type = v.type;
  out->f_bsize = v.block;
  out->f_blocks = v.blocks;
  out->f_bfree = v.free;
  out->f_bavail = v.available;
  out->f_files = v.blocks;
  out->f_ffree = v.free;
  out->f_fsid[0] = static_cast<std::int32_t>(v.serial);
  out->f_fsid[1] = static_cast<std::int32_t>(v.serial >> 32);
  out->f_namelen = v.name_max;
  out->f_frsize = v.block;
  out->f_flags = v.flags | kStatfsValid;
}

void FillStatvfs(const VolumeInfo& v, guest::Statvfs* out) {
  std::memset(out, 0, sizeof(*out));
  out->f_bsize = v.block;
  out->f_frsize = v.block;
  out->f_blocks = v.blocks;
  out->f_bfree = v.free;
  out->f_bavail = v.available;
  out->f_files = v.blocks;
  out->f_ffree = v.free;
  out->f_favail = v.free;
  out->f_fsid = v.serial;
  out->f_flag = v.flags;
  out->f_namemax = v.name_max;
}

template <typename Out, typename Fill>
int FinishVolumeQuery(const char* operation, bool proc_query, bool ok, int error, const VolumeInfo& volume,
                      Out* out, Fill fill) {
  if (ok) fill(volume, out);
  if (proc_query) LogProcVolumeQuery(operation, std::is_same_v<Out, guest::Statfs>, ok ? &volume : nullptr, error);
  return ok ? 0 : FailWith(error, -1);
}

template <typename Out, typename Fill>
int StatfsPath(const char* operation, const char* path, Out* out, Fill fill) {
  if (out == nullptr) return FailWith(guest::kEFAULT, -1);
  Resolved r;
  SynthNode node;
  VolumeInfo v;
  int err = 0;
  bool proc_query = false;
  const bool ok = ResolveMeta(guest::kAT_FDCWD, path, true, &r, &node, &err, &proc_query) &&
                  VolumeForResolved(r, &v, &err);
  if (!ok) RecordAppStorageQuery(r, operation, err);
  return FinishVolumeQuery(operation, proc_query, ok, err, v, out, fill);
}

template <typename Out, typename Fill>
int StatfsFd(const char* operation, int fd, Out* out, Fill fill) {
  if (out == nullptr) return FailWith(guest::kEFAULT, -1);
  VolumeInfo v;
  int err = 0;
  bool proc_query = false;
  const bool ok = VolumeForFd(fd, &v, &err, &proc_query);
  return FinishVolumeQuery(operation, proc_query, ok, err, v, out, fill);
}

abi::g_long PathconfValue(int name) {
  switch (name) {
    case guest::k_PC_FILESIZEBITS: return 64;
    case guest::k_PC_LINK_MAX: return 1024;
    case guest::k_PC_MAX_CANON: return 255;
    case guest::k_PC_MAX_INPUT: return 255;
    case guest::k_PC_NAME_MAX: return 255;
    case guest::k_PC_PATH_MAX: return 4096;
    case guest::k_PC_PIPE_BUF: return 4096;
    case guest::k_PC_CHOWN_RESTRICTED: return 1;
    case guest::k_PC_NO_TRUNC: return 1;
    case guest::k_PC_VDISABLE: return 0;
    case guest::k_PC_ALLOC_SIZE_MIN:
    case guest::k_PC_REC_INCR_XFER_SIZE:
    case guest::k_PC_REC_MIN_XFER_SIZE:
    case guest::k_PC_REC_XFER_ALIGN: return 4096;
    case guest::k_PC_REC_MAX_XFER_SIZE:
    case guest::k_PC_SYMLINK_MAX:
    case guest::k_PC_ASYNC_IO:
    case guest::k_PC_PRIO_IO:
    case guest::k_PC_SYNC_IO: return -1;
    default: return FailWith(guest::kEINVAL, static_cast<abi::g_long>(-1));
  }
}

}

HANDLE OpenForAttributes(const std::wstring& host, bool follow, DWORD extra_access) noexcept {
  return CreateFileW(host.c_str(), FILE_READ_ATTRIBUTES | extra_access, kShareAll, nullptr, OPEN_EXISTING,
                     FILE_FLAG_BACKUP_SEMANTICS | (follow ? 0 : FILE_FLAG_OPEN_REPARSE_POINT), nullptr);
}

std::uint32_t CurrentUmask() noexcept { return g_umask.load(); }

void FillStatFromInfo(const HostFileInfo& info, std::string_view name_hint, guest::Stat* st) noexcept {
  std::memset(st, 0, sizeof(*st));
  const bool is_link = (info.attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && IsSymlinkTag(info.reparse_tag);
  const bool is_dir = (info.attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  std::uint32_t mode;
  if (is_link) {
    mode = static_cast<std::uint32_t>(guest::kS_IFLNK) | 0777;
  } else if (is_dir) {
    mode = static_cast<std::uint32_t>(guest::kS_IFDIR) | 0755;
  } else {
    mode = static_cast<std::uint32_t>(guest::kS_IFREG) | ((info.attributes & FILE_ATTRIBUTE_READONLY) != 0 ? 0444 : 0644);
    if (HasExecutableSuffix(name_hint)) mode |= 0111;
  }
  st->st_dev = info.volume_serial;
  st->st_ino = info.file_id;
  st->st_nlink = info.links == 0 ? 1 : info.links;
  st->st_mode = mode;
  st->st_uid = kGuestUid;
  st->st_gid = kGuestGid;
  st->st_rdev = 0;
  st->st_size = is_dir ? 4096 : info.size;
  st->st_blksize = 4096;
  st->st_blocks = (info.allocation + 511) / 512;
  FileTimeToTimespec(info.access_time, &st->st_atim);
  FileTimeToTimespec(info.write_time, &st->st_mtim);
  FileTimeToTimespec(info.change_time, &st->st_ctim);
}

void FillSynthStat(const SynthNode& node, guest::Stat* st) noexcept {
  std::uint32_t type = static_cast<std::uint32_t>(guest::kS_IFREG);
  switch (node.type) {
    case SynthType::kDir: type = static_cast<std::uint32_t>(guest::kS_IFDIR); break;
    case SynthType::kNull:
    case SynthType::kZero:
    case SynthType::kRandom: type = static_cast<std::uint32_t>(guest::kS_IFCHR); break;
    case SynthType::kLink:
    case SynthType::kFdAlias: type = static_cast<std::uint32_t>(guest::kS_IFLNK); break;
    default: break;
  }
  FillSpecial(st, type | node.mode, node.rdev, node.ino, node.dev);
  st->st_size = node.size;
  if (node.type == SynthType::kDir) st->st_nlink = 2;
}

void FillDirStat(const std::string& guest, guest::Stat* st) noexcept {
  FillSpecial(st, static_cast<std::uint32_t>(guest::kS_IFDIR) | 0755, 0, HashString(guest) | 1, MakeDev(0, 1));
  st->st_nlink = 2;
  st->st_size = 4096;
}

bool StatResolved(const Resolved& resolved, bool follow, guest::Stat* st, int* err) {
  Resolved r = resolved;
  if (r.kind == NodeKind::kSynthetic) {
    SynthNode node;
    if (!SynthLookup(r.guest, &node)) {
      *err = guest::kENOENT;
      return false;
    }
    if (follow && node.type == SynthType::kFdAlias) {
      DescriptionPtr d = FindDescription(node.alias_fd);
      if (d == nullptr) {
        *err = guest::kENOENT;
        return false;
      }
      return DescStat(*d, st, err);
    }
    if (follow && node.type == SynthType::kLink) {
      if (!FollowSynthetic(&r, &node, err)) return false;
      if (r.kind != NodeKind::kSynthetic) return StatResolved(r, true, st, err);
      if (node.type == SynthType::kFdAlias) return StatResolved(r, true, st, err);
    }
    if (r.trailing_slash && node.type != SynthType::kDir && !(follow == false && node.type == SynthType::kLink)) {
      *err = guest::kENOTDIR;
      return false;
    }
    FillSynthStat(node, st);
    return true;
  }
  if (r.kind == NodeKind::kVirtualDir) {
    FillDirStat(r.guest, st);
    return true;
  }
  return StatHostPath(r, follow, st, err);
}

bool DescStat(Description& d, guest::Stat* st, int* err) {
  switch (d.provider) {
    case Provider::kMemory:
    case Provider::kZero:
    case Provider::kRandom: {
      SynthNode node;
      if (SynthLookup(d.guest_path, &node)) {
        FillSynthStat(node, st);
      } else {
        FillSpecial(st, static_cast<std::uint32_t>(guest::kS_IFREG) | 0444, 0, PseudoInode(&d), MakeDev(0, 22));
      }
      return true;
    }
    case Provider::kVirtualDir: {
      SynthNode node;
      if (d.synthetic_dir && SynthLookup(d.guest_path, &node)) {
        FillSynthStat(node, st);
      } else {
        FillDirStat(d.guest_path, st);
      }
      return true;
    }
    case Provider::kHandle:
      break;
  }
  switch (d.kind) {
    case vfs::FdKind::kFile:
    case vfs::FdKind::kDirectory: {
      HostFileInfo info;
      if (!QueryHandleInfo(d.handle, &info)) {
        *err = ErrnoFromWin32(GetLastError());
        return false;
      }
      FillStatFromInfo(info, d.guest_path, st);
      return true;
    }
    case vfs::FdKind::kPipe:
      FillSpecial(st, static_cast<std::uint32_t>(guest::kS_IFIFO) | 0600, 0, PseudoInode(&d), MakeDev(0, 12));
      return true;
    case vfs::FdKind::kConsole:
      FillSpecial(st, static_cast<std::uint32_t>(guest::kS_IFCHR) | 0620, MakeDev(136, 0), PseudoInode(&d),
                  MakeDev(0, 24));
      return true;
    case vfs::FdKind::kSocket:
      FillSpecial(st, static_cast<std::uint32_t>(guest::kS_IFSOCK) | 0777, 0, PseudoInode(&d), MakeDev(0, 8));
      return true;
    case vfs::FdKind::kNull: {
      SynthNode node;
      if (SynthLookup("/dev/null", &node)) FillSynthStat(node, st);
      return true;
    }
    case vfs::FdKind::kEvent:
      FillSpecial(st, 0600, 0, PseudoInode(&d), MakeDev(0, 13));
      return true;
  }
  *err = guest::kEBADF;
  return false;
}

ELIXIR_BIONIC_FN(int, stat)(const char* path, guest::Stat* st) noexcept {
  return Guarded(-1, [&] { return StatAt(guest::kAT_FDCWD, path, st, 0); });
}
ELIXIR_BIONIC_FN(int, lstat)(const char* path, guest::Stat* st) noexcept {
  return Guarded(-1, [&] { return StatAt(guest::kAT_FDCWD, path, st, guest::kAT_SYMLINK_NOFOLLOW); });
}
ELIXIR_BIONIC_FN(int, fstatat)(int dirfd, const char* path, guest::Stat* st, int flags) noexcept {
  return Guarded(-1, [&] { return StatAt(dirfd, path, st, flags); });
}
ELIXIR_BIONIC_FN(int, fstat)(int fd, guest::Stat* st) noexcept {
  return Guarded(-1, [&] {
    if (st == nullptr) return FailWith(guest::kEFAULT, -1);
    DescriptionPtr d = GetDescription(fd);
    if (d == nullptr) return -1;
    int err = 0;
    if (!DescStat(*d, st, &err)) return FailWith(err, -1);
    return 0;
  });
}
ELIXIR_BIONIC_FN(int, statx)(int dirfd, const char* path, int flags, std::uint32_t mask, GuestStatx* out) noexcept {
  return Guarded(-1, [&] {
    if (out == nullptr || path == nullptr) return FailWith(guest::kEFAULT, -1);
    guest::Stat st;
    if (StatAt(dirfd, path, &st, flags) != 0) return -1;
    std::memset(out, 0, sizeof(*out));
    out->stx_mask = kSTATX_BASIC_STATS;
    out->stx_blksize = static_cast<std::uint32_t>(st.st_blksize);
    out->stx_nlink = static_cast<std::uint32_t>(st.st_nlink);
    out->stx_uid = st.st_uid;
    out->stx_gid = st.st_gid;
    out->stx_mode = static_cast<std::uint16_t>(st.st_mode);
    out->stx_ino = st.st_ino;
    out->stx_size = static_cast<std::uint64_t>(st.st_size);
    out->stx_blocks = static_cast<std::uint64_t>(st.st_blocks);
    auto ts = [](const guest::Timespec& t) {
      GuestStatxTimestamp r{};
      r.tv_sec = t.tv_sec;
      r.tv_nsec = static_cast<std::uint32_t>(t.tv_nsec);
      return r;
    };
    out->stx_atime = ts(st.st_atim);
    out->stx_mtime = ts(st.st_mtim);
    out->stx_ctime = ts(st.st_ctim);
    auto major = [](std::uint64_t dev) {
      return static_cast<std::uint32_t>(((dev >> 8) & 0xfff) | ((dev >> 32) & ~0xfffull));
    };
    auto minor = [](std::uint64_t dev) {
      return static_cast<std::uint32_t>((dev & 0xff) | ((dev >> 12) & ~0xffull));
    };
    out->stx_rdev_major = major(st.st_rdev);
    out->stx_rdev_minor = minor(st.st_rdev);
    out->stx_dev_major = major(st.st_dev);
    out->stx_dev_minor = minor(st.st_dev);
    return 0;
  });
}
ELIXIR_BIONIC_FN(int, access)(const char* path, int mode) noexcept {
  return Guarded(-1, [&] { return AccessAt(guest::kAT_FDCWD, path, mode, 0); });
}
ELIXIR_BIONIC_FN(int, faccessat)(int dirfd, const char* path, int mode, int flags) noexcept {
  return Guarded(-1, [&] { return AccessAt(dirfd, path, mode, flags); });
}
ELIXIR_BIONIC_FN(int, chmod)(const char* path, std::uint32_t mode) noexcept {
  return Guarded(-1, [&] { return ChmodAt(guest::kAT_FDCWD, path, mode, true); });
}
ELIXIR_BIONIC_FN(int, lchmod)(const char* path, std::uint32_t mode) noexcept {
  return Guarded(-1, [&] { return ChmodAt(guest::kAT_FDCWD, path, mode, false); });
}
ELIXIR_BIONIC_FN(int, fchmodat)(int dirfd, const char* path, std::uint32_t mode, int flags) noexcept {
  return Guarded(-1, [&] {
    if ((flags & ~guest::kAT_SYMLINK_NOFOLLOW) != 0) return FailWith(guest::kEINVAL, -1);
    return ChmodAt(dirfd, path, mode, (flags & guest::kAT_SYMLINK_NOFOLLOW) == 0);
  });
}
ELIXIR_BIONIC_FN(int, fchmod)(int fd, std::uint32_t mode) noexcept {
  return Guarded(-1, [&] {
    DescriptionPtr d = GetDescription(fd);
    if (d == nullptr) return -1;
    if (d->provider == Provider::kHandle &&
        (d->kind == vfs::FdKind::kPipe || d->kind == vfs::FdKind::kSocket || d->kind == vfs::FdKind::kConsole)) {
      return 0;
    }
    const int error = WithWritableAttributes(*d, [&](HANDLE h) { return SetReadOnly(h, (mode & 0222) == 0); });
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, chown)(const char* path, std::uint32_t uid, std::uint32_t gid) noexcept {
  return Guarded(-1, [&] { return ChownAt(guest::kAT_FDCWD, path, 0); });
}
ELIXIR_BIONIC_FN(int, lchown)(const char* path, std::uint32_t uid, std::uint32_t gid) noexcept {
  return Guarded(-1, [&] { return ChownAt(guest::kAT_FDCWD, path, guest::kAT_SYMLINK_NOFOLLOW); });
}
ELIXIR_BIONIC_FN(int, fchownat)(int dirfd, const char* path, std::uint32_t uid, std::uint32_t gid, int flags) noexcept {
  return Guarded(-1, [&] { return ChownAt(dirfd, path, flags); });
}
ELIXIR_BIONIC_FN(int, fchown)(int fd, std::uint32_t uid, std::uint32_t gid) noexcept {
  return Guarded(-1, [&] { return GetDescription(fd) == nullptr ? -1 : 0; });
}
ELIXIR_BIONIC_FN(int, utimensat)(int dirfd, const char* path, const guest::Timespec* times, int flags) noexcept {
  return Guarded(-1, [&] { return UtimensAt(dirfd, path, times, flags); });
}
ELIXIR_BIONIC_FN(int, futimens)(int fd, const guest::Timespec* times) noexcept {
  return Guarded(-1, [&] { return UtimensAt(fd, nullptr, times, 0); });
}
ELIXIR_BIONIC_FN(int, utimes)(const char* path, const guest::Timeval* tv) noexcept {
  return Guarded(-1, [&] {
    guest::Timespec ts[2];
    if (tv != nullptr && !TimevalsToTimespecs(tv, ts)) return FailWith(guest::kEINVAL, -1);
    return UtimensAt(guest::kAT_FDCWD, path, tv != nullptr ? ts : nullptr, 0);
  });
}
ELIXIR_BIONIC_FN(int, lutimes)(const char* path, const guest::Timeval* tv) noexcept {
  return Guarded(-1, [&] {
    guest::Timespec ts[2];
    if (tv != nullptr && !TimevalsToTimespecs(tv, ts)) return FailWith(guest::kEINVAL, -1);
    return UtimensAt(guest::kAT_FDCWD, path, tv != nullptr ? ts : nullptr, guest::kAT_SYMLINK_NOFOLLOW);
  });
}
ELIXIR_BIONIC_FN(int, futimes)(int fd, const guest::Timeval* tv) noexcept {
  return Guarded(-1, [&] {
    guest::Timespec ts[2];
    if (tv != nullptr && !TimevalsToTimespecs(tv, ts)) return FailWith(guest::kEINVAL, -1);
    return UtimensAt(fd, nullptr, tv != nullptr ? ts : nullptr, 0);
  });
}
ELIXIR_BIONIC_FN(int, futimesat)(int dirfd, const char* path, const guest::Timeval* tv) noexcept {
  return Guarded(-1, [&] {
    guest::Timespec ts[2];
    if (tv != nullptr && !TimevalsToTimespecs(tv, ts)) return FailWith(guest::kEINVAL, -1);
    if (path == nullptr) return UtimensAt(dirfd, nullptr, tv != nullptr ? ts : nullptr, 0);
    return UtimensAt(dirfd, path, tv != nullptr ? ts : nullptr, 0);
  });
}
ELIXIR_BIONIC_FN(int, utime)(const char* path, const GuestUtimbuf* times) noexcept {
  return Guarded(-1, [&] {
    guest::Timespec ts[2];
    if (times != nullptr) {
      ts[0] = {times->actime, 0};
      ts[1] = {times->modtime, 0};
    }
    return UtimensAt(guest::kAT_FDCWD, path, times != nullptr ? ts : nullptr, 0);
  });
}
ELIXIR_BIONIC_FN(int, truncate)(const char* path, abi::g_off length) noexcept {
  return Guarded(-1, [&] { return TruncatePath(path, length); });
}
ELIXIR_BIONIC_FN(int, ftruncate)(int fd, abi::g_off length) noexcept {
  return Guarded(-1, [&] { return TruncateFd(fd, length); });
}
ELIXIR_BIONIC_FN(int, fallocate)(int fd, int mode, abi::g_off offset, abi::g_off length) noexcept {
  return Guarded(-1, [&] {
    const int error = FallocateImpl(fd, mode, offset, length);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, posix_fallocate)(int fd, abi::g_off offset, abi::g_off length) noexcept {
  return Guarded(guest::kENOMEM, [&] { return FallocateImpl(fd, 0, offset, length); });
}
ELIXIR_BIONIC_FN(int, posix_fadvise)(int fd, abi::g_off offset, abi::g_off length, int advice) noexcept {
  return Guarded(guest::kENOMEM, [&] {
    DescriptionPtr d = FindDescription(fd);
    if (d == nullptr) return guest::kEBADF;
    if (d->provider == Provider::kHandle && d->kind == vfs::FdKind::kPipe) return guest::kESPIPE;
    if (advice < 0 || advice > 5 || length < 0) return guest::kEINVAL;
    return 0;
  });
}
ELIXIR_BIONIC_FN(int, statfs)(const char* path, guest::Statfs* out) noexcept {
  return Guarded(-1, [&] { return StatfsPath("statfs", path, out, FillStatfs); });
}
ELIXIR_BIONIC_FN(int, fstatfs)(int fd, guest::Statfs* out) noexcept {
  return Guarded(-1, [&] { return StatfsFd("fstatfs", fd, out, FillStatfs); });
}
ELIXIR_BIONIC_FN(int, statvfs)(const char* path, guest::Statvfs* out) noexcept {
  return Guarded(-1, [&] { return StatfsPath("statvfs", path, out, FillStatvfs); });
}
ELIXIR_BIONIC_FN(int, fstatvfs)(int fd, guest::Statvfs* out) noexcept {
  return Guarded(-1, [&] { return StatfsFd("fstatvfs", fd, out, FillStatvfs); });
}
ELIXIR_BIONIC_FN(std::uint32_t, umask)(std::uint32_t mask) noexcept {
  return g_umask.exchange(mask & 0777) & 0777;
}
ELIXIR_BIONIC_FN(std::uint32_t, __umask_chk)(std::uint32_t mask) noexcept {
  if ((mask & 0777) != mask) FortifyFatal("umask", "called with invalid mask");
  return elixir_bionic_umask(mask);
}
ELIXIR_BIONIC_FN(abi::g_long, pathconf)(const char* path, int name) noexcept {
  return Guarded<abi::g_long>(-1, [&]() -> abi::g_long {
    guest::Stat st;
    if (StatAt(guest::kAT_FDCWD, path, &st, 0) != 0) return -1;
    return PathconfValue(name);
  });
}
ELIXIR_BIONIC_FN(abi::g_long, fpathconf)(int fd, int name) noexcept {
  return Guarded<abi::g_long>(-1, [&]() -> abi::g_long {
    if (GetDescription(fd) == nullptr) return -1;
    return PathconfValue(name);
  });
}

void RegisterMetadataExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("stat", &elixir_bionic_stat);
  add("stat64", &elixir_bionic_stat);
  add("lstat", &elixir_bionic_lstat);
  add("lstat64", &elixir_bionic_lstat);
  add("fstat", &elixir_bionic_fstat);
  add("fstat64", &elixir_bionic_fstat);
  add("fstatat", &elixir_bionic_fstatat);
  add("fstatat64", &elixir_bionic_fstatat);
  add("statx", &elixir_bionic_statx);
  add("access", &elixir_bionic_access);
  add("faccessat", &elixir_bionic_faccessat);
  add("chmod", &elixir_bionic_chmod);
  add("lchmod", &elixir_bionic_lchmod);
  add("fchmod", &elixir_bionic_fchmod);
  add("fchmodat", &elixir_bionic_fchmodat);
  add("chown", &elixir_bionic_chown);
  add("lchown", &elixir_bionic_lchown);
  add("fchown", &elixir_bionic_fchown);
  add("fchownat", &elixir_bionic_fchownat);
  add("utimensat", &elixir_bionic_utimensat);
  add("futimens", &elixir_bionic_futimens);
  add("utimes", &elixir_bionic_utimes);
  add("lutimes", &elixir_bionic_lutimes);
  add("futimes", &elixir_bionic_futimes);
  add("futimesat", &elixir_bionic_futimesat);
  add("utime", &elixir_bionic_utime);
  add("truncate", &elixir_bionic_truncate);
  add("truncate64", &elixir_bionic_truncate);
  add("ftruncate", &elixir_bionic_ftruncate);
  add("ftruncate64", &elixir_bionic_ftruncate);
  add("fallocate", &elixir_bionic_fallocate);
  add("fallocate64", &elixir_bionic_fallocate);
  add("posix_fallocate", &elixir_bionic_posix_fallocate);
  add("posix_fallocate64", &elixir_bionic_posix_fallocate);
  add("posix_fadvise", &elixir_bionic_posix_fadvise);
  add("posix_fadvise64", &elixir_bionic_posix_fadvise);
  add("statfs", &elixir_bionic_statfs);
  add("statfs64", &elixir_bionic_statfs);
  add("fstatfs", &elixir_bionic_fstatfs);
  add("fstatfs64", &elixir_bionic_fstatfs);
  add("statvfs", &elixir_bionic_statvfs);
  add("statvfs64", &elixir_bionic_statvfs);
  add("fstatvfs", &elixir_bionic_fstatvfs);
  add("fstatvfs64", &elixir_bionic_fstatvfs);
  add("umask", &elixir_bionic_umask);
  add("__umask_chk", &elixir_bionic___umask_chk);
  add("pathconf", &elixir_bionic_pathconf);
  add("fpathconf", &elixir_bionic_fpathconf);
}

}
