// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP system/logging liblog/logger_write.cpp,
// liblog/properties.cpp and liblog/logger_name.cpp (as vendored in
// third_party/mcpelauncher-linker/core/liblog), which carry this notice:
//
//   Copyright (C) 2007-2016 The Android Open Source Project
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

#include "abi/variadic_thunk.h"
#include "bionic/format.h"
#include "bionic/format/format_internal.h"
#include "bionic/guest_constants.h"
#include "bionic/runtime.h"

namespace elixir::bionic {
namespace {

using namespace format::internal;

struct GuestLogMessage {
  abi::g_size struct_size;
  std::int32_t buffer_id;
  std::int32_t priority;
  const char* tag;
  const char* file;
  std::uint32_t line;
  const char* message;
};
static_assert(sizeof(GuestLogMessage) == 48, "__android_log_message is 48 bytes on LP64");
static_assert(offsetof(GuestLogMessage, buffer_id) == 8, "buffer_id offset");
static_assert(offsetof(GuestLogMessage, priority) == 12, "priority offset");
static_assert(offsetof(GuestLogMessage, tag) == 16, "tag offset");
static_assert(offsetof(GuestLogMessage, file) == 24, "file offset");
static_assert(offsetof(GuestLogMessage, line) == 32, "line offset");
static_assert(offsetof(GuestLogMessage, message) == 40, "message offset");

constexpr std::int32_t kLogIdMain = 0;
constexpr std::int32_t kLogIdRadio = 1;
constexpr std::int32_t kLogIdSystem = 3;
constexpr std::int32_t kLogIdCrash = 4;
constexpr std::int32_t kLogIdMax = 8;
constexpr std::int32_t kLogIdDefault = 0x7fffffff;

constexpr std::size_t kLogBufferSize = 1024;
constexpr std::size_t kLoggerEntryMaxPayload = 4068;

using LoggerFunction = abi::SysvFn<void(const GuestLogMessage*)>;
using AborterFunction = abi::SysvFn<void(const char*)>;

class ErrnoRestorer {
 public:
  ErrnoRestorer() : saved_(GuestErrno()) {}
  ~ErrnoRestorer() { SetGuestErrno(saved_); }

 private:
  int saved_;
};

std::atomic<std::int32_t> g_minimum_priority{kLogDefault};

std::mutex& TagLock() {
  static std::mutex lock;
  return lock;
}
std::string& DefaultTag() {
  static std::string tag = Options().package_name;
  return tag;
}

const char* const kLogNames[kLogIdMax] = {"main", "radio", "events", "system",
                                          "crash", "stats", "security", "kernel"};

}
}

using elixir::abi::g_size;
using elixir::abi::SysvVaList;
using elixir::bionic::GuestLogMessage;

ELIXIR_BIONIC_FN(void, __android_log_logd_logger)(const GuestLogMessage* message) noexcept {
  if (message == nullptr) return;
  elixir::bionic::HostLog(message->priority, message->tag != nullptr ? message->tag : "",
                          message->message != nullptr ? message->message : "");
}

ELIXIR_BIONIC_FN(void, __android_log_logd_logger_with_timestamp)(const GuestLogMessage* message,
                                                                const void* ) noexcept {
  elixir_bionic___android_log_logd_logger(message);
}

ELIXIR_BIONIC_FN(void, __android_log_stderr_logger)(const GuestLogMessage* message) noexcept {
  if (message == nullptr) return;
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_s(&local, &now);
  char timestamp[32];
  std::strftime(timestamp, sizeof(timestamp), "%m-%d %H:%M:%S", &local);
  static const char kLogCharacters[] = "XXVDIWEF";
  std::int32_t priority = message->priority > elixir::bionic::format::internal::kLogSilent
                              ? elixir::bionic::format::internal::kLogFatal
                              : message->priority;
  if (priority < 0) priority = 0;
  const char priority_char = kLogCharacters[priority];
  std::string line;
  char head[256];
  std::snprintf(head, sizeof(head), "%s %c %s %5lu %5lu ", message->tag != nullptr ? message->tag : "nullptr",
                priority_char, timestamp, GetCurrentProcessId(), GetCurrentThreadId());
  line = head;
  if (message->file != nullptr) {
    line += message->file;
    line += ':';
    line += std::to_string(message->line);
    line += "] ";
  }
  if (message->message != nullptr) line += message->message;
  line += '\n';
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
}

