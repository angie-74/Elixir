// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/crash_logs.h"

#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <unordered_map>

#include "core/util.h"

namespace elixir::launcher {

std::vector<CrashLogEntry> ListCrashLogs(const std::wstring& directory) {
  std::vector<CrashLogEntry> entries;
  for (const wchar_t* pattern : {L"*.txt", L"*.dmp"}) {
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(JoinPath(directory, pattern).c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) continue;
    do {
      if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
      CrashLogEntry entry;
      entry.name = data.cFileName;
      const std::wstring extension = ExtensionOf(entry.name);
      if (extension != L".txt" && extension != L".dmp") continue;
      entry.path = JoinPath(directory, entry.name);
      entry.size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
      entry.modified = data.ftLastWriteTime;
      entry.is_dump = extension == L".dmp";
      entries.push_back(std::move(entry));
    } while (FindNextFileW(find, &data));
    FindClose(find);
  }
  const auto stem_of = [](const CrashLogEntry& entry) {
    std::wstring stem = entry.name.substr(0, entry.name.size() - 4);
    for (wchar_t& c : stem) c = static_cast<wchar_t>(towlower(c));
    return stem;
  };
  const auto ticks = [](const FILETIME& time) {
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
  };
  std::unordered_map<std::wstring, std::uint64_t> newest;
  for (const CrashLogEntry& entry : entries) {
    std::uint64_t& time = newest[stem_of(entry)];
    time = std::max(time, ticks(entry.modified));
  }
  std::sort(entries.begin(), entries.end(), [&](const CrashLogEntry& a, const CrashLogEntry& b) {
    const std::wstring stem_a = stem_of(a);
    const std::wstring stem_b = stem_of(b);
    const std::uint64_t time_a = newest[stem_a];
    const std::uint64_t time_b = newest[stem_b];
    if (time_a != time_b) return time_a > time_b;
    if (stem_a != stem_b) return stem_a > stem_b;
    return !a.is_dump && b.is_dump;
  });
  return entries;
}

bool ReadCrashLogText(const std::wstring& path, std::wstring* text, std::wstring* error, std::uint64_t max_bytes) {
  std::string bytes;
  const auto report = [error](DWORD code) {
    if (error == nullptr) return;
    *error = ErrorCause(code);
    if (error->empty()) *error = L"Windows couldn’t read the file.";
  };
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, 0, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    report(GetLastError());
    return false;
  }
  LARGE_INTEGER size{};
  GetFileSizeEx(file, &size);
  const std::uint64_t total = static_cast<std::uint64_t>(size.QuadPart);
  bytes.resize(static_cast<std::size_t>(total < max_bytes ? total : max_bytes));
  DWORD read = 0;
  const BOOL ok = bytes.empty() || ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
  const DWORD read_error = GetLastError();
  CloseHandle(file);
  if (!ok) {
    report(read_error);
    return false;
  }
  bytes.resize(read);
  std::wstring wide = Utf8ToWide(bytes);
  text->clear();
  text->reserve(wide.size() + wide.size() / 16);
  for (std::size_t i = 0; i < wide.size(); ++i) {
    if (wide[i] == L'\n' && (i == 0 || wide[i - 1] != L'\r')) text->push_back(L'\r');
    if (wide[i] == L'\0') {
      text->push_back(L' ');
      continue;
    }
    text->push_back(wide[i]);
  }
  if (total > max_bytes) {
    *text += L"\r\n\r\n[Showing the first " + FormatByteSize(max_bytes) + L" of " + FormatByteSize(total) +
             L". Open the folder to see the whole file.]";
  }
  return true;
}

std::vector<std::wstring> CrashLogGroup(const std::wstring& path) {
  std::vector<std::wstring> group{path};
  const std::wstring extension = ExtensionOf(path);
  const std::wstring stem = path.substr(0, path.size() - extension.size());
  const std::wstring other = stem + (extension == L".txt" ? L".dmp" : L".txt");
  if (PathIsFile(other)) group.push_back(other);
  return group;
}

bool RecycleFiles(const std::vector<std::wstring>& paths, std::wstring* error) {
  if (paths.empty()) return true;
  std::wstring list;
  for (const std::wstring& path : paths) {
    list += path;
    list.push_back(L'\0');
  }
  list.push_back(L'\0');
  SHFILEOPSTRUCTW operation{};
  operation.wFunc = FO_DELETE;
  operation.pFrom = list.c_str();
  operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_WANTNUKEWARNING;
  const int result = SHFileOperationW(&operation);
  if (result != 0 || operation.fAnyOperationsAborted) {
    if (error) {
      *error = operation.fAnyOperationsAborted ? L"Cancelled." : L"The files couldn’t be moved to the Recycle Bin.";
    }
    return false;
  }
  return true;
}

}
