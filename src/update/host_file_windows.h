// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef MOCKTAIL_UPDATE_HOST_FILE_WINDOWS_H_
#define MOCKTAIL_UPDATE_HOST_FILE_WINDOWS_H_

#if !defined(_WIN32)
#error "host_file_windows.h is only for Windows builds"
#endif

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace mocktail::update::host_file {

std::string Utf8(const std::filesystem::path& path);
std::string GenericUtf8(const std::filesystem::path& path);

bool IsValidUtf8(std::string_view text);

bool PathFromUtf8(std::string_view utf8, std::filesystem::path* path);

std::string ErrorText(unsigned long code);

enum class OpenStatus {
  kOk,
  kUnavailable,
  kNotRegular,
};

class ReadOnlyFile final {
 public:
  enum class Sharing {
    kReadRenameDelete,
    kReadOnly,
  };

  ReadOnlyFile() = default;
  ~ReadOnlyFile();
  ReadOnlyFile(const ReadOnlyFile&) = delete;
  ReadOnlyFile& operator=(const ReadOnlyFile&) = delete;

  OpenStatus Open(const std::filesystem::path& path,
                  Sharing sharing = Sharing::kReadRenameDelete);
  bool Size(std::uint64_t* size) const;
  long long Read(void* buffer, std::size_t size);
  bool ReadAt(std::uint64_t offset, void* buffer, std::size_t size);
  void* native_handle() const { return handle_; }
  unsigned long last_error() const { return last_error_; }
  void Close();

 private:
  friend class ReadOnlyMapping;
  void* handle_ = nullptr;
  unsigned long last_error_ = 0;
};

enum class ReadStatus { kOk, kUnavailable, kNotRegularOrTooLarge, kReadFailed };

ReadStatus ReadRegularFile(const std::filesystem::path& path,
                           std::uintmax_t maximum, std::string* contents);

class NewFile final {
 public:
  NewFile() = default;
  ~NewFile();
  NewFile(const NewFile&) = delete;
  NewFile& operator=(const NewFile&) = delete;

  bool Create(const std::filesystem::path& path);
  bool WriteAll(const void* data, std::size_t size);
  bool Sync();
  void Close();
  unsigned long last_error() const { return last_error_; }

 private:
  void* handle_ = nullptr;
  unsigned long last_error_ = 0;
};

bool WriteNewFile(const std::filesystem::path& path, std::string_view contents,
                  unsigned long* error_code);

class ReadOnlyMapping final {
 public:
  enum class Status { kOk, kUnavailable, kNotRegular, kTooLarge, kMapFailed };

  ReadOnlyMapping() = default;
  ~ReadOnlyMapping();
  ReadOnlyMapping(const ReadOnlyMapping&) = delete;
  ReadOnlyMapping& operator=(const ReadOnlyMapping&) = delete;

  Status Map(const std::filesystem::path& path);
  const unsigned char* data() const { return data_; }
  std::size_t size() const { return size_; }

 private:
  ReadOnlyFile file_;
  void* mapping_ = nullptr;
  const unsigned char* data_ = nullptr;
  std::size_t size_ = 0;
};

}

#endif