ELIXIR_BIONIC_FN(void, __android_log_default_aborter)(const char* abort_message) noexcept {
  elixir::bionic::format::internal::FatalError(abort_message != nullptr ? abort_message : "abort");
}

namespace elixir::bionic {
namespace {

std::atomic<LoggerFunction> g_logger{&elixir_bionic___android_log_logd_logger};
std::atomic<AborterFunction> g_aborter{&elixir_bionic___android_log_default_aborter};

bool IsLoggable(int priority, int default_priority) {
  const std::int32_t minimum = g_minimum_priority.load(std::memory_order_relaxed);
  if (minimum != kLogDefault) return priority >= minimum;
  return priority >= default_priority;
}

}
}

ELIXIR_BIONIC_FN(void, __android_log_set_logger)(elixir::bionic::LoggerFunction logger) noexcept {
  elixir::bionic::g_logger.store(logger);
}

ELIXIR_BIONIC_FN(void, __android_log_set_aborter)(elixir::bionic::AborterFunction aborter) noexcept {
  elixir::bionic::g_aborter.store(aborter);
}

ELIXIR_BIONIC_FN(void, __android_log_call_aborter)(const char* abort_message) noexcept {
  const elixir::bionic::AborterFunction aborter = elixir::bionic::g_aborter.load();
  if (aborter != nullptr) aborter(abort_message);
}

ELIXIR_BIONIC_FN(int, __android_log_is_loggable)(int priority, const char* , int default_priority) noexcept {
  return elixir::bionic::IsLoggable(priority, default_priority) ? 1 : 0;
}

ELIXIR_BIONIC_FN(int, __android_log_is_loggable_len)(int priority, const char* , g_size ,
                                                    int default_priority) noexcept {
  return elixir::bionic::IsLoggable(priority, default_priority) ? 1 : 0;
}

ELIXIR_BIONIC_FN(std::int32_t, __android_log_set_minimum_priority)(std::int32_t priority) noexcept {
  return elixir::bionic::g_minimum_priority.exchange(priority, std::memory_order_relaxed);
}

ELIXIR_BIONIC_FN(std::int32_t, __android_log_get_minimum_priority)() noexcept {
  return elixir::bionic::g_minimum_priority.load(std::memory_order_relaxed);
}

ELIXIR_BIONIC_FN(void, __android_log_set_default_tag)(const char* tag) noexcept {
  using namespace elixir::bionic;
  if (tag == nullptr) return;
  try {
    std::lock_guard<std::mutex> guard(TagLock());
    DefaultTag().assign(tag, 0, kLoggerEntryMaxPayload);
  } catch (...) {
  }
}

ELIXIR_BIONIC_FN(int, __android_log_is_debuggable)() noexcept { return 0; }

ELIXIR_BIONIC_FN(int, __android_log_security)() noexcept { return 0; }

ELIXIR_BIONIC_FN(void, __android_log_close)() noexcept {}

ELIXIR_BIONIC_FN(const char*, android_log_id_to_name)(std::uint32_t log_id) noexcept {
  if (log_id >= static_cast<std::uint32_t>(elixir::bionic::kLogIdMax)) log_id = elixir::bionic::kLogIdMain;
  return elixir::bionic::kLogNames[log_id];
}

ELIXIR_BIONIC_FN(std::uint32_t, android_name_to_log_id)(const char* name) noexcept {
  using namespace elixir::bionic;
  if (name == nullptr) return kLogIdMax;
  const char* base = std::strrchr(name, '/');
  base = base == nullptr ? name : base + 1;
  for (std::int32_t id = 0; id < kLogIdMax; ++id) {
    if (std::strcmp(base, kLogNames[id]) == 0) return static_cast<std::uint32_t>(id);
  }
  return kLogIdMax;
}

