// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/util.h"
#include "core/i18n.h"

#include <shlobj.h>
#include <shlwapi.h>
#undef PathIsDirectory

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>

namespace elixir::launcher {

std::wstring Utf8ToWide(std::string_view text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
  return result;
}

std::string WideToUtf8(std::wstring_view text) {
  if (text.empty()) return {};
  const int length =
      WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length, nullptr,
                      nullptr);
  return result;
}

std::wstring Win32ErrorText(DWORD code) {
  wchar_t* buffer = nullptr;
  const DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code,
      0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::wstring message;
  if (length > 0 && buffer != nullptr) {
    message.assign(buffer, length);
    LocalFree(buffer);
  }
  if (length == 0 && buffer == nullptr) {
    HMODULE winhttp = GetModuleHandleW(L"winhttp.dll");
    if (winhttp != nullptr) {
      const DWORD module_length = FormatMessageW(
          FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, winhttp,
          code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
      if (module_length > 0 && buffer != nullptr) {
        message.assign(buffer, module_length);
        LocalFree(buffer);
      }
    }
  }
  message = TrimWide(message);
  wchar_t suffix[32];
  std::swprintf(suffix, 32, L"(error %lu)", static_cast<unsigned long>(code));
  return message.empty() ? std::wstring(L"Unknown error ") + suffix : message + L" " + suffix;
}

std::wstring HResultText(HRESULT hr) {
  wchar_t* buffer = nullptr;
  const DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
      static_cast<DWORD>(hr), 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::wstring message;
  if (length > 0 && buffer != nullptr) {
    message = TrimWide(std::wstring_view(buffer, length));
    LocalFree(buffer);
  }
  wchar_t suffix[32];
  std::swprintf(suffix, 32, L"(HRESULT 0x%08lX)", static_cast<unsigned long>(hr));
  return message.empty() ? std::wstring(L"Unknown error ") + suffix : message + L" " + suffix;
}

std::wstring ErrorCause(DWORD code) {
  switch (code) {
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
      return T(L"Not enough disk space.");
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
      return T(L"Windows denied access.");
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_USER_MAPPED_FILE:
      return T(L"Another program is using the file.");
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return T(L"A file or folder is missing.");
    case ERROR_WRITE_PROTECT:
      return T(L"The drive is read-only.");
    case ERROR_NOT_READY:
    case ERROR_BAD_NETPATH:
    case ERROR_NETNAME_DELETED:
    case ERROR_DEV_NOT_EXIST:
      return T(L"The drive isn’t available.");
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
      return T(L"Not enough memory.");
    case ERROR_VIRUS_INFECTED:
    case ERROR_VIRUS_DELETED:
      return T(L"Your antivirus blocked the file.");
    case 12007:
    case 12029:
    case 12030:
      return T(L"No internet connection.");
    case 12002:
      return T(L"The connection timed out.");
    case 12157:
    case 12175:
      return T(L"A secure connection couldn’t be made.");
    default:
      return {};
  }
}

std::wstring WithErrorCause(std::wstring_view what, DWORD code) {
  const std::wstring cause = ErrorCause(code);
  std::wstring text(what);
  if (!cause.empty()) text += L" " + cause;
  return text;
}

std::wstring JoinPath(std::wstring_view base, std::wstring_view leaf) {
  std::wstring result(base);
  if (!result.empty() && result.back() != L'\\' && result.back() != L'/') result.push_back(L'\\');
  while (!leaf.empty() && (leaf.front() == L'\\' || leaf.front() == L'/')) leaf.remove_prefix(1);
  result.append(leaf);
  return result;
}

std::wstring ParentDirectory(std::wstring_view path) {
  while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.remove_suffix(1);
  const std::size_t slash = path.find_last_of(L"\\/");
  if (slash == std::wstring_view::npos) return {};
  if (slash == 2 && path.size() > 1 && path[1] == L':') return std::wstring(path.substr(0, 3));
  return std::wstring(path.substr(0, slash));
}

std::wstring FileNameOf(std::wstring_view path) {
  const std::size_t slash = path.find_last_of(L"\\/");
  return std::wstring(slash == std::wstring_view::npos ? path : path.substr(slash + 1));
}

std::wstring ExtensionOf(std::wstring_view path) {
  const std::wstring name = FileNameOf(path);
  const std::size_t dot = name.find_last_of(L'.');
  if (dot == std::wstring::npos || dot == 0) return {};
  std::wstring extension = name.substr(dot);
  for (wchar_t& c : extension) c = static_cast<wchar_t>(towlower(c));
  return extension;
}

bool PathIsFile(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool PathIsDirectory(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool PathsEqual(std::wstring_view a, std::wstring_view b) {
  return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) ==
         CSTR_EQUAL;
}

bool EnsureDirectory(const std::wstring& path, std::wstring* error) {
  if (path.empty()) {
    if (error) *error = T(L"A folder couldn’t be created.");
    return false;
  }
  if (PathIsDirectory(path)) return true;
  const std::wstring parent = ParentDirectory(path);
  if (!parent.empty() && parent != path && !PathIsDirectory(parent)) {
    if (!EnsureDirectory(parent, error)) return false;
  }
  if (CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) {
    if (PathIsDirectory(path)) return true;
    if (error) *error = T(L"A folder couldn’t be created because a file is in the way.");
    return false;
  }
  if (error) *error = WithErrorCause(T(L"A folder couldn’t be created."), GetLastError());
  return false;
}

namespace {

struct DispositionInfoEx {
  DWORD flags;
};
constexpr FILE_INFO_BY_HANDLE_CLASS kFileDispositionInfoEx = static_cast<FILE_INFO_BY_HANDLE_CLASS>(21);
constexpr DWORD kDispositionDelete = 0x1;
constexpr DWORD kDispositionPosixSemantics = 0x2;
constexpr DWORD kDispositionIgnoreReadOnly = 0x10;

bool IsPathSeparator(wchar_t c) { return c == L'\\' || c == L'/'; }

}

bool DeleteFileEntry(const std::wstring& path, DWORD attributes) {
  if (!(attributes & FILE_ATTRIBUTE_READONLY)) return DeleteFileW(path.c_str()) != FALSE;
  HANDLE file = CreateFileW(path.c_str(), DELETE | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DispositionInfoEx disposition{kDispositionDelete | kDispositionPosixSemantics | kDispositionIgnoreReadOnly};
  bool deleted = SetFileInformationByHandle(file, kFileDispositionInfoEx, &disposition, sizeof(disposition)) != FALSE;
  if (!deleted) {
    BY_HANDLE_FILE_INFORMATION information{};
    FILE_BASIC_INFO basic{};
    if (GetFileInformationByHandle(file, &information) && information.nNumberOfLinks == 1 &&
        GetFileInformationByHandleEx(file, FileBasicInfo, &basic, sizeof(basic))) {
      basic.CreationTime.QuadPart = 0;
      basic.LastAccessTime.QuadPart = 0;
      basic.LastWriteTime.QuadPart = 0;
      basic.ChangeTime.QuadPart = 0;
      basic.FileAttributes &= ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY);
      if (basic.FileAttributes == 0) basic.FileAttributes = FILE_ATTRIBUTE_NORMAL;
      FILE_DISPOSITION_INFO classic{TRUE};
      deleted = SetFileInformationByHandle(file, FileBasicInfo, &basic, sizeof(basic)) &&
                SetFileInformationByHandle(file, FileDispositionInfo, &classic, sizeof(classic));
    }
  }
  CloseHandle(file);
  return deleted;
}

bool IsRealDirectoryBelow(const std::wstring& root, const std::wstring& path) {
  if (root.empty() || path.size() <= root.size() || !StartsWithI(path, root)) return false;
  std::wstring_view rest = std::wstring_view(path).substr(root.size());
  if (!IsPathSeparator(root.back()) && !IsPathSeparator(rest.front())) return false;
  std::wstring current = root;
  bool any = false;
  while (!rest.empty()) {
    while (!rest.empty() && IsPathSeparator(rest.front())) rest.remove_prefix(1);
    if (rest.empty()) break;
    std::size_t end = 0;
    while (end < rest.size() && !IsPathSeparator(rest[end])) ++end;
    const std::wstring_view part = rest.substr(0, end);
    rest.remove_prefix(end);
    if (part == L"." || part == L"..") return false;
    current = JoinPath(current, part);
    const DWORD attributes = GetFileAttributesW(current.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
      return false;
    }
    any = true;
  }
  return any;
}

void DeleteDirectoryTree(const std::wstring& path) {
  if (path.size() < 4) return;
  const DWORD own = GetFileAttributesW(path.c_str());
  if (own == INVALID_FILE_ATTRIBUTES) return;
  if (own & FILE_ATTRIBUTE_REPARSE_POINT) {
    if (own & FILE_ATTRIBUTE_DIRECTORY) {
      RemoveDirectoryW(path.c_str());
    } else {
      DeleteFileEntry(path, own);
    }
    return;
  }
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileW(JoinPath(path, L"*").c_str(), &data);
  if (find != INVALID_HANDLE_VALUE) {
    do {
      const std::wstring name = data.cFileName;
      if (name == L"." || name == L"..") continue;
      const std::wstring child = JoinPath(path, name);
      const bool directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      const bool link = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
      if (directory && !link) {
        DeleteDirectoryTree(child);
      } else if (directory) {
        RemoveDirectoryW(child.c_str());
      } else {
        DeleteFileEntry(child, data.dwFileAttributes);
      }
    } while (FindNextFileW(find, &data));
    FindClose(find);
  }
  RemoveDirectoryW(path.c_str());
}

bool ReadFileBytes(const std::wstring& path, std::string* out, std::wstring* error, std::uint64_t max_bytes) {
  out->clear();
  const auto report = [error](DWORD code) {
    if (error == nullptr) return;
    *error = ErrorCause(code);
    if (error->empty()) *error = T(L"Windows couldn’t read the file.");
  };
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    report(GetLastError());
    return false;
  }
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file, &size)) {
    report(GetLastError());
    CloseHandle(file);
    return false;
  }
  if (static_cast<std::uint64_t>(size.QuadPart) > max_bytes) {
    if (error) *error = T(L"The file is too large.");
    CloseHandle(file);
    return false;
  }
  out->resize(static_cast<std::size_t>(size.QuadPart));
  std::size_t offset = 0;
  while (offset < out->size()) {
    DWORD read = 0;
    const DWORD chunk = static_cast<DWORD>((out->size() - offset) > (1u << 20) ? (1u << 20) : (out->size() - offset));
    if (!ReadFile(file, out->data() + offset, chunk, &read, nullptr)) {
      report(GetLastError());
      CloseHandle(file);
      out->clear();
      return false;
    }
    if (read == 0) break;
    offset += read;
  }
  out->resize(offset);
  CloseHandle(file);
  return true;
}

