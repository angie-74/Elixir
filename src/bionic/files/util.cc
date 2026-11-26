// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <bcrypt.h>

#include <cstdio>
#include <cstring>

#include "bionic/files/files_internal.h"

namespace elixir::bionic::files {

void FortifyFatal(const char* function, const char* what) noexcept {
  char message[256];
  std::snprintf(message, sizeof(message), "FORTIFY: %s: %s", function != nullptr ? function : "?",
                what != nullptr ? what : "prevented buffer overflow");
  HostLog(7 , "libc", message);
  RaiseFailFastException(nullptr, nullptr, 0);
  for (;;) TerminateProcess(GetCurrentProcess(), 134);
}

std::wstring Utf8ToWide(std::string_view text) {
  std::wstring out;
  if (text.empty()) return out;
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return out;
  out.resize(static_cast<std::size_t>(length));
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), length);
  return out;
}

std::string WideToUtf8(std::wstring_view text) {
  std::string out;
  if (text.empty()) return out;
  const int length =
      WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0) return out;
  out.resize(static_cast<std::size_t>(length));
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), length, nullptr, nullptr);
  return out;
}

namespace {

bool NeedsEscape(wchar_t c) {
  return (c >= 1 && c < 0x20) || c == L'"' || c == L'*' || c == L':' || c == L'<' || c == L'>' || c == L'?' ||
         c == L'\\' || c == L'|';
}

}

void AppendEscapedComponent(std::wstring* out, std::string_view component) {
  const std::wstring wide = Utf8ToWide(component);
  for (wchar_t c : wide) {
    out->push_back(NeedsEscape(c) ? static_cast<wchar_t>(0xF000 + c) : c);
  }
}

std::string UnescapeHostName(std::wstring_view name) {
  std::wstring plain(name);
  for (wchar_t& c : plain) {
    if (c > 0xF000 && c < 0xF080 && NeedsEscape(static_cast<wchar_t>(c - 0xF000))) c = static_cast<wchar_t>(c - 0xF000);
  }
  return WideToUtf8(plain);
}

namespace {
constexpr std::int64_t kUnixEpochAsFileTime = 116444736000000000LL;
constexpr std::int64_t kTicksPerSecond = 10000000LL;
}

void FileTimeToTimespec(std::int64_t filetime, guest::Timespec* out) noexcept {
  const std::int64_t ticks = filetime - kUnixEpochAsFileTime;
  std::int64_t seconds = ticks / kTicksPerSecond;
  std::int64_t remainder = ticks % kTicksPerSecond;
  if (remainder < 0) {
    remainder += kTicksPerSecond;
    --seconds;
  }
  out->tv_sec = seconds;
  out->tv_nsec = remainder * 100;
}

std::int64_t TimespecToFileTime(std::int64_t sec, std::int64_t nsec) noexcept {
  return sec * kTicksPerSecond + nsec / 100 + kUnixEpochAsFileTime;
}

std::int64_t NowFileTime() noexcept {
  FILETIME now;
  GetSystemTimePreciseAsFileTime(&now);
  return (static_cast<std::int64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
}

bool FillRandom(void* buffer, std::size_t length) noexcept {
  auto* bytes = static_cast<unsigned char*>(buffer);
  while (length > 0) {
    const ULONG chunk = length > 0x10000000u ? 0x10000000u : static_cast<ULONG>(length);
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, bytes, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) return false;
    bytes += chunk;
    length -= chunk;
  }
  return true;
}

std::uint64_t HashString(std::string_view text) noexcept {
  std::uint64_t hash = 1469598103934665603ull;
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ull;
  }
  return hash;
}

int MissingPathErrno(const std::wstring& host_path) {
  std::size_t end = host_path.size();
  std::size_t floor = 0;
  if (host_path.rfind(L"\\\\?\\UNC\\", 0) == 0) {
    floor = 8;
  } else if (host_path.rfind(L"\\\\?\\", 0) == 0) {
    floor = 4;
  }
  while (true) {
    const std::size_t slash = host_path.find_last_of(L'\\', end == 0 ? 0 : end - 1);
    if (slash == std::wstring::npos || slash < floor + 2) break;
    std::wstring parent = host_path.substr(0, slash);
    if (parent.size() == floor + 2 && parent[floor + 1] == L':') parent.push_back(L'\\');
    const DWORD attributes = GetFileAttributesW(parent.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
      return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? guest::kENOENT : guest::kENOTDIR;
    }
    end = slash;
  }
  return guest::kENOENT;
}

int PathErrnoFromWin32(DWORD error, const std::wstring& host_path) {
  switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
      return MissingPathErrno(host_path);
    case ERROR_DIRECTORY:
      return guest::kENOTDIR;
    case ERROR_DELETE_PENDING:
      return guest::kENOENT;
    default:
      return ErrnoFromWin32(error);
  }
}

bool IsSymlinkTag(DWORD tag) noexcept {
  return tag == IO_REPARSE_TAG_SYMLINK || tag == IO_REPARSE_TAG_MOUNT_POINT || tag == 0xA000001DUL ;
}

bool QueryHandleInfo(HANDLE handle, HostFileInfo* info) noexcept {
  FILE_BASIC_INFO basic{};
  if (!GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic))) return false;
  FILE_STANDARD_INFO standard{};
  if (!GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard))) return false;
  info->attributes = basic.FileAttributes;
  info->creation_time = basic.CreationTime.QuadPart;
  info->access_time = basic.LastAccessTime.QuadPart;
  info->write_time = basic.LastWriteTime.QuadPart;
  info->change_time = basic.ChangeTime.QuadPart != 0 ? basic.ChangeTime.QuadPart : basic.LastWriteTime.QuadPart;
  info->size = standard.EndOfFile.QuadPart;
  info->allocation = standard.AllocationSize.QuadPart;
  info->links = standard.NumberOfLinks;
  FILE_ID_INFO id{};
  if (GetFileInformationByHandleEx(handle, FileIdInfo, &id, sizeof(id))) {
    info->volume_serial = id.VolumeSerialNumber;
    std::memcpy(&info->file_id, id.FileId.Identifier, sizeof(info->file_id));
  } else {
    BY_HANDLE_FILE_INFORMATION legacy{};
    if (GetFileInformationByHandle(handle, &legacy)) {
      info->volume_serial = legacy.dwVolumeSerialNumber;
      info->file_id = (static_cast<std::uint64_t>(legacy.nFileIndexHigh) << 32) | legacy.nFileIndexLow;
    }
  }
  info->reparse_tag = 0;
  if ((info->attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    FILE_ATTRIBUTE_TAG_INFO tag{};
    if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag))) info->reparse_tag = tag.ReparseTag;
  }
  return true;
}

}
