// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_CORE_PROCESS_EXT_H_
#define ELIXIR_BIONIC_CORE_PROCESS_EXT_H_

#include <cstddef>
#include <cstdint>

#include "abi/sysv_abi.h"
#include "abi/sysv_va_list.h"
#include "bionic/guest_types.h"
#include "bionic/runtime.h"

namespace elixir::bionic::core {

void RegisterProcessExtExports(ExportTable& table);
void RegisterIdentityExports(ExportTable& table);
void RegisterSyslogExports(ExportTable& table);
void RegisterSysmiscExports(ExportTable& table);

struct LateServices {
  abi::SysvFn<int(const char*, guest::Stat*)> stat = nullptr;
  abi::SysvFn<int(int, guest::Stat*)> fstat = nullptr;
  abi::SysvFn<abi::g_ssize(int, const void*, abi::g_size)> write = nullptr;
  abi::SysvFn<int(void*)> fclose = nullptr;
  abi::SysvFn<int(char*, abi::g_size, const char*, abi::SysvVaList*)> vsnprintf = nullptr;
};

void BindLateServices(const ExportTable& table) noexcept;
LateServices BoundLateServices() noexcept;

struct CrashDetailView {
  const char* name;
  std::size_t name_size;
  const char* data;
  std::size_t data_size;
};

std::size_t SnapshotCrashDetails(CrashDetailView* out, std::size_t capacity) noexcept;

}

#endif
