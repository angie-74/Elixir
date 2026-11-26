// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "bionic/files/files_internal.h"

#include <windows.h>
#include <winioctl.h>

#include <algorithm>
#include <cstring>

#include "bionic/memory.h"

#ifndef FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE
#define FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE 0x00000010
#endif
#ifndef FILE_RENAME_FLAG_REPLACE_IF_EXISTS
#define FILE_RENAME_FLAG_REPLACE_IF_EXISTS 0x00000001
#define FILE_RENAME_FLAG_POSIX_SEMANTICS 0x00000002
#endif
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

namespace elixir::bionic::files {
namespace {

constexpr DWORD kShareAll = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
constexpr DWORD kRenameIgnoreReadonly = 0x00000040;

bool IsUnsupported(DWORD error) {
  return error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_SUPPORTED || error == ERROR_INVALID_FUNCTION ||
         error == ERROR_CALL_NOT_IMPLEMENTED;
}

struct ReparseBuffer {
  ULONG ReparseTag;
  USHORT ReparseDataLength;
  USHORT Reserved;
  union {
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG Flags;
      WCHAR PathBuffer[1];
    } SymbolicLink;
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR PathBuffer[1];
    } MountPoint;
    struct {
      UCHAR DataBuffer[1];
    } Generic;
  };
};

std::string HostTargetToGuest(std::wstring name) {
  if (name.rfind(L"\\??\\", 0) == 0) name = L"\\\\?\\" + name.substr(4);
  return vfs::HostToGuest(WideToUtf8(name));
}

struct HostAttributes {
  bool exists = false;
  bool directory = false;
  bool symlink = false;
  DWORD error = ERROR_SUCCESS;
};

HostAttributes Inspect(const std::wstring& host) {
  HostAttributes out;
  const DWORD attributes = GetFileAttributesW(host.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    out.error = GetLastError();
    return out;
  }
  out.exists = true;
  out.directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    HANDLE handle = OpenForAttributes(host, false);
    if (handle != INVALID_HANDLE_VALUE) {
      FILE_ATTRIBUTE_TAG_INFO tag{};
      if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag))) {
        out.symlink = IsSymlinkTag(tag.ReparseTag);
      }
      CloseHandle(handle);
    }
  }
  return out;
}

bool SameFile(const std::wstring& a, const std::wstring& b) {
  HANDLE first = OpenForAttributes(a, false);
  if (first == INVALID_HANDLE_VALUE) return false;
  HANDLE second = OpenForAttributes(b, false);
  if (second == INVALID_HANDLE_VALUE) {
    CloseHandle(first);
    return false;
  }
  HostFileInfo x, y;
  const bool ok = QueryHandleInfo(first, &x) && QueryHandleInfo(second, &y);
  CloseHandle(first);
  CloseHandle(second);
  return ok && x.volume_serial == y.volume_serial && x.file_id == y.file_id;
}

bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b) {
  return a.size() == b.size() &&
         CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) ==
             CSTR_EQUAL;
}