std::wstring BackupPathFor(const std::wstring& path) { return path + L".bak"; }

namespace {

struct RenameInfoEx {
  DWORD flags;
  HANDLE root_directory;
  DWORD file_name_length;
  WCHAR file_name[1];
};
constexpr FILE_INFO_BY_HANDLE_CLASS kFileRenameInfoEx = static_cast<FILE_INFO_BY_HANDLE_CLASS>(22);
constexpr DWORD kRenameReplaceIfExists = 0x1;
constexpr DWORD kRenamePosixSemantics = 0x2;

bool RenameHandleOver(HANDLE file, const std::wstring& destination) {
  const std::size_t name_bytes = destination.size() * sizeof(wchar_t);
  std::vector<std::byte> buffer(offsetof(RenameInfoEx, file_name) + name_bytes + sizeof(wchar_t));
  auto* info = reinterpret_cast<RenameInfoEx*>(buffer.data());
  info->flags = kRenameReplaceIfExists | kRenamePosixSemantics;
  info->root_directory = nullptr;
  info->file_name_length = static_cast<DWORD>(name_bytes);
  std::memcpy(info->file_name, destination.c_str(), name_bytes + sizeof(wchar_t));
  return SetFileInformationByHandle(file, kFileRenameInfoEx, info, static_cast<DWORD>(buffer.size())) != FALSE;
}

bool IsTransientReplaceError(DWORD code) {
  return code == ERROR_SHARING_VIOLATION || code == ERROR_ACCESS_DENIED || code == ERROR_LOCK_VIOLATION ||
         code == ERROR_UNABLE_TO_REMOVE_REPLACED || code == ERROR_DELETE_PENDING;
}

}

