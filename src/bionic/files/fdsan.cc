// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The tag encoding, the owner-type names and the message texts are adapted
// from AOSP bionic libc/bionic/fdsan.cpp, which carries this notice:
//
// Copyright (C) 2018 The Android Open Source Project
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
#include "bionic/files/files_internal.h"

#include <windows.h>

#include <atomic>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "bionic/core/core.h"

ELIXIR_BIONIC_FN(void, abort)() noexcept;
ELIXIR_BIONIC_FN(void, android_set_abort_message)(const char* message) noexcept;

namespace elixir::bionic::files {
namespace {

constexpr int kLevelDisabled = 0;
constexpr int kLevelWarnOnce = 1;
constexpr int kLevelFatal = 3;

constexpr std::uint64_t kOwnerGeneric00 = 0;
constexpr std::uint64_t kOwnerFile = 1;
constexpr std::uint64_t kOwnerDir = 2;
constexpr std::uint64_t kOwnerUniqueFd = 3;
constexpr std::uint64_t kOwnerSqlite = 4;
constexpr std::uint64_t kOwnerFileInputStream = 5;
constexpr std::uint64_t kOwnerFileOutputStream = 6;
constexpr std::uint64_t kOwnerRandomAccessFile = 7;
constexpr std::uint64_t kOwnerParcelFileDescriptor = 8;
constexpr std::uint64_t kOwnerArtFdFile = 9;
constexpr std::uint64_t kOwnerDatagramSocketImpl = 10;
constexpr std::uint64_t kOwnerSocketImpl = 11;
constexpr std::uint64_t kOwnerZipArchive = 12;
constexpr std::uint64_t kOwnerNativeHandle = 13;
constexpr std::uint64_t kOwnerParcel = 14;
constexpr std::uint64_t kOwnerGenericFf = 255;

constexpr int kLogError = 6;
constexpr int kLogFatal = 7;

std::atomic<int> g_error_level{kLevelFatal};

const char* TagType(std::uint64_t tag) noexcept {
  switch (tag >> 56) {
    case kOwnerFile: return "FILE*";
    case kOwnerDir: return "DIR*";
    case kOwnerUniqueFd: return "unique_fd";
    case kOwnerFileInputStream: return "FileInputStream";
    case kOwnerFileOutputStream: return "FileOutputStream";
    case kOwnerRandomAccessFile: return "RandomAccessFile";
    case kOwnerParcelFileDescriptor: return "ParcelFileDescriptor";
    case kOwnerSqlite: return "sqlite";
    case kOwnerArtFdFile: return "ART FdFile";
    case kOwnerDatagramSocketImpl: return "DatagramSocketImpl";
    case kOwnerSocketImpl: return "SocketImpl";
    case kOwnerZipArchive: return "ZipArchive";
    case kOwnerNativeHandle: return "native_handle_t";
    case kOwnerParcel: return "Parcel";
    case kOwnerGenericFf:
      if ((tag >> 48) == 0xffff) return "native object of unknown type";
      return "Java object of unknown type";
    case kOwnerGeneric00:
    default:
      return "native object of unknown type";
  }
}

std::uint64_t TagValue(std::uint64_t tag) noexcept {
  return static_cast<std::uint64_t>(static_cast<std::int64_t>(tag << 8) >> 8);
}

void WriteGuestStderr(const char* text) noexcept {
  try {
    DescriptionPtr d = FindDescription(2);
    if (d == nullptr) return;
    std::string line(text);
    line.push_back('\n');
    DescWrite(*d, line.data(), line.size());
  } catch (...) {
  }
}

[[noreturn]] void Fatal(const char* prefix, const char* message, bool raise_first) noexcept {
  char text[1024];
  if (prefix != nullptr) {
    std::snprintf(text, sizeof(text), "%s: %s", prefix, message);
  } else {
    std::snprintf(text, sizeof(text), "%s", message);
  }
  WriteGuestStderr(text);
  HostLog(kLogFatal, "libc", text);
  elixir_bionic_android_set_abort_message(text);
  if (raise_first) core::RaiseGuestSignal(guest::kSIGABRT);
  elixir_bionic_abort();
  for (;;) TerminateProcess(GetCurrentProcess(), 128 + guest::kSIGABRT);
}

#if defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
void FdsanError(const char* format, ...) noexcept {
  int level = g_error_level.load();
  if (level == kLevelDisabled) return;
  char message[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  if (level == kLevelFatal) Fatal("fdsan", message, true);
  const int saved = GuestErrno();
  HostLog(kLogError, "fdsan", message);
  SetGuestErrno(saved);
  if (level == kLevelWarnOnce) g_error_level.compare_exchange_strong(level, kLevelDisabled);
}

void ExchangeOwnerTag(int fd, std::uint64_t expected_tag, std::uint64_t new_tag) {
  std::uint64_t tag = 0;
  if (!ExchangeFdOwnerTag(fd, expected_tag, new_tag, &tag) || tag == expected_tag) return;
  if (expected_tag != 0 && tag != 0) {
    FdsanError("failed to exchange ownership of file descriptor: fd %d is owned by %s 0x%" PRIx64
               ", was expected to be owned by %s 0x%" PRIx64,
               fd, TagType(tag), TagValue(tag), TagType(expected_tag), TagValue(expected_tag));
  } else if (expected_tag != 0) {
    FdsanError("failed to exchange ownership of file descriptor: fd %d is unowned, was expected to be owned by %s "
               "0x%" PRIx64,
               fd, TagType(expected_tag), TagValue(expected_tag));
  } else {
    FdsanError("failed to exchange ownership of file descriptor: fd %d is owned by %s 0x%" PRIx64
               ", was expected to be unowned",
               fd, TagType(tag), TagValue(tag));
  }
}

}

int FdsanClose(int fd, std::uint64_t expected_tag) {
  std::uint64_t tag = 0;
  if (ExchangeFdOwnerTag(fd, expected_tag, 0, &tag) && tag != expected_tag) {
    if (expected_tag != 0 && tag != 0) {
      FdsanError("attempted to close file descriptor %d, expected to be owned by %s 0x%" PRIx64
                 ", actually owned by %s 0x%" PRIx64,
                 fd, TagType(expected_tag), TagValue(expected_tag), TagType(tag), TagValue(tag));
    } else if (expected_tag != 0) {
      FdsanError("attempted to close file descriptor %d, expected to be owned by %s 0x%" PRIx64 ", actually unowned",
                 fd, TagType(expected_tag), TagValue(expected_tag));
    } else {
      FdsanError("attempted to close file descriptor %d, expected to be unowned, actually owned by %s 0x%" PRIx64,
                 fd, TagType(tag), TagValue(tag));
    }
  }
  const int rc = CloseFd(fd);
  if (expected_tag != 0 && rc == -1 && GuestErrno() == guest::kEBADF) {
    FdsanError("double-close of file descriptor %d detected", fd);
    SetGuestErrno(guest::kEBADF);
  }
  return rc;
}

}

using elixir::bionic::files::Guarded;
namespace files = elixir::bionic::files;
namespace guest = elixir::bionic::guest;

ELIXIR_BIONIC_FN(std::uint64_t, android_fdsan_create_owner_tag)(int type, std::uint64_t tag) noexcept {
  if (tag == 0) return 0;
  const auto value = static_cast<std::uint32_t>(type);
  if ((value & 0xff) != value) {
    char message[96];
    std::snprintf(message, sizeof(message), "invalid android_fdsan_owner_type value: %x", value);
    files::Fatal(nullptr, message, false);
  }
  const std::uint64_t mask = (std::uint64_t{1} << 56) - 1;
  return (static_cast<std::uint64_t>(value) << 56) | (tag & mask);
}

ELIXIR_BIONIC_FN(void, android_fdsan_exchange_owner_tag)(int fd, std::uint64_t expected_tag,
                                                          std::uint64_t new_tag) noexcept {
  Guarded(0, [&] {
    files::ExchangeOwnerTag(fd, expected_tag, new_tag);
    return 0;
  });
}

ELIXIR_BIONIC_FN(int, android_fdsan_close_with_tag)(int fd, std::uint64_t expected_tag) noexcept {
  return Guarded(-1, [&] { return files::FdsanClose(fd, expected_tag); });
}

ELIXIR_BIONIC_FN(std::uint64_t, android_fdsan_get_owner_tag)(int fd) noexcept {
  return Guarded(std::uint64_t{0}, [&] { return files::FdOwnerTag(fd); });
}

ELIXIR_BIONIC_FN(const char*, android_fdsan_get_tag_type)(std::uint64_t tag) noexcept { return files::TagType(tag); }

ELIXIR_BIONIC_FN(std::uint64_t, android_fdsan_get_tag_value)(std::uint64_t tag) noexcept {
  return files::TagValue(tag);
}

ELIXIR_BIONIC_FN(int, android_fdsan_get_error_level)() noexcept { return files::g_error_level.load(); }

ELIXIR_BIONIC_FN(int, android_fdsan_set_error_level)(int new_level) noexcept {
  return files::g_error_level.exchange(new_level);
}

namespace elixir::bionic::files {

void RegisterFdsanExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("android_fdsan_create_owner_tag", &elixir_bionic_android_fdsan_create_owner_tag);
  add("android_fdsan_exchange_owner_tag", &elixir_bionic_android_fdsan_exchange_owner_tag);
  add("android_fdsan_close_with_tag", &elixir_bionic_android_fdsan_close_with_tag);
  add("android_fdsan_get_owner_tag", &elixir_bionic_android_fdsan_get_owner_tag);
  add("android_fdsan_get_tag_type", &elixir_bionic_android_fdsan_get_tag_type);
  add("android_fdsan_get_tag_value", &elixir_bionic_android_fdsan_get_tag_value);
  add("android_fdsan_get_error_level", &elixir_bionic_android_fdsan_get_error_level);
  add("android_fdsan_set_error_level", &elixir_bionic_android_fdsan_set_error_level);
}

}