int RenameHost(const std::wstring& from, const std::wstring& to, bool replace) {
  HANDLE handle = CreateFileW(from.c_str(), DELETE | SYNCHRONIZE | FILE_READ_ATTRIBUTES, kShareAll, nullptr,
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return PathErrnoFromWin32(GetLastError(), from);
  const std::size_t bytes = offsetof(FILE_RENAME_INFO, FileName) + (to.size() + 1) * sizeof(wchar_t);
  std::vector<std::uint8_t> storage(bytes + 8, 0);
  auto* info = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
  info->RootDirectory = nullptr;
  info->FileNameLength = static_cast<DWORD>(to.size() * sizeof(wchar_t));
  std::memcpy(info->FileName, to.c_str(), (to.size() + 1) * sizeof(wchar_t));
  const DWORD base = (replace ? FILE_RENAME_FLAG_REPLACE_IF_EXISTS : 0) | FILE_RENAME_FLAG_POSIX_SEMANTICS;
  DWORD error = ERROR_SUCCESS;
  bool ok = false;
  for (DWORD flags : {base | kRenameIgnoreReadonly, base}) {
    info->Flags = flags;
    if (SetFileInformationByHandle(handle, FileRenameInfoEx, info, static_cast<DWORD>(bytes))) {
      ok = true;
      break;
    }
    error = GetLastError();
    if (!IsUnsupported(error)) break;
  }
  if (!ok && IsUnsupported(error)) {
    info->Flags = 0;
    info->ReplaceIfExists = replace ? TRUE : FALSE;
    if (SetFileInformationByHandle(handle, FileRenameInfo, info, static_cast<DWORD>(bytes))) {
      ok = true;
    } else {
      error = GetLastError();
    }
  }
  CloseHandle(handle);
  if (!ok && (IsUnsupported(error) || error == ERROR_ACCESS_DENIED)) {
    if (MoveFileExW(from.c_str(), to.c_str(), replace ? MOVEFILE_REPLACE_EXISTING : 0)) {
      ok = true;
    } else {
      error = GetLastError();
    }
  }
  if (ok) return 0;
  switch (error) {
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS: return guest::kEEXIST;
    case ERROR_NOT_SAME_DEVICE: return guest::kEXDEV;
    case ERROR_DIR_NOT_EMPTY: return guest::kENOTEMPTY;
    default: return PathErrnoFromWin32(error, to);
  }
}

bool IsGuestDescendant(const std::string& parent, const std::string& child) {
  if (parent == "/") return child != "/";
  return child.size() > parent.size() && child.compare(0, parent.size(), parent) == 0 && child[parent.size()] == '/';
}

int UnlinkResolved(const Resolved& r) {
  if (r.kind == NodeKind::kSynthetic) return guest::kEACCES;
  if (r.kind == NodeKind::kVirtualDir || r.mount_root) return guest::kEISDIR;
  const HostAttributes a = Inspect(r.host);
  if (!a.exists) return PathErrnoFromWin32(a.error, r.host);
  if (a.directory && !a.symlink) return guest::kEISDIR;
  if (r.trailing_slash) return guest::kENOTDIR;
  if (r.read_only) return guest::kEROFS;
  return DeleteHostPath(r.host, false);
}

int RmdirResolved(const Resolved& r) {
  if (r.last_is_dot) return guest::kEINVAL;
  if (r.kind == NodeKind::kSynthetic) {
    SynthNode node;
    return SynthLookup(r.guest, &node) && node.type == SynthType::kDir ? guest::kEBUSY : guest::kENOTDIR;
  }
  if (r.kind == NodeKind::kVirtualDir || r.mount_root) return guest::kEBUSY;
  const HostAttributes a = Inspect(r.host);
  if (!a.exists) return PathErrnoFromWin32(a.error, r.host);
  if (!a.directory || a.symlink) return guest::kENOTDIR;
  if (r.read_only) return guest::kEROFS;
  return DeleteHostPath(r.host, true);
}

int RenameAt(int old_dirfd, const char* old_path, int new_dirfd, const char* new_path, unsigned flags) {
  if (old_path == nullptr || new_path == nullptr) return guest::kEFAULT;
  if ((flags & ~static_cast<unsigned>(kRENAME_NOREPLACE | kRENAME_EXCHANGE | kRENAME_WHITEOUT)) != 0) return guest::kEINVAL;
  if ((flags & (kRENAME_EXCHANGE | kRENAME_WHITEOUT)) != 0) return guest::kEINVAL;
  if (old_path[0] == '\0' || new_path[0] == '\0') return guest::kENOENT;
  Resolved from, to;
  int err = 0;
  if (!ResolveAt(old_dirfd, old_path, &from, &err)) return err;
  if (!ResolveAt(new_dirfd, new_path, &to, &err)) return err;
  if (from.kind != NodeKind::kHost || to.kind != NodeKind::kHost) {
    return (from.kind == NodeKind::kHost) != (to.kind == NodeKind::kHost) ? guest::kEXDEV : guest::kEBUSY;
  }
  const auto report_failure = [&from, &to](int error) {
    RecordContentCacheIoFailure(ContentCacheOperation::kRename, error, from.guest, to.guest);
    return error;
  };
  if (from.mount_root || to.mount_root) return guest::kEBUSY;
  if (from.last_is_dot || to.last_is_dot) return guest::kEBUSY;
  const HostAttributes source = Inspect(from.host);
  if (!source.exists) return report_failure(PathErrnoFromWin32(source.error, from.host));
  const bool source_dir = source.directory && !source.symlink;
  if (!source_dir && (from.trailing_slash || to.trailing_slash)) return guest::kENOTDIR;
  if (from.read_only || to.read_only) return report_failure(guest::kEROFS);
  if (source_dir && IsGuestDescendant(from.guest, to.guest)) return guest::kEINVAL;
  const HostAttributes target = Inspect(to.host);
  if (target.exists) {
    if (SameFile(from.host, to.host)) {
      if (from.host == to.host || !EqualsIgnoreCase(from.host, to.host)) return 0;
    } else {
      if ((flags & kRENAME_NOREPLACE) != 0) return guest::kEEXIST;
      const bool target_dir = target.directory && !target.symlink;
      if (source_dir && !target_dir) return guest::kENOTDIR;
      if (!source_dir && target_dir) return guest::kEISDIR;
      if (target_dir) {
        const int removed = DeleteHostPath(to.host, true);
        if (removed != 0) return report_failure(removed);
      }
    }
  } else if (target.error == ERROR_PATH_NOT_FOUND) {
    return report_failure(MissingPathErrno(to.host));
  }
  const int rename_error = RenameHost(from.host, to.host, (flags & kRENAME_NOREPLACE) == 0);
  return rename_error == 0 ? 0 : report_failure(rename_error);
}

int LinkAt(int old_dirfd, const char* old_path, int new_dirfd, const char* new_path, int flags) {
  if (old_path == nullptr || new_path == nullptr) return guest::kEFAULT;
  if ((flags & ~(guest::kAT_SYMLINK_FOLLOW | guest::kAT_EMPTY_PATH)) != 0) return guest::kEINVAL;
  if (old_path[0] == '\0' || new_path[0] == '\0') return guest::kENOENT;
  Resolved from, to;
  int err = 0;
  if (!ResolveAt(old_dirfd, old_path, &from, &err)) return err;
  if (!ResolveAt(new_dirfd, new_path, &to, &err)) return err;
  if (to.kind != NodeKind::kHost) return guest::kEEXIST;
  if (from.kind != NodeKind::kHost) return guest::kEXDEV;
  const HostAttributes source = Inspect(from.host);
  if (!source.exists) return PathErrnoFromWin32(source.error, from.host);
  if (source.directory && !source.symlink) return guest::kEPERM;
  if (from.trailing_slash) return guest::kENOTDIR;
  const HostAttributes target = Inspect(to.host);
  if (target.exists) return guest::kEEXIST;
  if (target.error == ERROR_PATH_NOT_FOUND) return MissingPathErrno(to.host);
  if (from.read_only || to.read_only) return guest::kEROFS;
  std::wstring existing = from.host;
  if ((flags & guest::kAT_SYMLINK_FOLLOW) != 0 && source.symlink) {
    HANDLE handle = OpenForAttributes(from.host, true);
    if (handle == INVALID_HANDLE_VALUE) return PathErrnoFromWin32(GetLastError(), from.host);
    wchar_t buffer[2048];
    const DWORD length = GetFinalPathNameByHandleW(handle, buffer, 2048, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(handle);
    if (length == 0 || length >= 2048) return guest::kENAMETOOLONG;
    existing.assign(buffer, length);
  }
  if (!CreateHardLinkW(to.host.c_str(), existing.c_str(), nullptr)) {
    const DWORD error = GetLastError();
    switch (error) {
      case ERROR_ALREADY_EXISTS:
      case ERROR_FILE_EXISTS: return guest::kEEXIST;
      case ERROR_NOT_SAME_DEVICE: return guest::kEXDEV;
      case ERROR_TOO_MANY_LINKS: return guest::kEMLINK;
      case ERROR_INVALID_FUNCTION:
      case ERROR_NOT_SUPPORTED: return guest::kEPERM;
      default: return PathErrnoFromWin32(error, to.host);
    }
  }
  return 0;
}

int SymlinkAt(const char* target, int new_dirfd, const char* link_path) {
  if (target == nullptr || link_path == nullptr) return guest::kEFAULT;
  if (target[0] == '\0' || link_path[0] == '\0') return guest::kENOENT;
  Resolved link;
  int err = 0;
  if (!ResolveAt(new_dirfd, link_path, &link, &err)) return err;
  if (link.kind != NodeKind::kHost || link.mount_root) return guest::kEEXIST;
  if (link.read_only) return guest::kEROFS;
  const HostAttributes existing = Inspect(link.host);
  if (existing.exists) return guest::kEEXIST;
  if (existing.error == ERROR_PATH_NOT_FOUND) return MissingPathErrno(link.host);
  const std::string_view text(target);
  std::wstring host_target;
  bool directory = false;
  const std::string link_parent = link.guest.substr(0, std::max<std::size_t>(1, link.guest.rfind('/')));
  Resolved pointee;
  int ignored = 0;
  const bool resolvable = ResolveFrom(link_parent, text, &pointee, &ignored) && pointee.kind == NodeKind::kHost;
  if (resolvable) {
    const DWORD attributes = GetFileAttributesW(pointee.host.c_str());
    directory = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  }
  const bool absolute = text[0] == '/' || (text.size() >= 3 && text[1] == ':' && (text[2] == '/' || text[2] == '\\'));
  if (absolute) {
    if (!resolvable) return guest::kEPERM;
    host_target = StripLongPrefixPath(pointee.host);
    if (host_target.size() >= MAX_PATH) host_target = pointee.host;
  } else {
    std::size_t i = 0;
    while (i <= text.size()) {
      std::size_t j = text.find('/', i);
      if (j == std::string_view::npos) j = text.size();
      if (j > i) {
        if (!host_target.empty()) host_target.push_back(L'\\');
        AppendEscapedComponent(&host_target, text.substr(i, j - i));
      }
      i = j + 1;
    }
    if (host_target.empty()) host_target = L".";
  }
  DWORD link_flags = directory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
  if (CreateSymbolicLinkW(link.host.c_str(), host_target.c_str(), link_flags | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
    return 0;
  }
  DWORD error = GetLastError();
  if (error == ERROR_INVALID_PARAMETER) {
    if (CreateSymbolicLinkW(link.host.c_str(), host_target.c_str(), link_flags)) return 0;
    error = GetLastError();
  }
  switch (error) {
    case ERROR_PRIVILEGE_NOT_HELD: return guest::kEPERM;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS: return guest::kEEXIST;
    case ERROR_INVALID_FUNCTION:
    case ERROR_NOT_SUPPORTED: return guest::kEPERM;
    default: return PathErrnoFromWin32(error, link.host);
  }
}

bool ReadLinkAt(int dirfd, const char* path, std::string* out, int* err) {
  if (path == nullptr) {
    *err = guest::kEFAULT;
    return false;
  }
  if (path[0] == '\0') {
    *err = guest::kENOENT;
    return false;
  }
  Resolved r;
  if (!ResolveAt(dirfd, path, &r, err)) return false;
  if (r.kind == NodeKind::kSynthetic) {
    SynthNode node;
    if (!SynthLookup(r.guest, &node)) {
      *err = guest::kENOENT;
      return false;
    }
    if (node.type == SynthType::kLink) {
      *out = node.link_target;
      return true;
    }
    if (node.type == SynthType::kFdAlias) {
      *out = FdTargetPath(node.alias_fd);
      return true;
    }
    *err = guest::kEINVAL;
    return false;
  }
  if (r.kind == NodeKind::kVirtualDir) {
    *err = guest::kEINVAL;
    return false;
  }
  return ReadHostLink(r.host, out, err);
}

abi::g_ssize CopyLink(const std::string& text, char* buffer, abi::g_size size) {
  if (buffer == nullptr) return FailWith(guest::kEFAULT, static_cast<abi::g_ssize>(-1));
  const std::size_t n = std::min<std::size_t>(text.size(), size);
  std::memcpy(buffer, text.data(), n);
  return static_cast<abi::g_ssize>(n);
}

abi::g_ssize ReadLinkImpl(int dirfd, const char* path, char* buffer, abi::g_size size) {
  if (static_cast<abi::g_ssize>(size) <= 0) return FailWith(guest::kEINVAL, static_cast<abi::g_ssize>(-1));
  std::string text;
  int err = 0;
  if (!ReadLinkAt(dirfd, path, &text, &err)) return FailWith(err, static_cast<abi::g_ssize>(-1));
  return CopyLink(text, buffer, size);
}

bool RealPath(const char* path, std::string* out, int* err) {
  if (path == nullptr) {
    *err = guest::kEINVAL;
    return false;
  }
  if (path[0] == '\0') {
    *err = guest::kENOENT;
    return false;
  }
  Resolved r;
  if (!Resolve(path, &r, err)) return false;
  if (r.kind == NodeKind::kSynthetic) {
    SynthNode node;
    if (!FollowSynthetic(&r, &node, err)) return false;
    if (r.kind == NodeKind::kSynthetic) {
      if (node.type == SynthType::kFdAlias) {
        *out = FdTargetPath(node.alias_fd);
        return true;
      }
      if (r.trailing_slash && node.type != SynthType::kDir) {
        *err = guest::kENOTDIR;
        return false;
      }
      *out = r.guest;
      return true;
    }
  }
  if (r.kind == NodeKind::kVirtualDir) {
    *out = r.guest;
    return true;
  }
  HANDLE handle = OpenForAttributes(r.host, true);
  if (handle == INVALID_HANDLE_VALUE) {
    *err = PathErrnoFromWin32(GetLastError(), r.host);
    return false;
  }
  FILE_STANDARD_INFO standard{};
  GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard));
  std::wstring buffer(1024, L'\0');
  DWORD length = GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                                           FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (length >= buffer.size()) {
    buffer.resize(length + 1);
    length = GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                                       FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  }
  const DWORD error = GetLastError();
  CloseHandle(handle);
  if (r.trailing_slash && !standard.Directory) {
    *err = guest::kENOTDIR;
    return false;
  }
  if (length == 0 || length >= buffer.size()) {
    *err = ErrnoFromWin32(error);
    return false;
  }
  buffer.resize(length);
  *out = vfs::HostToGuest(WideToUtf8(buffer));
  return true;
}

int ChdirResolved(Resolved r) {
  if (r.kind == NodeKind::kSynthetic) {
    SynthNode node;
    int err = 0;
    if (!FollowSynthetic(&r, &node, &err)) return err;
    if (r.kind == NodeKind::kSynthetic) {
      if (node.type != SynthType::kDir) return guest::kENOTDIR;
      SetGuestCwd(r.guest);
      return 0;
    }
  }
  if (r.kind == NodeKind::kHost) {
    const DWORD attributes = GetFileAttributesW(r.host.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return PathErrnoFromWin32(GetLastError(), r.host);
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) return guest::kENOTDIR;
  }
  SetGuestCwd(r.guest);
  return 0;
}

char* CopyOut(const std::string& text, char* buffer, abi::g_size size, bool allocate_exact) {
  if (buffer == nullptr) {
    const std::size_t capacity = allocate_exact || size == 0 ? text.size() + 1 : size;
    if (text.size() + 1 > capacity) return FailWith(guest::kERANGE, static_cast<char*>(nullptr));
    auto* out = static_cast<char*>(GuestMalloc(capacity));
    if (out == nullptr) return FailWith(guest::kENOMEM, static_cast<char*>(nullptr));
    std::memcpy(out, text.c_str(), text.size() + 1);
    return out;
  }
  if (size == 0) return FailWith(guest::kEINVAL, static_cast<char*>(nullptr));
  if (text.size() + 1 > size) return FailWith(guest::kERANGE, static_cast<char*>(nullptr));
  std::memcpy(buffer, text.c_str(), text.size() + 1);
  return buffer;
}

constexpr char kTempChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

char* TemplateRun(char* templ, int suffix_length) {
  if (templ == nullptr || suffix_length < 0) return nullptr;
  const std::size_t length = std::strlen(templ);
  if (length < 6 + static_cast<std::size_t>(suffix_length)) return nullptr;
  char* run = templ + length - suffix_length - 6;
  for (int i = 0; i < 6; ++i) {
    if (run[i] != 'X') return nullptr;
  }
  return run;
}

void FillTemplate(char* run) {
  unsigned char bytes[6] = {};
  FillRandom(bytes, sizeof(bytes));
  for (int i = 0; i < 6; ++i) run[i] = kTempChars[bytes[i] % (sizeof(kTempChars) - 1)];
}

int MkstempImpl(char* templ, int suffix_length, int flags) {
  if ((flags & ~(guest::kO_APPEND | guest::kO_CLOEXEC | guest::kO_SYNC | guest::kO_DSYNC | guest::kO_LARGEFILE)) != 0) {
    return FailWith(guest::kEINVAL, -1);
  }
  char* run = TemplateRun(templ, suffix_length);
  if (run == nullptr) return FailWith(guest::kEINVAL, -1);
  for (int attempt = 0; attempt < 100; ++attempt) {
    FillTemplate(run);
    const int fd = OpenAt(guest::kAT_FDCWD, templ, guest::kO_RDWR | guest::kO_CREAT | guest::kO_EXCL | flags, 0600);
    if (fd >= 0) return fd;
    if (GuestErrno() != guest::kEEXIST) return -1;
  }
  return FailWith(guest::kEEXIST, -1);
}

int MkdirAt(int dirfd, const char* path, std::uint32_t mode) {
  Resolved r;
  int err = 0;
  if (path == nullptr) return FailWith(guest::kEFAULT, -1);
  if (path[0] == '\0') return FailWith(guest::kENOENT, -1);
  if (!ResolveAt(dirfd, path, &r, &err)) return FailWith(err, -1);
  if (r.kind != NodeKind::kHost) return FailWith(guest::kEEXIST, -1);
  if (r.read_only) {
    return FailWith(GetFileAttributesW(r.host.c_str()) != INVALID_FILE_ATTRIBUTES ? guest::kEEXIST : guest::kEROFS, -1);
  }
  if (!CreateDirectoryW(r.host.c_str(), nullptr)) {
    const DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS) return FailWith(guest::kEEXIST, -1);
    return FailWith(PathErrnoFromWin32(error, r.host), -1);
  }
  (void)mode;
  return 0;
}

}