bool AtomicWriteFile(const std::wstring& path, std::string_view data, AtomicWriteBackup backup,
                     std::wstring* error) {
  const std::wstring directory = ParentDirectory(path);
  if (!directory.empty() && !EnsureDirectory(directory, error)) return false;
  const auto report = [error](DWORD code) {
    if (error == nullptr) return;
    *error = ErrorCause(code);
    if (error->empty()) *error = T(L"Windows couldn’t save the file.");
  };

  static std::atomic<unsigned> counter{0};
  wchar_t suffix[64];
  std::swprintf(suffix, 64, L".tmp-%lu-%u", static_cast<unsigned long>(GetCurrentProcessId()),
                counter.fetch_add(1));
  const std::wstring temp = path + suffix;

  HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    report(GetLastError());
    return false;
  }
  bool ok = true;
  DWORD failure = 0;
  std::size_t offset = 0;
  while (ok && offset < data.size()) {
    const DWORD chunk = static_cast<DWORD>((data.size() - offset) > (1u << 20) ? (1u << 20) : (data.size() - offset));
    DWORD written = 0;
    if (!WriteFile(file, data.data() + offset, chunk, &written, nullptr) || written == 0) {
      ok = false;
      failure = GetLastError();
    }
    offset += written;
  }
  if (ok && !FlushFileBuffers(file)) {
    ok = false;
    failure = GetLastError();
  }
  if (!ok) {
    CloseHandle(file);
    DeleteFileW(temp.c_str());
    report(failure);
    return false;
  }

  if (backup == AtomicWriteBackup::kKeepPrevious && PathIsFile(path)) {
    const std::wstring backup_path = BackupPathFor(path);
    const std::wstring backup_temp = backup_path + suffix;
    if (CopyFileW(path.c_str(), backup_temp.c_str(), FALSE)) {
      if (!MoveFileExW(backup_temp.c_str(), backup_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(backup_temp.c_str());
      }
    }
  }

  for (int attempt = 0; attempt < 40; ++attempt) {
    if (RenameHandleOver(file, path)) {
      CloseHandle(file);
      return true;
    }
    failure = GetLastError();
    if (!IsTransientReplaceError(failure)) break;
    Sleep(static_cast<DWORD>(2 + attempt));
  }
  CloseHandle(file);

  for (int attempt = 0; attempt < 40; ++attempt) {
    if (MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    failure = GetLastError();
    if (!IsTransientReplaceError(failure)) break;
    Sleep(static_cast<DWORD>(2 + attempt));
  }
  DeleteFileW(temp.c_str());
  report(failure);
  return false;
}

bool IsDangerousToOpen(const std::wstring& path) {
  const std::wstring extension = ExtensionOf(path);
  if (extension.empty()) return false;
  for (const wchar_t* risky : {L".exe", L".com", L".bat", L".cmd", L".scr", L".pif", L".lnk", L".msi", L".ps1"}) {
    if (extension == risky) return true;
  }
  return AssocIsDangerous(extension.c_str()) != FALSE;
}

std::wstring ExecutablePath() {
  std::wstring buffer(MAX_PATH, L'\0');
  for (;;) {
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) return {};
    if (length < buffer.size()) {
      buffer.resize(length);
      return buffer;
    }
    buffer.resize(buffer.size() * 2);
  }
}

