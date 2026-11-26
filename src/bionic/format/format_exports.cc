// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <cstdio>
#include <cstring>
#include <string>

#include "abi/variadic_thunk.h"
#include "bionic/format.h"
#include "bionic/format/format_internal.h"
#include "bionic/guest_constants.h"
#include "bionic/memory.h"
#include "bionic/runtime.h"

namespace elixir::bionic {
namespace {

using abi::g_size;
using abi::g_wchar;
using abi::SysvVaList;

constexpr g_size kSsizeMax = 0x7fffffffffffffffull;
constexpr g_size kSizeMax = ~g_size{0};

[[noreturn]] void FortifyFatal(const char* text) noexcept {
  std::string message = "FORTIFY: ";
  message += text;
  format::internal::FatalError(message.c_str());
}

void CheckCount(const char* function, const char* identifier, g_size value) noexcept {
  if (value > kSsizeMax) {
    char text[160];
    std::snprintf(text, sizeof(text), "%s: %s %llu > SSIZE_MAX", function, identifier,
                  static_cast<unsigned long long>(value));
    FortifyFatal(text);
  }
}

void CheckBufferAccess(const char* function, const char* action, g_size claim, g_size actual) noexcept {
  if (claim > actual) {
    char text[200];
    std::snprintf(text, sizeof(text), "%s: prevented %llu-byte %s %llu-byte buffer", function,
                  static_cast<unsigned long long>(claim), action, static_cast<unsigned long long>(actual));
    FortifyFatal(text);
  }
}

class StringSink final : public format::Sink {
 public:
  bool Write(const char* data, std::size_t length) override {
    text.append(data, length);
    return true;
  }
  std::string text;
};

class StringSource final : public format::Source {
 public:
  explicit StringSource(const char* text) : text_(reinterpret_cast<const unsigned char*>(text)), length_(std::strlen(text)) {}
  int Get() override { return position_ < length_ ? text_[position_++] : -1; }
  void Unget(int) override {
    if (position_ > 0) --position_;
  }

 private:
  const unsigned char* text_;
  std::size_t length_;
  std::size_t position_ = 0;
};

class WideStringSource final : public format::WideSource {
 public:
  WideStringSource(const g_wchar* text, std::size_t length) : text_(text), length_(length) {}
  std::int64_t Get() override { return position_ < length_ ? static_cast<std::int64_t>(text_[position_++]) : -1; }
  void Unget(std::int64_t) override {
    if (position_ > 0) --position_;
  }

 private:
  const g_wchar* text_;
  std::size_t length_;
  std::size_t position_ = 0;
};

}
}

using elixir::abi::g_size;
using elixir::abi::g_wchar;
using elixir::abi::SysvVaList;

ELIXIR_BIONIC_FN(int, vsnprintf)(char* buffer, g_size size, const char* format, SysvVaList* args) noexcept {
  elixir::bionic::CheckCount("vsnprintf", "size", size);
  return elixir::bionic::format::FormatToBuffer(buffer, size, format, args);
}

ELIXIR_BIONIC_FN(int, vsprintf)(char* buffer, const char* format, SysvVaList* args) noexcept {
  return elixir::bionic::format::FormatToBuffer(buffer, elixir::bionic::kSsizeMax, format, args);
}

ELIXIR_BIONIC_FN(int, vasprintf)(char** out, const char* format, SysvVaList* args) noexcept {
  using namespace elixir::bionic;
  try {
    StringSink sink;
    const int result = format::Format(sink, format, args);
    if (result >= 0) {
      auto* block = static_cast<char*>(GuestMalloc(sink.text.size() + 1));
      if (block != nullptr) {
        std::memcpy(block, sink.text.data(), sink.text.size());
        block[sink.text.size()] = '\0';
        *out = block;
        return result;
      }
    }
    if (result >= 0 || GuestErrno() != guest::kEINVAL) SetGuestErrno(guest::kENOMEM);
  } catch (...) {
    SetGuestErrno(guest::kENOMEM);
  }
  *out = nullptr;
  return -1;
}

ELIXIR_BIONIC_FN(int, vsscanf)(const char* input, const char* format, SysvVaList* args) noexcept {
  elixir::bionic::StringSource source(input);
  return elixir::bionic::format::Scan(source, format, args);
}