int DeleteHostPath(const std::wstring& host, bool directory) {
  HANDLE handle = CreateFileW(host.c_str(), DELETE | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, kShareAll, nullptr,
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED) {
    handle = CreateFileW(host.c_str(), DELETE, kShareAll, nullptr, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  }
  if (handle == INVALID_HANDLE_VALUE) return PathErrnoFromWin32(GetLastError(), host);
  DWORD error = ERROR_SUCCESS;
  bool ok = false;
  const DWORD base = FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS;
  for (DWORD flags : {base | FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE, base}) {
    FILE_DISPOSITION_INFO_EX info{flags};
    if (SetFileInformationByHandle(handle, FileDispositionInfoEx, &info, sizeof(info))) {
      ok = true;
      break;
    }
    error = GetLastError();
    if (!IsUnsupported(error) && error != ERROR_ACCESS_DENIED) break;
  }
  if (!ok && (IsUnsupported(error) || error == ERROR_ACCESS_DENIED)) {
    FILE_BASIC_INFO basic{};
    if (GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic)) &&
        (basic.FileAttributes & FILE_ATTRIBUTE_READONLY) != 0) {
      FILE_BASIC_INFO update{};
      update.FileAttributes = basic.FileAttributes & ~FILE_ATTRIBUTE_READONLY;
      if (update.FileAttributes == 0) update.FileAttributes = FILE_ATTRIBUTE_NORMAL;
      SetFileInformationByHandle(handle, FileBasicInfo, &update, sizeof(update));
    }
    FILE_DISPOSITION_INFO legacy{TRUE};
    if (SetFileInformationByHandle(handle, FileDispositionInfo, &legacy, sizeof(legacy))) {
      ok = true;
    } else {
      error = GetLastError();
    }
  }
  CloseHandle(handle);
  if (ok) return 0;
  if (error == ERROR_DIR_NOT_EMPTY) return guest::kENOTEMPTY;
  if (directory && error == ERROR_DIRECTORY) return guest::kENOTDIR;
  return ErrnoFromWin32(error);
}