std::wstring ExecutableDirectory() { return ParentDirectory(ExecutablePath()); }

std::wstring LocalAppDataDirectory() {
  PWSTR raw = nullptr;
  std::wstring result;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw)) && raw != nullptr) {
    result = raw;
  }
  CoTaskMemFree(raw);
  if (result.empty()) result = EnvironmentVariable(L"LOCALAPPDATA");
  return result;
}

std::wstring EnvironmentVariable(const wchar_t* name) {
  const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
  if (needed == 0) return {};
  std::wstring value(needed, L'\0');
  const DWORD length = GetEnvironmentVariableW(name, value.data(), needed);
  value.resize(length);
  return value;
}

std::string UtcIsoTimestamp() {
  SYSTEMTIME now{};
  GetSystemTime(&now);
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02uZ", now.wYear, now.wMonth, now.wDay, now.wHour,
                now.wMinute, now.wSecond);
  return buffer;
}

std::wstring UtcFileStamp() {
  SYSTEMTIME now{};
  GetSystemTime(&now);
  wchar_t buffer[32];
  std::swprintf(buffer, 32, L"%04u%02u%02u-%02u%02u%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
                now.wSecond);
  return buffer;
}

std::wstring FormatByteSize(std::uint64_t bytes) {
  static const wchar_t* const kUnits[] = {L"bytes", L"KB", L"MB", L"GB", L"TB"};
  double value = static_cast<double>(bytes);
  int unit = 0;
  while (value >= 1024.0 && unit < 4) {
    value /= 1024.0;
    ++unit;
  }
  wchar_t buffer[48];
  if (unit == 0) {
    std::swprintf(buffer, 48, L"%llu bytes", static_cast<unsigned long long>(bytes));
  } else {
    std::swprintf(buffer, 48, value >= 100 ? L"%.0f %ls" : L"%.1f %ls", value, kUnits[unit]);
  }
  return buffer;
}

std::wstring FormatFileTimeLocal(const FILETIME& time) {
  FILETIME local{};
  SYSTEMTIME st{};
  if (!FileTimeToLocalFileTime(&time, &local) || !FileTimeToSystemTime(&local, &st)) return L"?";
  wchar_t buffer[48];
  std::swprintf(buffer, 48, L"%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                st.wSecond);
  return buffer;
}

std::string TrimAscii(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' || text.front() == '\n'))
    text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n'))
    text.remove_suffix(1);
  return std::string(text);
}

std::wstring TrimWide(std::wstring_view text) {
  while (!text.empty() && iswspace(text.front())) text.remove_prefix(1);
  while (!text.empty() && iswspace(text.back())) text.remove_suffix(1);
  return std::wstring(text);
}

std::string ToLowerAscii(std::string_view text) {
  std::string result(text);
  for (char& c : result) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return result;
}

bool StartsWithI(std::wstring_view text, std::wstring_view prefix) {
  return text.size() >= prefix.size() && PathsEqual(text.substr(0, prefix.size()), prefix);
}

}