ELIXIR_BIONIC_FN(void, __android_log_write_log_message)(GuestLogMessage* message) noexcept {
  using namespace elixir::bionic;
  if (message == nullptr) return;
  ErrnoRestorer errno_restorer;
  const std::int32_t buffer = message->buffer_id;
  if (buffer != kLogIdDefault && buffer != kLogIdMain && buffer != kLogIdSystem && buffer != kLogIdRadio &&
      buffer != kLogIdCrash) {
    return;
  }
  std::string tag_copy;
  const bool use_default_tag = message->tag == nullptr;
  if (use_default_tag) {
    try {
      std::lock_guard<std::mutex> guard(TagLock());
      tag_copy = DefaultTag();
    } catch (...) {
    }
    message->tag = tag_copy.c_str();
  }
  const LoggerFunction logger = g_logger.load();
  if (logger != nullptr) logger(message);
  if (use_default_tag) message->tag = nullptr;
}

ELIXIR_BIONIC_FN(int, __android_log_buf_write)(int buffer_id, int priority, const char* tag,
                                               const char* text) noexcept {
  using namespace elixir::bionic;
  ErrnoRestorer errno_restorer;
  if (!IsLoggable(priority, kLogVerbose)) return -guest::kEPERM;
  GuestLogMessage message{sizeof(GuestLogMessage), buffer_id, priority, tag, nullptr, 0, text};
  elixir_bionic___android_log_write_log_message(&message);
  return 1;
}

ELIXIR_BIONIC_FN(int, __android_log_write)(int priority, const char* tag, const char* text) noexcept {
  return elixir_bionic___android_log_buf_write(elixir::bionic::kLogIdMain, priority, tag, text);
}

extern "C" ELIXIR_SYSV ELIXIR_ASM_CALLED int elixir_format_android_log_buf_vprint(int buffer_id, int priority, const char* tag,
                                                                const char* format, SysvVaList* args) noexcept {
  using namespace elixir::bionic;
  ErrnoRestorer errno_restorer;
  if (!IsLoggable(priority, kLogVerbose)) return -guest::kEPERM;
  char buffer[kLogBufferSize];
  format::FormatToBuffer(buffer, sizeof(buffer), format, args);
  GuestLogMessage message{sizeof(GuestLogMessage), buffer_id, priority, tag, nullptr, 0, buffer};
  elixir_bionic___android_log_write_log_message(&message);
  return 1;
}

ELIXIR_BIONIC_FN(int, __android_log_vprint)(int priority, const char* tag, const char* format,
                                            SysvVaList* args) noexcept {
  return elixir_format_android_log_buf_vprint(elixir::bionic::kLogIdMain, priority, tag, format, args);
}

extern "C" [[noreturn]] ELIXIR_SYSV ELIXIR_ASM_CALLED void elixir_format_android_log_vassert(const char* condition, const char* tag,
                                                                          const char* format,
                                                                          SysvVaList* args) noexcept {
  using namespace elixir::bionic;
  char buffer[kLogBufferSize];
  if (format != nullptr) {
    format::FormatToBuffer(buffer, sizeof(buffer), format, args);
  } else if (condition != nullptr) {
    std::snprintf(buffer, sizeof(buffer), "Assertion failed: %s", condition);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%s", "Unspecified assertion failed");
  }
  elixir_bionic___android_log_write(kLogFatal, tag, buffer);
  elixir_bionic___android_log_call_aborter(buffer);
  format::internal::AbortProcess();
}

ELIXIR_BIONIC_FN(int, __android_log_bwrite)(std::int32_t , const void* , g_size ) noexcept {
  return 0;
}

ELIXIR_BIONIC_FN(int, __android_log_btwrite)(std::int32_t , char , const void* ,
                                             g_size ) noexcept {
  return 0;
}

ELIXIR_BIONIC_FN(int, __android_log_bswrite)(std::int32_t , const char* ) noexcept { return 0; }

ELIXIR_BIONIC_FN(int, __android_log_security_bwrite)(std::int32_t , const void* ,
                                                     g_size ) noexcept {
  return 0;
}

ELIXIR_BIONIC_FN(int, __android_log_security_bswrite)(std::int32_t , const char* ) noexcept {
  return 0;
}