bool ReadHostLink(const std::wstring& host, std::string* target, int* err) {
  HANDLE handle = OpenForAttributes(host, false);
  if (handle == INVALID_HANDLE_VALUE) {
    *err = PathErrnoFromWin32(GetLastError(), host);
    return false;
  }
  std::vector<std::uint8_t> storage(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
  DWORD returned = 0;
  const BOOL ok = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, storage.data(),
                                  static_cast<DWORD>(storage.size()), &returned, nullptr);
  const DWORD error = GetLastError();
  CloseHandle(handle);
  if (!ok) {
    *err = error == ERROR_NOT_A_REPARSE_POINT ? guest::kEINVAL : ErrnoFromWin32(error);
    return false;
  }
  const auto* data = reinterpret_cast<const ReparseBuffer*>(storage.data());
  auto name_at = [&](const WCHAR* base, USHORT offset, USHORT length) {
    return std::wstring(base + offset / sizeof(WCHAR), length / sizeof(WCHAR));
  };
  if (data->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
    const auto& link = data->SymbolicLink;
    std::wstring print = name_at(link.PathBuffer, link.PrintNameOffset, link.PrintNameLength);
    std::wstring substitute = name_at(link.PathBuffer, link.SubstituteNameOffset, link.SubstituteNameLength);
    if ((link.Flags & 1) != 0) {
      std::string text = UnescapeHostName(print.empty() ? substitute : print);
      std::replace(text.begin(), text.end(), '\\', '/');
      *target = text;
      return true;
    }
    *target = HostTargetToGuest(substitute.empty() ? print : substitute);
    return true;
  }
  if (data->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
    const auto& mount = data->MountPoint;
    *target = HostTargetToGuest(name_at(mount.PathBuffer, mount.SubstituteNameOffset, mount.SubstituteNameLength));
    return true;
  }
  if (data->ReparseTag == 0xA000001DUL && data->ReparseDataLength > 4) {
    const auto* bytes = reinterpret_cast<const char*>(data->Generic.DataBuffer) + 4;
    target->assign(bytes, data->ReparseDataLength - 4u);
    return true;
  }
  *err = guest::kEINVAL;
  return false;
}

