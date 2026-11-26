// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <async_safe/log.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "elf/loader_log.h"

namespace {

elixir_loader_log_sink g_sink = nullptr;

void Emit(int priority, const char* tag, const char* message) {
  if (g_sink != nullptr) {
    g_sink(priority, tag != nullptr ? tag : "linker", message);
    return;
  }
  fprintf(stderr, "[%s] %s\n", tag != nullptr ? tag : "linker", message);
  fflush(stderr);
}

}

extern "C" void elixir_loader_set_log_sink(elixir_loader_log_sink sink) { g_sink = sink; }

extern "C" int async_safe_format_buffer_va_list(char* buffer, size_t buffer_size, const char* format,
                                                 va_list args) {
  if (buffer_size == 0) return 0;
  const int written = vsnprintf(buffer, buffer_size, format, args);
  return written < 0 ? 0 : written;
}

extern "C" int async_safe_format_buffer(char* buffer, size_t buffer_size, const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int result = async_safe_format_buffer_va_list(buffer, buffer_size, format, args);
  va_end(args);
  return result;
}

extern "C" int async_safe_format_fd_va_list(int fd, const char* format, va_list args) {
  char buffer[1024];
  const int length = async_safe_format_buffer_va_list(buffer, sizeof(buffer), format, args);
  Emit(fd == 2 ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, "linker", buffer);
  return length;
}

extern "C" int async_safe_format_fd(int fd, const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int result = async_safe_format_fd_va_list(fd, format, args);
  va_end(args);
  return result;
}

extern "C" int async_safe_write_log(int priority, const char* tag, const char* message) {
  Emit(priority, tag, message);
  return 0;
}

extern "C" int async_safe_format_log_va_list(int priority, const char* tag, const char* format, va_list args) {
  char buffer[1024];
  async_safe_format_buffer_va_list(buffer, sizeof(buffer), format, args);
  return async_safe_write_log(priority, tag, buffer);
}

extern "C" int async_safe_format_log(int priority, const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int result = async_safe_format_log_va_list(priority, tag, format, args);
  va_end(args);
  return result;
}

extern "C" void async_safe_fatal_va_list(const char* prefix, const char* format, va_list args) {
  char buffer[1024];
  size_t used = 0;
  if (prefix != nullptr) {
    used = static_cast<size_t>(snprintf(buffer, sizeof(buffer), "%s: ", prefix));
    if (used >= sizeof(buffer)) used = sizeof(buffer) - 1;
  }
  vsnprintf(buffer + used, sizeof(buffer) - used, format, args);
  Emit(ANDROID_LOG_FATAL, "linker", buffer);
}

extern "C" void async_safe_fatal_no_abort(const char* format, ...) {
  va_list args;
  va_start(args, format);
  async_safe_fatal_va_list(nullptr, format, args);
  va_end(args);
}
