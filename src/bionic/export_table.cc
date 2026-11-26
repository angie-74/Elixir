// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/export_table.h"

#include "bionic/raw_syscalls.h"

#include <windows.h>

#include <mcpelauncher/linker.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

#include "bionic/core/core.h"
#include "bionic/files/files.h"
#include "bionic/files/stdio.h"
#include "bionic/mm/mm.h"
#include "bionic/net.h"
#include "bionic/text/environment.h"
#include "bionic/vfs.h"
#include "elf/guest_images.h"

namespace elixir::bionic {
namespace {

constexpr int kAndroidLogError = 6;

std::atomic<std::uintptr_t> g_host_image_begin{0};
std::atomic<std::uintptr_t> g_host_image_end{0};

void RecordHostImage() noexcept {
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&RecordHostImage), &module)) {
    return;
  }
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + static_cast<std::uintptr_t>(dos->e_lfanew));
  g_host_image_end.store(base + nt->OptionalHeader.SizeOfImage, std::memory_order_release);
  g_host_image_begin.store(base, std::memory_order_release);
}

bool DeliverFaultsFrom(std::uintptr_t rip) noexcept {
  if (elf::IsGuestCode(rip)) return true;
  if (mm::IsGuestMemory(rip)) return true;
  return rip >= g_host_image_begin.load(std::memory_order_acquire) &&
         rip < g_host_image_end.load(std::memory_order_acquire);
}

void ObserveGuestSegment(const elf::GuestSegment& segment, bool loaded) {
  try {
    if (!loaded) {
      files::UnregisterMappedRange(segment.start, segment.end);
      return;
    }
    files::MappedSegment mapped;
    mapped.start = segment.start;
    mapped.end = segment.end;
    mapped.file_offset = segment.file_offset;
    mapped.guest_path = vfs::HostToGuest(segment.path != nullptr ? segment.path : "");
    files::RegisterMappedSegment(mapped);
  } catch (...) {
  }
}

bool GuestNameForHostPath(const char* host_path, std::string* guest_path) {
  std::string mapped = vfs::HostToGuest(host_path);
  if (mapped.empty() || mapped[0] != '/') return false;
  *guest_path = std::move(mapped);
  return true;
}

bool HostPathForGuestPath(const char* guest_path, std::string* host_path) {
  int guest_errno = 0;
  std::string resolved;
  if (!vfs::ResolveToHost(guest_path, &resolved, &guest_errno) || resolved.empty()) return false;
  *host_path = std::move(resolved);
  return true;
}

void FlushGuestStreamsAtExit() { stdio::FlushAllStreams(); }

}

ExportTable BuildExportTable() {
  ExportTable table;
  RegisterCoreExports(table);
  RegisterStringExports(table);
  RegisterStdlibExports(table);
  RegisterMathExports(table);
  RegisterTimeExports(table);
  RegisterFormatExports(table);
  RegisterThreadExports(table);
  RegisterFileExports(table);
  net::RegisterNetExports(table);
  RegisterStdioExports(table);
  RegisterSystemExports(table);
  RegisterLogExports(table);
  RegisterMemoryMapExports(table);
  for (const std::string& conflict : table.conflicts()) {
    HostLog(kAndroidLogError, "elixir",
            ("bionic export registered twice with different addresses: " + conflict).c_str());
  }
  return table;
}

bool InitializeGuestLibc(const RuntimeOptions& options, ExportTable* table) {
  if (!InitializeRuntime(options)) return false;
  static std::once_flag wiring;
  std::call_once(wiring, [&options] {
    text::SetGuestProgramName(options.package_name.c_str());
    core::AddExitHook(&FlushGuestStreamsAtExit);
    RecordHostImage();
    core::SetFaultDeliveryFilter(&DeliverFaultsFrom);
    elf::SetGuestSegmentObserver(&ObserveGuestSegment);
    elf::SetGuestPathHooks(&GuestNameForHostPath, &HostPathForGuestPath);
  });
  if (table != nullptr) {
    *table = BuildExportTable();
    InstallRawSyscalls(*table);
  }
  return true;
}

std::size_t InstallSyntheticLibraries(const ExportTable& table) {
  std::size_t libraries = 0;
  for (const auto& [soname, symbols] : table.libraries()) {
    if (soname == kLibdl) continue;
    std::unordered_map<std::string, void*> map(symbols.begin(), symbols.end());
    linker::load_library(soname.c_str(), map);
    ++libraries;
  }
  return libraries;
}

}
