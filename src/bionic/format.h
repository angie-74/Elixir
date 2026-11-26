// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_FORMAT_H_
#define ELIXIR_BIONIC_FORMAT_H_

#include <cstddef>
#include <cstdint>

#include "abi/sysv_va_list.h"

namespace elixir::bionic::format {

class Sink {
 public:
  virtual ~Sink() = default;
  virtual bool Write(const char* data, std::size_t length) = 0;
};

class WideSink {
 public:
  virtual ~WideSink() = default;
  virtual bool Write(const abi::g_wchar* data, std::size_t length) = 0;
};

class Source {
 public:
  virtual ~Source() = default;
  virtual int Get() = 0;
  virtual void Unget(int c) = 0;
};

class WideSource {
 public:
  virtual ~WideSource() = default;
  virtual std::int64_t Get() = 0;
  virtual void Unget(std::int64_t c) = 0;
};

int Format(Sink& sink, const char* format, abi::SysvVaList* args) noexcept;
int FormatWide(WideSink& sink, const abi::g_wchar* format, abi::SysvVaList* args) noexcept;

int FormatToBuffer(char* buffer, std::size_t size, const char* format, abi::SysvVaList* args) noexcept;
int FormatWideToBuffer(abi::g_wchar* buffer, std::size_t size, const abi::g_wchar* format,
                       abi::SysvVaList* args) noexcept;

int Scan(Source& source, const char* format, abi::SysvVaList* args) noexcept;
int ScanWide(WideSource& source, const abi::g_wchar* format, abi::SysvVaList* args) noexcept;

}

#endif
