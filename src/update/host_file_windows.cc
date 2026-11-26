// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "update/host_file_windows.h"

#include <windows.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace mocktail::update::host_file {
namespace {

HANDLE Native(void* handle) { return static_cast<HANDLE>(handle); }

bool SameFile(HANDLE left, HANDLE right) {
  BY_HANDLE_FILE_INFORMATION first = {};
  BY_HANDLE_FILE_INFORMATION second = {};
  return GetFileInformationByHandle(left, &first) != 0 &&
         GetFileInformationByHandle(right, &second) != 0 &&
         first.dwVolumeSerialNumber == second.dwVolumeSerialNumber &&
         first.nFileIndexHigh == second.nFileIndexHigh &&
         first.nFileIndexLow == second.nFileIndexLow;
}

std::wstring Utf8ToWide(std::string_view text) {
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       text.data(), static_cast<int>(text.size()),
                                       nullptr, 0);
  if (size <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                      static_cast<int>(text.size()), wide.data(), size);
  return wide;
}

std::string WideToUtf8(std::wstring_view text) {
  if (text.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                       static_cast<int>(text.size()), nullptr,
                                       0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string narrow(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      narrow.data(), size, nullptr, nullptr);
  return narrow;
}

}

std::string Utf8(const std::filesystem::path& path) {
  return WideToUtf8(path.native());
}

std::string GenericUtf8(const std::filesystem::path& path) {
  return WideToUtf8(path.generic_wstring());
}

bool IsValidUtf8(std::string_view text) {
  std::size_t index = 0;
  while (index < text.size()) {
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80U) {
      ++index;
      continue;
    }
    std::size_t length = 0;
    std::uint32_t point = 0;
    std::uint32_t minimum = 0;
    if ((lead & 0xe0U) == 0xc0U) {
      length = 2;
      point = lead & 0x1fU;
      minimum = 0x80U;
    } else if ((lead & 0xf0U) == 0xe0U) {
      length = 3;
      point = lead & 0x0fU;
      minimum = 0x800U;
    } else if ((lead & 0xf8U) == 0xf0U) {
      length = 4;
      point = lead & 0x07U;
      minimum = 0x10000U;
    } else {
      return false;
    }
    if (text.size() - index < length) return false;
    for (std::size_t offset = 1; offset < length; ++offset) {
      const unsigned char next = static_cast<unsigned char>(text[index + offset]);
      if ((next & 0xc0U) != 0x80U) return false;
      point = (point << 6U) | (next & 0x3fU);
    }
    if (point < minimum || point > 0x10ffffU ||
        (point >= 0xd800U && point <= 0xdfffU)) {
      return false;
    }
    index += length;
  }
  return true;
}

bool PathFromUtf8(std::string_view utf8, std::filesystem::path* path) {
  if (!IsValidUtf8(utf8)) return false;
  *path = std::filesystem::path(Utf8ToWide(utf8));
  return true;
}

std::string ErrorText(unsigned long code) {
  wchar_t* buffer = nullptr;
  const DWORD size = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::string text;
  if (size > 0 && buffer != nullptr) {
    std::wstring_view message(buffer, size);
    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n' ||
            message.back() == L' ' || message.back() == L'.')) {
      message.remove_suffix(1);
    }
    text = WideToUtf8(message);
  }
  if (buffer != nullptr) LocalFree(buffer);
  if (text.empty()) text = "Windows error " + std::to_string(code);
  return text;
}

ReadOnlyFile::~ReadOnlyFile() { Close(); }

void ReadOnlyFile::Close() {
  if (handle_ != nullptr) CloseHandle(Native(handle_));
  handle_ = nullptr;
}

