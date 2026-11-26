// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_UTIL_H_
#define ELIXIR_LAUNCHER_CORE_UTIL_H_

#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace elixir::launcher {

std::wstring Utf8ToWide(std::string_view text);
std::string WideToUtf8(std::wstring_view text);

std::wstring Win32ErrorText(DWORD code);
std::wstring HResultText(HRESULT hr);
std::wstring ErrorCause(DWORD code);
std::wstring WithErrorCause(std::wstring_view what, DWORD code);

std::wstring JoinPath(std::wstring_view base, std::wstring_view leaf);
std::wstring ParentDirectory(std::wstring_view path);
std::wstring FileNameOf(std::wstring_view path);
std::wstring ExtensionOf(std::wstring_view path);
bool PathIsFile(const std::wstring& path);
bool PathIsDirectory(const std::wstring& path);
bool PathsEqual(std::wstring_view a, std::wstring_view b);
bool EnsureDirectory(const std::wstring& path, std::wstring* error);
void DeleteDirectoryTree(const std::wstring& path);
bool DeleteFileEntry(const std::wstring& path, DWORD attributes);
bool IsRealDirectoryBelow(const std::wstring& root, const std::wstring& path);

bool ReadFileBytes(const std::wstring& path, std::string* out, std::wstring* error,
                   std::uint64_t max_bytes = 64ull << 20);

enum class AtomicWriteBackup { kNone, kKeepPrevious };

bool AtomicWriteFile(const std::wstring& path, std::string_view data, AtomicWriteBackup backup,
                     std::wstring* error);
std::wstring BackupPathFor(const std::wstring& path);

bool IsDangerousToOpen(const std::wstring& path);

std::wstring ExecutablePath();
std::wstring ExecutableDirectory();
std::wstring LocalAppDataDirectory();
std::wstring EnvironmentVariable(const wchar_t* name);

std::string UtcIsoTimestamp();
std::wstring UtcFileStamp();
std::wstring FormatByteSize(std::uint64_t bytes);
std::wstring FormatFileTimeLocal(const FILETIME& time);

std::string TrimAscii(std::string_view text);
std::wstring TrimWide(std::wstring_view text);
std::string ToLowerAscii(std::string_view text);
bool StartsWithI(std::wstring_view text, std::wstring_view prefix);

}

#endif
