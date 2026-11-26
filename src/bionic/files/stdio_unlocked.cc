// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/files/stdio_internal.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace elixir::bionic::files {

using GFile = guest::File;

ELIXIR_BIONIC_FN(int, fflush)(GFile* file) noexcept;

namespace {

char* FgetsUnlocked(char* buffer, int size, Stream* s) {
  char* out = buffer;
  std::size_t remaining = static_cast<std::size_t>(size) - 1;
  while (remaining > 0) {
    if (s->mode == Stream::Mode::kReading && s->pushback.empty() && s->rpos < s->rend) {
      const unsigned char* start = s->buffer + s->rpos;
      const std::size_t available = std::min(s->rend - s->rpos, remaining);
      const void* newline = std::memchr(start, '\n', available);
      const std::size_t take =
          newline != nullptr ? static_cast<std::size_t>(static_cast<const unsigned char*>(newline) - start) + 1
                             : available;
      std::memcpy(out, start, take);
      out += take;
      remaining -= take;
      s->rpos += take;
      if (newline != nullptr) break;
      continue;
    }
    const int c = StreamGetcLocked(s);
    if (c == guest::kEOF) {
      if (out == buffer) return nullptr;
      break;
    }
    *out++ = static_cast<char>(c);
    --remaining;
    if (c == '\n') break;
  }
  *out = '\0';
  return buffer;
}

}

ELIXIR_BIONIC_FN(int, fflush_unlocked)(GFile* file) noexcept {
  if (file == nullptr) return elixir_bionic_fflush(nullptr);
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return guest::kEOF;
    if (!s->writable) return FailWith(guest::kEBADF, guest::kEOF);
    return StreamFlushLocked(s);
  });
}

ELIXIR_BIONIC_FN(int, fgetc_unlocked)(GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    return s == nullptr ? guest::kEOF : StreamGetcLocked(s);
  });
}

ELIXIR_BIONIC_FN(char*, fgets_unlocked)(char* buffer, int size, GFile* file) noexcept {
  if (size <= 0) {
    char what[64];
    std::snprintf(what, sizeof(what), "buffer size %d <= 0", size);
    FortifyFatal("fgets", what);
  }
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    Stream* s = StreamOf(file);
    if (s == nullptr) return nullptr;
    if (buffer == nullptr) return FailWith(guest::kEINVAL, static_cast<char*>(nullptr));
    return FgetsUnlocked(buffer, size, s);
  });
}

ELIXIR_BIONIC_FN(int, fputc_unlocked)(int c, GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    return s == nullptr ? guest::kEOF : StreamPutcLocked(s, c);
  });
}

ELIXIR_BIONIC_FN(int, fputs_unlocked)(const char* text, GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return guest::kEOF;
    if (text == nullptr) return FailWith(guest::kEFAULT, guest::kEOF);
    const std::size_t length = std::strlen(text);
    return StreamWriteLocked(s, text, length) == length ? 0 : guest::kEOF;
  });
}

ELIXIR_BIONIC_FN(abi::g_size, fread_unlocked)(void* buffer, abi::g_size size, abi::g_size count,
                                              GFile* file) noexcept {
  return Guarded<abi::g_size>(0, [&]() -> abi::g_size {
    Stream* s = StreamOf(file);
    if (s == nullptr) return 0;
    if (size == 0 || count == 0) return 0;
    if (count > static_cast<abi::g_size>(INT64_MAX) / size) {
      s->error = true;
      return FailWith(guest::kEOVERFLOW, static_cast<abi::g_size>(0));
    }
    return StreamReadLocked(s, buffer, size * count) / size;
  });
}

ELIXIR_BIONIC_FN(abi::g_size, fwrite_unlocked)(const void* buffer, abi::g_size size, abi::g_size count,
                                               GFile* file) noexcept {
  return Guarded<abi::g_size>(0, [&]() -> abi::g_size {
    Stream* s = StreamOf(file);
    if (s == nullptr) return 0;
    if (size == 0 || count == 0) return 0;
    if (count > static_cast<abi::g_size>(INT64_MAX) / size) {
      s->error = true;
      return FailWith(guest::kEOVERFLOW, static_cast<abi::g_size>(0));
    }
    return StreamWriteLocked(s, buffer, size * count) / size;
  });
}

void RegisterStdioUnlockedExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("fflush_unlocked", &elixir_bionic_fflush_unlocked);
  add("fgetc_unlocked", &elixir_bionic_fgetc_unlocked);
  add("fgets_unlocked", &elixir_bionic_fgets_unlocked);
  add("fputc_unlocked", &elixir_bionic_fputc_unlocked);
  add("fputs_unlocked", &elixir_bionic_fputs_unlocked);
  add("fread_unlocked", &elixir_bionic_fread_unlocked);
  add("fwrite_unlocked", &elixir_bionic_fwrite_unlocked);
}

}