ELIXIR_BIONIC_FN(int, __android_log_error_write)(int , const char* , std::int32_t ,
                                                 const char* , std::uint32_t ) noexcept {
  return 0;
}

ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic___android_log_print, elixir_bionic___android_log_vprint, 3)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic___android_log_buf_print, elixir_format_android_log_buf_vprint, 4)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic___android_log_assert, elixir_format_android_log_vassert, 3)

namespace elixir::bionic {

void RegisterLogExports(ExportTable& table) {
  struct Entry {
    const char* name;
    void* address;
  };
  const Entry entries[] = {
      {"__android_log_write", reinterpret_cast<void*>(&elixir_bionic___android_log_write)},
      {"__android_log_buf_write", reinterpret_cast<void*>(&elixir_bionic___android_log_buf_write)},
      {"__android_log_print", reinterpret_cast<void*>(&elixir_bionic___android_log_print)},
      {"__android_log_vprint", reinterpret_cast<void*>(&elixir_bionic___android_log_vprint)},
      {"__android_log_buf_print", reinterpret_cast<void*>(&elixir_bionic___android_log_buf_print)},
      {"__android_log_assert", reinterpret_cast<void*>(&elixir_bionic___android_log_assert)},
      {"__android_log_is_loggable", reinterpret_cast<void*>(&elixir_bionic___android_log_is_loggable)},
      {"__android_log_is_loggable_len", reinterpret_cast<void*>(&elixir_bionic___android_log_is_loggable_len)},
      {"__android_log_set_minimum_priority",
       reinterpret_cast<void*>(&elixir_bionic___android_log_set_minimum_priority)},
      {"__android_log_get_minimum_priority",
       reinterpret_cast<void*>(&elixir_bionic___android_log_get_minimum_priority)},
      {"__android_log_set_default_tag", reinterpret_cast<void*>(&elixir_bionic___android_log_set_default_tag)},
      {"__android_log_write_log_message", reinterpret_cast<void*>(&elixir_bionic___android_log_write_log_message)},
      {"__android_log_set_logger", reinterpret_cast<void*>(&elixir_bionic___android_log_set_logger)},
      {"__android_log_logd_logger", reinterpret_cast<void*>(&elixir_bionic___android_log_logd_logger)},
      {"__android_log_logd_logger_with_timestamp",
       reinterpret_cast<void*>(&elixir_bionic___android_log_logd_logger_with_timestamp)},
      {"__android_log_stderr_logger", reinterpret_cast<void*>(&elixir_bionic___android_log_stderr_logger)},
      {"__android_log_set_aborter", reinterpret_cast<void*>(&elixir_bionic___android_log_set_aborter)},
      {"__android_log_call_aborter", reinterpret_cast<void*>(&elixir_bionic___android_log_call_aborter)},
      {"__android_log_default_aborter", reinterpret_cast<void*>(&elixir_bionic___android_log_default_aborter)},
      {"__android_log_bwrite", reinterpret_cast<void*>(&elixir_bionic___android_log_bwrite)},
      {"__android_log_btwrite", reinterpret_cast<void*>(&elixir_bionic___android_log_btwrite)},
      {"__android_log_bswrite", reinterpret_cast<void*>(&elixir_bionic___android_log_bswrite)},
      {"__android_log_security_bwrite", reinterpret_cast<void*>(&elixir_bionic___android_log_security_bwrite)},
      {"__android_log_security_bswrite", reinterpret_cast<void*>(&elixir_bionic___android_log_security_bswrite)},
      {"__android_log_error_write", reinterpret_cast<void*>(&elixir_bionic___android_log_error_write)},
      {"__android_log_is_debuggable", reinterpret_cast<void*>(&elixir_bionic___android_log_is_debuggable)},
      {"__android_log_security", reinterpret_cast<void*>(&elixir_bionic___android_log_security)},
      {"__android_log_close", reinterpret_cast<void*>(&elixir_bionic___android_log_close)},
      {"android_log_id_to_name", reinterpret_cast<void*>(&elixir_bionic_android_log_id_to_name)},
      {"android_name_to_log_id", reinterpret_cast<void*>(&elixir_bionic_android_name_to_log_id)},
  };
  for (const Entry& entry : entries) table.Add(kLiblog, entry.name, entry.address);
}

}
