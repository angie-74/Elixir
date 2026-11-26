// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_RUNTIME_H_
#define ELIXIR_BIONIC_RUNTIME_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "abi/sysv_abi.h"
#include "bionic/guest_constants.h"
#include "bionic/guest_types.h"

#define ELIXIR_BIONIC_FN(return_type, name) extern "C" ELIXIR_SYSV return_type elixir_bionic_##name

namespace elixir::bionic {

int* GuestErrnoLocation() noexcept;
inline void SetGuestErrno(int value) noexcept { *GuestErrnoLocation() = value; }
inline int GuestErrno() noexcept { return *GuestErrnoLocation(); }

template <typename T>
inline T FailWith(int guest_errno, T failure) noexcept {
  SetGuestErrno(guest_errno);
  return failure;
}

int ErrnoFromWin32(std::uint32_t win32_error) noexcept;
int ErrnoFromWinsock(int wsa_error) noexcept;
int ErrnoFromCrt(int crt_errno) noexcept;

class ExportTable {
 public:
  void Add(std::string_view soname, std::string_view name, void* address);
  void* Find(std::string_view soname, std::string_view name) const;
  const std::unordered_map<std::string, std::unordered_map<std::string, void*>>& libraries() const {
    return libraries_;
  }
  std::size_t size() const;
  const std::vector<std::string>& conflicts() const { return conflicts_; }

 private:
  std::unordered_map<std::string, std::unordered_map<std::string, void*>> libraries_;
  std::vector<std::string> conflicts_;
};

inline constexpr std::string_view kLibc = "libc.so";
inline constexpr std::string_view kLibm = "libm.so";
inline constexpr std::string_view kLibdl = "libdl.so";
inline constexpr std::string_view kLiblog = "liblog.so";
inline constexpr std::string_view kLibz = "libz.so";
inline constexpr std::string_view kLibandroid = "libandroid.so";

void RegisterCoreExports(ExportTable& table);
void RegisterStringExports(ExportTable& table);
void RegisterStdlibExports(ExportTable& table);
void RegisterMathExports(ExportTable& table);
void RegisterTimeExports(ExportTable& table);
void RegisterFormatExports(ExportTable& table);
void RegisterThreadExports(ExportTable& table);
void RegisterFileExports(ExportTable& table);
void RegisterStdioExports(ExportTable& table);
void RegisterSystemExports(ExportTable& table);
void RegisterLogExports(ExportTable& table);
void RegisterMemoryMapExports(ExportTable& table);

ExportTable BuildExportTable();

struct RuntimeOptions {
  std::string package_name = "com.elixir.guest";
  int sdk_version = 35;
  void (*log_sink)(int priority, const char* tag, const char* message) = nullptr;
};

bool InitializeRuntime(const RuntimeOptions& options);
const RuntimeOptions& Options();

void HostLog(int priority, const char* tag, const char* message) noexcept;

std::string FutexStatisticsReport();

std::string MmStatisticsReport();

}

#endif