OpenStatus ReadOnlyFile::Open(const std::filesystem::path& path,
                               Sharing sharing) {
  Close();
  const DWORD share = sharing == Sharing::kReadOnly
                          ? FILE_SHARE_READ
                          : FILE_SHARE_READ | FILE_SHARE_DELETE;
  HANDLE handle =
      CreateFileW(path.c_str(), GENERIC_READ, share, nullptr, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT |
                      FILE_FLAG_SEQUENTIAL_SCAN,
                  nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    last_error_ = GetLastError();
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
      return OpenStatus::kNotRegular;
    }
    return OpenStatus::kUnavailable;
  }
  if (GetFileType(handle) != FILE_TYPE_DISK) {
    CloseHandle(handle);
    return OpenStatus::kNotRegular;
  }
  FILE_ATTRIBUTE_TAG_INFO tag = {};
  if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag,
                                   sizeof(tag)) == 0) {
    last_error_ = GetLastError();
    CloseHandle(handle);
    return OpenStatus::kUnavailable;
  }
  if ((tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    if (IsReparseTagNameSurrogate(tag.ReparseTag)) {
      CloseHandle(handle);
      last_error_ = ERROR_CANT_ACCESS_FILE;
      return OpenStatus::kUnavailable;
    }
    HANDLE through = CreateFileW(path.c_str(), GENERIC_READ, share, nullptr,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                 nullptr);
    if (through == INVALID_HANDLE_VALUE || !SameFile(handle, through)) {
      last_error_ = through == INVALID_HANDLE_VALUE ? GetLastError()
                                                    : ERROR_CANT_ACCESS_FILE;
      if (through != INVALID_HANDLE_VALUE) CloseHandle(through);
      CloseHandle(handle);
      return OpenStatus::kUnavailable;
    }
    CloseHandle(handle);
    handle = through;
  }
  if ((tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    CloseHandle(handle);
    return OpenStatus::kNotRegular;
  }
  handle_ = handle;
  return OpenStatus::kOk;
}

bool ReadOnlyFile::Size(std::uint64_t* size) const {
  LARGE_INTEGER value = {};
  if (handle_ == nullptr || GetFileSizeEx(Native(handle_), &value) == 0 ||
      value.QuadPart < 0) {
    return false;
  }
  *size = static_cast<std::uint64_t>(value.QuadPart);
  return true;
}

long long ReadOnlyFile::Read(void* buffer, std::size_t size) {
  if (handle_ == nullptr) return -1;
  const DWORD request = static_cast<DWORD>(
      std::min<std::size_t>(size, std::numeric_limits<DWORD>::max() / 2U));
  DWORD read = 0;
  if (ReadFile(Native(handle_), buffer, request, &read, nullptr) == 0) {
    last_error_ = GetLastError();
    return -1;
  }
  return static_cast<long long>(read);
}

bool ReadOnlyFile::ReadAt(std::uint64_t offset, void* buffer,
                          std::size_t size) {
  if (handle_ == nullptr) return false;
  auto* bytes = static_cast<unsigned char*>(buffer);
  while (size > 0) {
    OVERLAPPED position = {};
    position.Offset = static_cast<DWORD>(offset);
    position.OffsetHigh = static_cast<DWORD>(offset >> 32U);
    const DWORD request = static_cast<DWORD>(
        std::min<std::size_t>(size, std::numeric_limits<DWORD>::max() / 2U));
    DWORD read = 0;
    if (ReadFile(Native(handle_), bytes, request, &read, &position) == 0) {
      last_error_ = GetLastError();
      return false;
    }
    if (read == 0) {
      last_error_ = ERROR_HANDLE_EOF;
      return false;
    }
    bytes += read;
    size -= read;
    offset += read;
  }
  return true;
}

ReadStatus ReadRegularFile(const std::filesystem::path& path,
                           std::uintmax_t maximum, std::string* contents) {
  contents->clear();
  ReadOnlyFile file;
  const OpenStatus opened = file.Open(path);
  if (opened == OpenStatus::kUnavailable) return ReadStatus::kUnavailable;
  std::uint64_t size = 0;
  if (opened != OpenStatus::kOk || !file.Size(&size) || size > maximum ||
      size > std::numeric_limits<std::size_t>::max()) {
    return ReadStatus::kNotRegularOrTooLarge;
  }
  contents->reserve(static_cast<std::size_t>(size));
  std::vector<char> buffer(64U * 1024U);
  while (true) {
    const long long bytes = file.Read(buffer.data(), buffer.size());
    if (bytes == 0) break;
    if (bytes < 0) {
      contents->clear();
      return ReadStatus::kReadFailed;
    }
    if (static_cast<std::uintmax_t>(bytes) > maximum - contents->size()) {
      contents->clear();
      return ReadStatus::kNotRegularOrTooLarge;
    }
    contents->append(buffer.data(), static_cast<std::size_t>(bytes));
  }
  return ReadStatus::kOk;
}

NewFile::~NewFile() { Close(); }

void NewFile::Close() {
  if (handle_ != nullptr) CloseHandle(Native(handle_));
  handle_ = nullptr;
}

bool NewFile::Create(const std::filesystem::path& path) {
  Close();
  HANDLE handle = CreateFileW(
      path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    last_error_ = GetLastError();
    return false;
  }
  handle_ = handle;
  return true;
}

bool NewFile::WriteAll(const void* data, std::size_t size) {
  if (handle_ == nullptr) return false;
  const auto* bytes = static_cast<const unsigned char*>(data);
  while (size > 0) {
    const DWORD request = static_cast<DWORD>(
        std::min<std::size_t>(size, 1U << 30U));
    DWORD written = 0;
    if (WriteFile(Native(handle_), bytes, request, &written, nullptr) == 0 ||
        written == 0) {
      last_error_ = GetLastError();
      return false;
    }
    bytes += written;
    size -= written;
  }
  return true;
}

bool NewFile::Sync() {
  if (handle_ == nullptr) return false;
  if (FlushFileBuffers(Native(handle_)) == 0) {
    last_error_ = GetLastError();
    return false;
  }
  return true;
}

bool WriteNewFile(const std::filesystem::path& path, std::string_view contents,
                  unsigned long* error_code) {
  NewFile file;
  const bool written = file.Create(path) &&
                       file.WriteAll(contents.data(), contents.size()) &&
                       file.Sync();
  if (!written && error_code != nullptr) *error_code = file.last_error();
  return written;
}

ReadOnlyMapping::~ReadOnlyMapping() {
  if (data_ != nullptr) UnmapViewOfFile(data_);
  if (mapping_ != nullptr) CloseHandle(Native(mapping_));
}

ReadOnlyMapping::Status ReadOnlyMapping::Map(const std::filesystem::path& path) {
  const OpenStatus opened = file_.Open(path);
  if (opened == OpenStatus::kUnavailable) return Status::kUnavailable;
  if (opened != OpenStatus::kOk) return Status::kNotRegular;
  std::uint64_t size = 0;
  if (!file_.Size(&size)) return Status::kNotRegular;
  if (size > std::numeric_limits<std::size_t>::max()) return Status::kTooLarge;
  if (size == 0) return Status::kOk;
  HANDLE mapping = CreateFileMappingW(Native(file_.handle_), nullptr,
                                      PAGE_READONLY, 0, 0, nullptr);
  if (mapping == nullptr) return Status::kMapFailed;
  void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
  if (view == nullptr) {
    CloseHandle(mapping);
    return Status::kMapFailed;
  }
  mapping_ = mapping;
  data_ = static_cast<const unsigned char*>(view);
  size_ = static_cast<std::size_t>(size);
  return Status::kOk;
}

}