ELIXIR_BIONIC_FN(int, vswprintf)(g_wchar* buffer, g_size size, const g_wchar* format, SysvVaList* args) noexcept {
  return elixir::bionic::format::FormatWideToBuffer(buffer, size, format, args);
}

ELIXIR_BIONIC_FN(int, vswscanf)(const g_wchar* input, const g_wchar* format, SysvVaList* args) noexcept {
  using namespace elixir::bionic;
  std::size_t length = 0;
  while (input[length] != 0) {
    const g_wchar c = input[length];
    if (c > 0x1fffff) return FailWith(guest::kEILSEQ, -1);
    if ((c >= 0xd800 && c <= 0xdfff) || c > 0x10ffff) break;
    ++length;
  }
  WideStringSource source(input, length);
  return format::ScanWide(source, format, args);
}

ELIXIR_BIONIC_FN(int, __vsnprintf_chk)(char* buffer, g_size supplied_size, int , g_size buffer_size,
                                       const char* format, SysvVaList* args) noexcept {
  elixir::bionic::CheckBufferAccess("vsnprintf", "write into", supplied_size, buffer_size);
  return elixir_bionic_vsnprintf(buffer, supplied_size, format, args);
}

ELIXIR_BIONIC_FN(int, __vsprintf_chk)(char* buffer, int , g_size buffer_size, const char* format,
                                      SysvVaList* args) noexcept {
  using namespace elixir::bionic;
  const int result =
      elixir_bionic_vsnprintf(buffer, buffer_size == kSizeMax ? kSsizeMax : buffer_size, format, args);
  CheckBufferAccess("vsprintf", "write into", static_cast<g_size>(static_cast<std::int64_t>(result) + 1),
                    buffer_size);
  return result;
}

ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_sprintf, elixir_bionic_vsprintf, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_snprintf, elixir_bionic_vsnprintf, 3)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_asprintf, elixir_bionic_vasprintf, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_sscanf, elixir_bionic_vsscanf, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_swprintf, elixir_bionic_vswprintf, 3)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_swscanf, elixir_bionic_vswscanf, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic___sprintf_chk, elixir_bionic___vsprintf_chk, 4)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic___snprintf_chk, elixir_bionic___vsnprintf_chk, 5)

namespace elixir::bionic {

void RegisterFormatExports(ExportTable& table) {
  struct Entry {
    const char* name;
    void* address;
  };
  const Entry entries[] = {
      {"sprintf", reinterpret_cast<void*>(&elixir_bionic_sprintf)},
      {"snprintf", reinterpret_cast<void*>(&elixir_bionic_snprintf)},
      {"vsprintf", reinterpret_cast<void*>(&elixir_bionic_vsprintf)},
      {"vsnprintf", reinterpret_cast<void*>(&elixir_bionic_vsnprintf)},
      {"asprintf", reinterpret_cast<void*>(&elixir_bionic_asprintf)},
      {"vasprintf", reinterpret_cast<void*>(&elixir_bionic_vasprintf)},
      {"sscanf", reinterpret_cast<void*>(&elixir_bionic_sscanf)},
      {"vsscanf", reinterpret_cast<void*>(&elixir_bionic_vsscanf)},
      {"swprintf", reinterpret_cast<void*>(&elixir_bionic_swprintf)},
      {"vswprintf", reinterpret_cast<void*>(&elixir_bionic_vswprintf)},
      {"swscanf", reinterpret_cast<void*>(&elixir_bionic_swscanf)},
      {"vswscanf", reinterpret_cast<void*>(&elixir_bionic_vswscanf)},
      {"__sprintf_chk", reinterpret_cast<void*>(&elixir_bionic___sprintf_chk)},
      {"__snprintf_chk", reinterpret_cast<void*>(&elixir_bionic___snprintf_chk)},
      {"__vsprintf_chk", reinterpret_cast<void*>(&elixir_bionic___vsprintf_chk)},
      {"__vsnprintf_chk", reinterpret_cast<void*>(&elixir_bionic___vsnprintf_chk)},
  };
  for (const Entry& entry : entries) table.Add(kLibc, entry.name, entry.address);
}

}