ELIXIR_BIONIC_FN(int, mkdir)(const char* path, std::uint32_t mode) noexcept {
  return Guarded(-1, [&] { return MkdirAt(guest::kAT_FDCWD, path, mode); });
}
ELIXIR_BIONIC_FN(int, mkdirat)(int dirfd, const char* path, std::uint32_t mode) noexcept {
  return Guarded(-1, [&] { return MkdirAt(dirfd, path, mode); });
}
ELIXIR_BIONIC_FN(int, rmdir)(const char* path) noexcept {
  return Guarded(-1, [&] {
    Resolved r;
    int err = 0;
    if (path == nullptr) return FailWith(guest::kEFAULT, -1);
    if (path[0] == '\0') return FailWith(guest::kENOENT, -1);
    if (!Resolve(path, &r, &err)) return FailWith(err, -1);
    const int error = RmdirResolved(r);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, unlink)(const char* path) noexcept {
  return Guarded(-1, [&] {
    Resolved r;
    int err = 0;
    if (path == nullptr) return FailWith(guest::kEFAULT, -1);
    if (path[0] == '\0') return FailWith(guest::kENOENT, -1);
    if (!Resolve(path, &r, &err)) return FailWith(err, -1);
    const int error = UnlinkResolved(r);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, unlinkat)(int dirfd, const char* path, int flags) noexcept {
  return Guarded(-1, [&] {
    if ((flags & ~guest::kAT_REMOVEDIR) != 0) return FailWith(guest::kEINVAL, -1);
    Resolved r;
    int err = 0;
    if (path == nullptr) return FailWith(guest::kEFAULT, -1);
    if (path[0] == '\0') return FailWith(guest::kENOENT, -1);
    if (!ResolveAt(dirfd, path, &r, &err)) return FailWith(err, -1);
    const int error = (flags & guest::kAT_REMOVEDIR) != 0 ? RmdirResolved(r) : UnlinkResolved(r);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, remove)(const char* path) noexcept {
  return Guarded(-1, [&] {
    Resolved r;
    int err = 0;
    if (path == nullptr) return FailWith(guest::kEFAULT, -1);
    if (path[0] == '\0') return FailWith(guest::kENOENT, -1);
    if (!Resolve(path, &r, &err)) return FailWith(err, -1);
    int error = UnlinkResolved(r);
    if (error == guest::kEISDIR) error = RmdirResolved(r);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, rename)(const char* old_path, const char* new_path) noexcept {
  return Guarded(-1, [&] {
    const int error = RenameAt(guest::kAT_FDCWD, old_path, guest::kAT_FDCWD, new_path, 0);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, renameat)(int old_dirfd, const char* old_path, int new_dirfd, const char* new_path) noexcept {
  return Guarded(-1, [&] {
    const int error = RenameAt(old_dirfd, old_path, new_dirfd, new_path, 0);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, renameat2)(int old_dirfd, const char* old_path, int new_dirfd, const char* new_path,
                                 std::uint32_t flags) noexcept {
  return Guarded(-1, [&] {
    const int error = RenameAt(old_dirfd, old_path, new_dirfd, new_path, flags);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, link)(const char* old_path, const char* new_path) noexcept {
  return Guarded(-1, [&] {
    const int error = LinkAt(guest::kAT_FDCWD, old_path, guest::kAT_FDCWD, new_path, 0);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, linkat)(int old_dirfd, const char* old_path, int new_dirfd, const char* new_path,
                              int flags) noexcept {
  return Guarded(-1, [&] {
    const int error = LinkAt(old_dirfd, old_path, new_dirfd, new_path, flags);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, symlink)(const char* target, const char* link_path) noexcept {
  return Guarded(-1, [&] {
    const int error = SymlinkAt(target, guest::kAT_FDCWD, link_path);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, symlinkat)(const char* target, int new_dirfd, const char* link_path) noexcept {
  return Guarded(-1, [&] {
    const int error = SymlinkAt(target, new_dirfd, link_path);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(abi::g_ssize, readlink)(const char* path, char* buffer, abi::g_size size) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return ReadLinkImpl(guest::kAT_FDCWD, path, buffer, size); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, readlinkat)(int dirfd, const char* path, char* buffer, abi::g_size size) noexcept {
  return Guarded<abi::g_ssize>(-1, [&] { return ReadLinkImpl(dirfd, path, buffer, size); });
}
ELIXIR_BIONIC_FN(abi::g_ssize, __readlink_chk)(const char* path, char* buffer, abi::g_size size,
                                               abi::g_size buffer_size) noexcept {
  if (size > buffer_size) FortifyFatal("readlink", "size > buffer size");
  return elixir_bionic_readlink(path, buffer, size);
}
ELIXIR_BIONIC_FN(abi::g_ssize, __readlinkat_chk)(int dirfd, const char* path, char* buffer, abi::g_size size,
                                                 abi::g_size buffer_size) noexcept {
  if (size > buffer_size) FortifyFatal("readlinkat", "size > buffer size");
  return elixir_bionic_readlinkat(dirfd, path, buffer, size);
}
ELIXIR_BIONIC_FN(char*, realpath)(const char* path, char* resolved) noexcept {
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    std::string text;
    int err = 0;
    if (!RealPath(path, &text, &err)) return FailWith(err, static_cast<char*>(nullptr));
    if (text.size() >= static_cast<std::size_t>(guest::kPATH_MAX)) {
      return FailWith(guest::kENAMETOOLONG, static_cast<char*>(nullptr));
    }
    if (resolved == nullptr) {
      char* out = GuestStrdup(text.c_str());
      return out != nullptr ? out : FailWith(guest::kENOMEM, static_cast<char*>(nullptr));
    }
    std::memcpy(resolved, text.c_str(), text.size() + 1);
    return resolved;
  });
}
ELIXIR_BIONIC_FN(char*, getcwd)(char* buffer, abi::g_size size) noexcept {
  return Guarded(static_cast<char*>(nullptr), [&] { return CopyOut(GuestCwd(), buffer, size, false); });
}
ELIXIR_BIONIC_FN(char*, __getcwd_chk)(char* buffer, abi::g_size size, abi::g_size buffer_size) noexcept {
  if (size > buffer_size) FortifyFatal("getcwd", "size > buffer size");
  return elixir_bionic_getcwd(buffer, size);
}
ELIXIR_BIONIC_FN(int, chdir)(const char* path) noexcept {
  return Guarded(-1, [&] {
    Resolved r;
    int err = 0;
    if (path == nullptr) return FailWith(guest::kEFAULT, -1);
    if (path[0] == '\0') return FailWith(guest::kENOENT, -1);
    if (!Resolve(path, &r, &err)) return FailWith(err, -1);
    const int error = ChdirResolved(std::move(r));
    return error == 0 ? 0 : FailWith(error, -1);
  });
}
ELIXIR_BIONIC_FN(int, fchdir)(int fd) noexcept {
  return Guarded(-1, [&] {
    DescriptionPtr d = GetDescription(fd);
    if (d == nullptr) return -1;
    if (!IsDirectoryDescription(*d)) return FailWith(guest::kENOTDIR, -1);
    std::string path = d->guest_path;
    if (path.empty()) path = FdTargetPath(fd);
    SetGuestCwd(path);
    return 0;
  });
}
ELIXIR_BIONIC_FN(int, mkstemp)(char* templ) noexcept {
  return Guarded(-1, [&] { return MkstempImpl(templ, 0, 0); });
}
ELIXIR_BIONIC_FN(int, mkostemp)(char* templ, int flags) noexcept {
  return Guarded(-1, [&] { return MkstempImpl(templ, 0, flags); });
}
ELIXIR_BIONIC_FN(int, mkstemps)(char* templ, int suffix_length) noexcept {
  return Guarded(-1, [&] { return MkstempImpl(templ, suffix_length, 0); });
}
ELIXIR_BIONIC_FN(int, mkostemps)(char* templ, int suffix_length, int flags) noexcept {
  return Guarded(-1, [&] { return MkstempImpl(templ, suffix_length, flags); });
}
ELIXIR_BIONIC_FN(char*, mkdtemp)(char* templ) noexcept {
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    char* run = TemplateRun(templ, 0);
    if (run == nullptr) return FailWith(guest::kEINVAL, static_cast<char*>(nullptr));
    for (int attempt = 0; attempt < 100; ++attempt) {
      FillTemplate(run);
      if (MkdirAt(guest::kAT_FDCWD, templ, 0700) == 0) return templ;
      if (GuestErrno() != guest::kEEXIST) return nullptr;
    }
    return FailWith(guest::kEEXIST, static_cast<char*>(nullptr));
  });
}
ELIXIR_BIONIC_FN(char*, mktemp)(char* templ) noexcept {
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    char* run = TemplateRun(templ, 0);
    if (run == nullptr) {
      if (templ != nullptr) templ[0] = '\0';
      return FailWith(guest::kEINVAL, templ);
    }
    for (int attempt = 0; attempt < 100; ++attempt) {
      FillTemplate(run);
      guest::Stat st;
      Resolved r;
      int err = 0;
      if (!Resolve(templ, &r, &err) || !StatResolved(r, false, &st, &err)) {
        if (err == guest::kENOENT) return templ;
        templ[0] = '\0';
        return FailWith(err, templ);
      }
    }
    templ[0] = '\0';
    return FailWith(guest::kEEXIST, templ);
  });
}
ELIXIR_BIONIC_FN(char*, tempnam)(const char* dir, const char* prefix) noexcept {
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    std::string base = dir != nullptr && dir[0] != '\0' ? dir : "/data/local/tmp";
    std::string name = base + "/" + (prefix != nullptr ? std::string(prefix).substr(0, 5) : std::string("tmp.")) +
                       "XXXXXX";
    std::vector<char> buffer(name.begin(), name.end());
    buffer.push_back('\0');
    if (elixir_bionic_mktemp(buffer.data()) == nullptr || buffer[0] == '\0') return nullptr;
    char* out = GuestStrdup(buffer.data());
    return out != nullptr ? out : FailWith(guest::kENOMEM, static_cast<char*>(nullptr));
  });
}
ELIXIR_BIONIC_FN(char*, tmpnam)(char* buffer) noexcept {
  static thread_local char storage[guest::kL_tmpnam];
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    char name[] = "/data/local/tmp/tmp.XXXXXX";
    if (elixir_bionic_mktemp(name) == nullptr || name[0] == '\0') return nullptr;
    char* out = buffer != nullptr ? buffer : storage;
    std::memcpy(out, name, sizeof(name));
    return out;
  });
}
ELIXIR_BIONIC_FN(int, mknodat)(int dirfd, const char* path, std::uint32_t mode, std::uint64_t dev) noexcept {
  return Guarded(-1, [&] {
    const std::uint32_t type = mode & static_cast<std::uint32_t>(guest::kS_IFMT);
    if (type != 0 && type != static_cast<std::uint32_t>(guest::kS_IFREG)) {
      return FailWith(guest::kEPERM, -1);
    }
    const int fd = OpenAt(dirfd, path, guest::kO_WRONLY | guest::kO_CREAT | guest::kO_EXCL, mode & 07777);
    if (fd < 0) return -1;
    CloseFd(fd);
    return 0;
  });
}
ELIXIR_BIONIC_FN(int, mknod)(const char* path, std::uint32_t mode, std::uint64_t dev) noexcept {
  return elixir_bionic_mknodat(guest::kAT_FDCWD, path, mode, dev);
}
ELIXIR_BIONIC_FN(int, mkfifoat)(int dirfd, const char* path, std::uint32_t mode) noexcept {
  return elixir_bionic_mknodat(dirfd, path, (mode & 07777) | static_cast<std::uint32_t>(guest::kS_IFIFO), 0);
}
ELIXIR_BIONIC_FN(int, mkfifo)(const char* path, std::uint32_t mode) noexcept {
  return elixir_bionic_mknodat(guest::kAT_FDCWD, path, (mode & 07777) | static_cast<std::uint32_t>(guest::kS_IFIFO), 0);
}

void RegisterPathExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("mkdir", &elixir_bionic_mkdir);
  add("mkdirat", &elixir_bionic_mkdirat);
  add("rmdir", &elixir_bionic_rmdir);
  add("unlink", &elixir_bionic_unlink);
  add("unlinkat", &elixir_bionic_unlinkat);
  add("remove", &elixir_bionic_remove);
  add("rename", &elixir_bionic_rename);
  add("renameat", &elixir_bionic_renameat);
  add("renameat2", &elixir_bionic_renameat2);
  add("link", &elixir_bionic_link);
  add("linkat", &elixir_bionic_linkat);
  add("symlink", &elixir_bionic_symlink);
  add("symlinkat", &elixir_bionic_symlinkat);
  add("readlink", &elixir_bionic_readlink);
  add("readlinkat", &elixir_bionic_readlinkat);
  add("__readlink_chk", &elixir_bionic___readlink_chk);
  add("__readlinkat_chk", &elixir_bionic___readlinkat_chk);
  add("realpath", &elixir_bionic_realpath);
  add("getcwd", &elixir_bionic_getcwd);
  add("__getcwd_chk", &elixir_bionic___getcwd_chk);
  add("chdir", &elixir_bionic_chdir);
  add("fchdir", &elixir_bionic_fchdir);
  add("mkstemp", &elixir_bionic_mkstemp);
  add("mkstemp64", &elixir_bionic_mkstemp);
  add("mkostemp", &elixir_bionic_mkostemp);
  add("mkostemp64", &elixir_bionic_mkostemp);
  add("mkstemps", &elixir_bionic_mkstemps);
  add("mkstemps64", &elixir_bionic_mkstemps);
  add("mkostemps", &elixir_bionic_mkostemps);
  add("mkostemps64", &elixir_bionic_mkostemps);
  add("mkdtemp", &elixir_bionic_mkdtemp);
  add("mktemp", &elixir_bionic_mktemp);
  add("tempnam", &elixir_bionic_tempnam);
  add("tmpnam", &elixir_bionic_tmpnam);
  add("mknod", &elixir_bionic_mknod);
  add("mknodat", &elixir_bionic_mknodat);
  add("mkfifo", &elixir_bionic_mkfifo);
  add("mkfifoat", &elixir_bionic_mkfifoat);
}

}
