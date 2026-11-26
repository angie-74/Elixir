// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_PLATFORM_WINDOWS_CRASH_REPORTER_H_
#define ELIXIR_PLATFORM_WINDOWS_CRASH_REPORTER_H_

#include <cstdint>
#include <functional>
#include <string>

namespace elixir::platform {

struct SymbolInfo {
  std::string module;
  std::uintptr_t module_offset = 0;
  std::string symbol;
  std::uintptr_t symbol_offset = 0;
};

using Symbolizer = std::function<bool(std::uintptr_t address, SymbolInfo* info)>;

struct CrashReporterOptions {
  std::wstring directory;
  bool write_minidump = true;
  Symbolizer guest_symbolizer;
  std::string product_version;
};

bool InstallCrashReporter(const CrashReporterOptions& options);

std::wstring DefaultCrashDirectory();

std::string FormatCrashReport(const void* exception_pointers);

void ReportFatalException(const void* exception_pointers, const char* reason) noexcept;

}

#endif
