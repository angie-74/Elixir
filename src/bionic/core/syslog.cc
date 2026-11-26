// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The logic follows bionic libc/bionic/syslog.cpp, whose notice is:
//
//   Copyright (C) 2014 The Android Open Source Project
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
#include <atomic>
#include <cstring>
#include <string>

#include "abi/sysv_va_list.h"
#include "abi/variadic_thunk.h"
#include "bionic/core/core_internal.h"
#include "bionic/core/process_ext.h"
#include "bionic/runtime.h"

namespace elixir::bionic {

ELIXIR_BIONIC_FN(const char*, getprogname)() noexcept;

namespace core {
namespace {

constexpr int kLogErr = 3, kLogWarning = 4, kLogInfo = 6;
constexpr int kLogPrimask = 7;
constexpr int kLogPerror = 0x20;
constexpr int kAndroidLogDebug = 3, kAndroidLogInfo = 4, kAndroidLogWarn = 5, kAndroidLogError = 6;
constexpr int kGuestStderr = 2;

std::atomic<const char*> g_log_tag{nullptr};
std::atomic<int> g_priority_mask{0xff};
std::atomic<int> g_options{0};

int AndroidPriority(int priority) noexcept {
  priority &= kLogPrimask;
  if (priority <= kLogErr) return kAndroidLogError;
  if (priority == kLogWarning) return kAndroidLogWarn;
  if (priority <= kLogInfo) return kAndroidLogInfo;
  return kAndroidLogDebug;
}

void WriteGuestStderr(const std::string& text) noexcept {
  const LateServices services = BoundLateServices();
  if (services.write == nullptr) {
    WriteHostStderr(text.data(), text.size());
    return;
  }
  const int saved = GuestErrno();
  std::size_t written = 0;
  while (written < text.size()) {
    const abi::g_ssize n = services.write(kGuestStderr, text.data() + written, text.size() - written);
    if (n <= 0) break;
    written += static_cast<std::size_t>(n);
  }
  SetGuestErrno(saved);
}

}
}

ELIXIR_BIONIC_FN(void, closelog)() noexcept {
  core::g_log_tag.store(nullptr);
  core::g_options.store(0);
}

ELIXIR_BIONIC_FN(void, openlog)(const char* log_tag, int options, int facility) noexcept {
  (void)facility;
  core::g_log_tag.store(log_tag);
  core::g_options.store(options);
}

ELIXIR_BIONIC_FN(int, setlogmask)(int new_mask) noexcept {
  if (new_mask == 0) return core::g_priority_mask.load();
  return core::g_priority_mask.exchange(new_mask);
}

ELIXIR_BIONIC_FN(void, vsyslog)(int priority, const char* format, abi::SysvVaList* args) noexcept {
  using namespace core;
  if ((g_priority_mask.load() & (1 << (priority & kLogPrimask))) == 0) return;
  const char* log_tag = g_log_tag.load();
  if (log_tag == nullptr) log_tag = elixir_bionic_getprogname();
  if (log_tag == nullptr) log_tag = "";
  const int android_priority = AndroidPriority(priority);

  char log_line[1024];
  int n;
  const LateServices services = BoundLateServices();
  if (services.vsnprintf != nullptr) {
    n = services.vsnprintf(log_line, sizeof(log_line), format != nullptr ? format : "", args);
  } else {
    const std::size_t length = format != nullptr ? std::strlen(format) : 0;
    const std::size_t copied = length < sizeof(log_line) ? length : sizeof(log_line) - 1;
    if (copied != 0) std::memcpy(log_line, format, copied);
    log_line[copied] = '\0';
    n = static_cast<int>(length);
  }
  if (n < 0) return;

  HostLog(android_priority, log_tag, log_line);
  if ((g_options.load() & kLogPerror) != 0) {
    const bool have_newline = n > 0 && n < static_cast<int>(sizeof(log_line)) && log_line[n - 1] == '\n';
    try {
      std::string text(log_tag);
      text += ": ";
      text += log_line;
      if (!have_newline) text += '\n';
      WriteGuestStderr(text);
    } catch (...) {
    }
  }
}

ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_syslog, elixir_bionic_vsyslog, 2)

namespace core {

void RegisterSyslogExports(ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(openlog);
  ELIXIR_ADD(closelog);
  ELIXIR_ADD(setlogmask);
  ELIXIR_ADD(syslog);
  ELIXIR_ADD(vsyslog);
#undef ELIXIR_ADD
}

}
}
