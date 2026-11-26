// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>

#include "bionic/mm/mm_internal.h"
#include "bionic/vfs.h"

namespace elixir::bionic::mm {
namespace {

std::mutex g_memfd_mutex;
std::map<std::wstring, std::string> g_memfd_names;
std::size_t g_prune_at = 64;

void PruneLocked() {
  if (g_memfd_names.size() < g_prune_at) return;
  for (auto it = g_memfd_names.begin(); it != g_memfd_names.end();) {
    if (GetFileAttributesW(it->first.c_str()) == INVALID_FILE_ATTRIBUTES &&
        (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)) {
      it = g_memfd_names.erase(it);
    } else {
      ++it;
    }
  }
  g_prune_at = g_memfd_names.size() * 2 > 64 ? g_memfd_names.size() * 2 : 64;
}

std::wstring FinalPath(HANDLE handle) {
  wchar_t buffer[MAX_PATH + 64];
  const DWORD length = GetFinalPathNameByHandleW(handle, buffer, MAX_PATH + 64, FILE_NAME_NORMALIZED);
  if (length == 0 || length >= MAX_PATH + 64) return {};
  return std::wstring(buffer, length);
}

std::wstring TempDirectory() {
  wchar_t buffer[MAX_PATH + 1];
  const DWORD length = GetTempPathW(MAX_PATH + 1, buffer);
  if (length == 0 || length > MAX_PATH) return L"C:\\Windows\\Temp\\";
  return std::wstring(buffer, length);
}

}

std::string MemfdName(HANDLE handle) {
  const std::wstring path = FinalPath(handle);
  if (path.empty()) return {};
  std::lock_guard<std::mutex> guard(g_memfd_mutex);
  auto it = g_memfd_names.find(path);
  return it == g_memfd_names.end() ? std::string() : "/" + it->second + " (deleted)";
}

std::size_t MemfdNameCount() {
  std::lock_guard<std::mutex> guard(g_memfd_mutex);
  return g_memfd_names.size();
}

}

namespace mm = elixir::bionic::mm;
namespace guest = elixir::bionic::guest;
using elixir::bionic::FailWith;

ELIXIR_BIONIC_FN(int, memfd_create)(const char* name, unsigned flags) noexcept {
  return mm::Guarded(-1, [&] {
    constexpr unsigned kAll =
        mm::kMFD_CLOEXEC | mm::kMFD_ALLOW_SEALING | mm::kMFD_HUGETLB | mm::kMFD_NOEXEC_SEAL | mm::kMFD_EXEC;
    if ((flags & mm::kMFD_HUGETLB) == 0) {
      if ((flags & ~kAll) != 0) return FailWith(guest::kEINVAL, -1);
    } else if ((flags & ~(kAll | (mm::kMFD_HUGE_MASK << mm::kMFD_HUGE_SHIFT))) != 0) {
      return FailWith(guest::kEINVAL, -1);
    }
    if ((flags & mm::kMFD_EXEC) != 0 && (flags & mm::kMFD_NOEXEC_SEAL) != 0) return FailWith(guest::kEINVAL, -1);
    if (name == nullptr) return FailWith(guest::kEFAULT, -1);
    if (::strnlen(name, mm::kMFD_NAME_MAX_LEN + 1) > mm::kMFD_NAME_MAX_LEN) return FailWith(guest::kEINVAL, -1);
    if ((flags & mm::kMFD_HUGETLB) != 0) return FailWith(guest::kENOSYS, -1);

    static std::atomic<std::uint64_t> counter{0};
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    const std::wstring path = mm::TempDirectory() + L"elixir-memfd-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                              std::to_wstring(counter.fetch_add(1)) + L"-" +
                              std::to_wstring(static_cast<std::uint64_t>(now.QuadPart)) + L".tmp";
    const bool exec = (flags & mm::kMFD_NOEXEC_SEAL) == 0;
    const DWORD access = GENERIC_READ | GENERIC_WRITE | (exec ? FILE_EXECUTE : 0);
    HANDLE handle = CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                CREATE_NEW,
                                FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_FLAG_DELETE_ON_CLOSE,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      const DWORD error = GetLastError();
      return FailWith(error == ERROR_DISK_FULL ? guest::kENOSPC : guest::kENOMEM, -1);
    }
    const std::wstring final_path = mm::FinalPath(handle);
    {
      std::lock_guard<std::mutex> guard(mm::g_memfd_mutex);
      mm::PruneLocked();
      mm::g_memfd_names[final_path] = std::string("memfd:") + name;
    }
    const int fd = elixir::bionic::vfs::InstallHandle(
        handle, elixir::bionic::vfs::FdKind::kFile,
        guest::kO_RDWR | ((flags & mm::kMFD_CLOEXEC) != 0 ? guest::kO_CLOEXEC : 0));
    if (fd < 0) {
      CloseHandle(handle);
      std::lock_guard<std::mutex> guard(mm::g_memfd_mutex);
      mm::g_memfd_names.erase(final_path);
      return -1;
    }
    return fd;
  });
}

namespace elixir::bionic::mm {

void RegisterMemfdExports(ExportTable& table) {
  table.Add(kLibc, "memfd_create", reinterpret_cast<void*>(&elixir_bionic_memfd_create));
}

}
