// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <cstring>

#include "bionic/files/stdio_internal.h"
#include "bionic/text.h"

namespace elixir::bionic::files {
namespace {

std::size_t SequenceLength(unsigned char lead) {
  if (lead < 0x80) return 1;
  if (lead >= 0xC2 && lead <= 0xDF) return 2;
  if (lead >= 0xE0 && lead <= 0xEF) return 3;
  if (lead >= 0xF0 && lead <= 0xF4) return 4;
  return 0;
}

std::uint32_t Malformed(Stream* s) {
  s->error = true;
  SetGuestErrno(guest::kEILSEQ);
  return kWEOF;
}

}

std::uint32_t StreamGetwcLocked(Stream* s) {
  if (s->orientation == 0) s->orientation = 1;
  const int first = StreamGetcLocked(s);
  if (first == guest::kEOF) return kWEOF;
  char bytes[4] = {static_cast<char>(first)};
  const std::size_t length = SequenceLength(static_cast<unsigned char>(first));
  if (length == 0) return Malformed(s);
  for (std::size_t i = 1; i < length; ++i) {
    const int next = StreamGetcLocked(s);
    if (next == guest::kEOF) return Malformed(s);
    if ((next & 0xC0) != 0x80) {
      StreamUngetcLocked(s, next);
      return Malformed(s);
    }
    bytes[i] = static_cast<char>(next);
  }
  abi::g_wchar wc = 0;
  const std::size_t used = text::DecodeUtf8(bytes, length, &wc);
  if (used == static_cast<std::size_t>(-1) || used == static_cast<std::size_t>(-2)) return Malformed(s);
  return wc;
}

std::uint32_t StreamPutwcLocked(Stream* s, std::uint32_t wc) {
  if (s->orientation == 0) s->orientation = 1;
  char bytes[4];
  const std::size_t length = text::EncodeUtf8(wc, bytes);
  if (length == static_cast<std::size_t>(-1) || length == 0 || length > 4) return Malformed(s);
  return StreamWriteLocked(s, bytes, length) == length ? wc : kWEOF;
}

std::uint32_t StreamUngetwcLocked(Stream* s, std::uint32_t wc) {
  if (wc == kWEOF) return kWEOF;
  char bytes[4];
  const std::size_t length = text::EncodeUtf8(wc, bytes);
  if (length == static_cast<std::size_t>(-1) || length == 0 || length > 4) {
    SetGuestErrno(guest::kEILSEQ);
    return kWEOF;
  }
  if (s->orientation == 0) s->orientation = 1;
  for (std::size_t i = length; i > 0; --i) {
    if (StreamUngetcLocked(s, static_cast<unsigned char>(bytes[i - 1])) == guest::kEOF) return kWEOF;
  }
  return wc;
}

using GFile = guest::File;

ELIXIR_BIONIC_FN(abi::g_wint, fgetwc)(GFile* file) noexcept {
  return Guarded<abi::g_wint>(kWEOF, [&]() -> abi::g_wint {
    Stream* s = StreamOf(file);
    if (s == nullptr) return kWEOF;
    StreamLock lock(s);
    return StreamGetwcLocked(s);
  });
}
ELIXIR_BIONIC_FN(abi::g_wint, getwchar)() noexcept { return elixir_bionic_fgetwc(StdVariable(0)); }

ELIXIR_BIONIC_FN(abi::g_wint, fputwc)(abi::g_wchar wc, GFile* file) noexcept {
  return Guarded<abi::g_wint>(kWEOF, [&]() -> abi::g_wint {
    Stream* s = StreamOf(file);
    if (s == nullptr) return kWEOF;
    StreamLock lock(s);
    return StreamPutwcLocked(s, wc);
  });
}
ELIXIR_BIONIC_FN(abi::g_wint, putwchar)(abi::g_wchar wc) noexcept { return elixir_bionic_fputwc(wc, StdVariable(1)); }

ELIXIR_BIONIC_FN(abi::g_wint, ungetwc)(abi::g_wint wc, GFile* file) noexcept {
  return Guarded<abi::g_wint>(kWEOF, [&]() -> abi::g_wint {
    Stream* s = StreamOf(file);
    if (s == nullptr) return kWEOF;
    StreamLock lock(s);
    return StreamUngetwcLocked(s, wc);
  });
}

ELIXIR_BIONIC_FN(abi::g_wchar*, fgetws)(abi::g_wchar* buffer, int size, GFile* file) noexcept {
  return Guarded(static_cast<abi::g_wchar*>(nullptr), [&]() -> abi::g_wchar* {
    Stream* s = StreamOf(file);
    if (s == nullptr) return nullptr;
    if (buffer == nullptr || size <= 0) return FailWith(guest::kEINVAL, static_cast<abi::g_wchar*>(nullptr));
    StreamLock lock(s);
    int length = 0;
    while (length < size - 1) {
      const std::uint32_t wc = StreamGetwcLocked(s);
      if (wc == kWEOF) {
        if (s->error) return nullptr;
        break;
      }
      buffer[length++] = wc;
      if (wc == L'\n') break;
    }
    if (length == 0 && size > 1) return nullptr;
    buffer[length] = 0;
    return buffer;
  });
}

ELIXIR_BIONIC_FN(int, fputws)(const abi::g_wchar* text, GFile* file) noexcept {
  return Guarded(-1, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return -1;
    if (text == nullptr) return FailWith(guest::kEFAULT, -1);
    StreamLock lock(s);
    for (const abi::g_wchar* p = text; *p != 0; ++p) {
      if (StreamPutwcLocked(s, *p) == kWEOF) return -1;
    }
    return 0;
  });
}

void RegisterStdioWideExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("fgetwc", &elixir_bionic_fgetwc);
  add("getwc", &elixir_bionic_fgetwc);
  add("getwchar", &elixir_bionic_getwchar);
  add("fputwc", &elixir_bionic_fputwc);
  add("putwc", &elixir_bionic_fputwc);
  add("putwchar", &elixir_bionic_putwchar);
  add("ungetwc", &elixir_bionic_ungetwc);
  add("fgetws", &elixir_bionic_fgetws);
  add("fputws", &elixir_bionic_fputws);
}

}
