// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elf/guest_images.h"

#include "elf/guest_image_check.h"
#include "platform/windows/guest_unwind_barrier.h"
#include "platform/windows/raw_syscall_trap.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elixir::elf {
namespace {

constexpr std::size_t kMaxCodeRanges = 1024;

struct CodeRange {
  std::atomic<std::uintptr_t> start{0};
  std::atomic<std::uintptr_t> end{0};
};

CodeRange g_ranges[kMaxCodeRanges];
std::atomic<std::size_t> g_high_water{0};
std::atomic<std::size_t> g_count{0};
std::atomic<GuestSegmentObserver> g_observer{nullptr};
std::mutex g_writers;

struct CodeFileRange {
  std::uintptr_t start = 0;
  std::uintptr_t end = 0;
  std::uint64_t file_offset = 0;
  std::uint64_t file_bytes = 0;
  std::string path;
};
std::vector<CodeFileRange>* g_code_files = nullptr;

std::wstring WidePath(const std::string& utf8) {
  if (utf8.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(length), L' ');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
  return wide;
}

std::string MappedFileOf(const std::string& path) {
  const std::size_t separator = path.find("!/");
  return separator == std::string::npos ? path : path.substr(0, separator);
}

}

void SetGuestSegmentObserver(GuestSegmentObserver observer) noexcept {
  g_observer.store(observer, std::memory_order_release);
}

void RegisterGuestSegment(const GuestSegment& segment) noexcept {
  if (segment.end <= segment.start) return;
  if (segment.executable) {
    platform::InstallGuestUnwindBarrier(segment.start, segment.end);
    std::lock_guard<std::mutex> lock(g_writers);
    for (std::size_t i = 0; i < kMaxCodeRanges; ++i) {
      CodeRange& range = g_ranges[i];
      if (range.start.load(std::memory_order_relaxed) != 0) continue;
      range.end.store(segment.end, std::memory_order_release);
      range.start.store(segment.start, std::memory_order_release);
      if (i + 1 > g_high_water.load(std::memory_order_relaxed)) g_high_water.store(i + 1, std::memory_order_release);
      g_count.fetch_add(1, std::memory_order_relaxed);
      break;
    }
    if (segment.path != nullptr && segment.file_bytes != 0) {
      try {
        if (g_code_files == nullptr) g_code_files = new std::vector<CodeFileRange>();
        CodeFileRange range;
        range.start = segment.start;
        range.end = segment.end;
        range.file_offset = segment.file_offset;
        range.file_bytes = segment.file_bytes;
        range.path = segment.path;
        g_code_files->push_back(std::move(range));
      } catch (...) {
      }
    }
  }
  if (GuestSegmentObserver observer = g_observer.load(std::memory_order_acquire)) observer(segment, true);
}

void UnregisterGuestRange(std::uintptr_t start, std::uintptr_t end) noexcept {
  if (end <= start) return;
  platform::RemoveGuestUnwindBarriers(start, end);
  platform::RemoveRawSyscallFunctions(start, end);
  {
    std::lock_guard<std::mutex> lock(g_writers);
    const std::size_t used = g_high_water.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < used; ++i) {
      CodeRange& range = g_ranges[i];
      const std::uintptr_t s = range.start.load(std::memory_order_relaxed);
      if (s == 0) continue;
      const std::uintptr_t e = range.end.load(std::memory_order_relaxed);
      if (s < end && start < e) {
        range.start.store(0, std::memory_order_release);
        range.end.store(0, std::memory_order_release);
        g_count.fetch_sub(1, std::memory_order_relaxed);
      }
    }
    if (g_code_files != nullptr) {
      g_code_files->erase(std::remove_if(g_code_files->begin(), g_code_files->end(),
                                         [start, end](const CodeFileRange& range) {
                                           return range.start < end && start < range.end;
                                         }),
                          g_code_files->end());
    }
  }
  if (GuestSegmentObserver observer = g_observer.load(std::memory_order_acquire)) {
    GuestSegment removed;
    removed.start = start;
    removed.end = end;
    observer(removed, false);
  }
}

void OnGuestImageLoaded(const char* path, std::uintptr_t load_bias, const void* program_headers,
                        std::size_t count) noexcept {
  struct ProgramHeader {
    std::uint32_t type;
    std::uint32_t flags;
    std::uint64_t offset, address, physical, file_size, memory_size, alignment;
  };
  constexpr std::uint32_t kLoad = 1, kUnwindTable = 0x6474e550, kExecutable = 1;
  if (program_headers == nullptr || count == 0) return;
  try {
    platform::RawSyscallImage image;
    const auto* const headers = static_cast<const ProgramHeader*>(program_headers);
    for (std::size_t i = 0; i < count; ++i) {
      const ProgramHeader& header = headers[i];
      const std::uintptr_t start = load_bias + static_cast<std::uintptr_t>(header.address);
      if (header.type == kLoad && header.file_size != 0) {
        platform::RawSyscallImage::Range range;
        range.start = start;
        range.end = start + static_cast<std::uintptr_t>(header.file_size);
        image.loaded.push_back(range);
        if ((header.flags & kExecutable) != 0) image.executable.push_back(range);
      } else if (header.type == kUnwindTable) {
        image.eh_frame_hdr = start;
        image.eh_frame_hdr_size = static_cast<std::size_t>(header.memory_size);
      }
    }
    if (image.executable.empty()) return;
    LARGE_INTEGER frequency{}, before{}, after{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&before);
    platform::RawSyscallScan scan = platform::ScanRawSyscalls(image);
    QueryPerformanceCounter(&after);
    const bool unchecked = !scan.function_table && scan.byte_pairs != 0;
    if (scan.functions.empty() && scan.undecodable == 0 && scan.too_many_stops == 0 && scan.outside_functions == 0 &&
        !unchecked) {
      return;
    }
    std::string sizes;
    for (const platform::RawSyscallFunction& function : scan.functions) {
      if (!sizes.empty()) sizes += ", ";
      sizes += std::to_string(function.syscalls);
    }
    const std::size_t functions = scan.functions.size();
    const char* name = GuestNameOf(path);
    if (name == nullptr) name = "";
    const platform::RawSyscallWatch watch =
        platform::AddRawSyscallFunctions(std::move(scan.functions), load_bias, name);
    const double milliseconds =
        frequency.QuadPart != 0
            ? static_cast<double>(after.QuadPart - before.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart)
            : 0.0;
    std::fprintf(stderr,
                 "[elixir][raw-syscall] image=%s instructions=%zu functions=%zu%s%s%s answered=%zu%s"
                 " undecodable_functions=%zu too_many_stops=%zu outside_functions=%zu function_table=%s"
                 " byte_pairs=%zu scan_ms=%.1f\n",
                 name, scan.syscalls, functions, sizes.empty() ? "" : " (", sizes.c_str(), sizes.empty() ? "" : ")",
                 watch.watched_syscalls,
                 platform::RawSyscallTrapEnabled() ? "" : " (ELIXIR_RAW_SYSCALLS=0: none)", scan.undecodable,
                 scan.too_many_stops, scan.outside_functions, scan.function_table ? "yes" : "no", scan.byte_pairs,
                 milliseconds);
  } catch (...) {
  }
}

bool IsGuestCode(std::uintptr_t address) noexcept {
  const std::size_t used = g_high_water.load(std::memory_order_acquire);
  for (std::size_t i = 0; i < used; ++i) {
    const std::uintptr_t s = g_ranges[i].start.load(std::memory_order_acquire);
    if (s == 0 || address < s) continue;
    if (address < g_ranges[i].end.load(std::memory_order_acquire)) return true;
  }
  return false;
}

std::size_t GuestCodeRangeCount() noexcept { return g_count.load(std::memory_order_relaxed); }

namespace {

std::atomic<GuestNameHook> g_to_guest{nullptr};
std::atomic<HostPathHook> g_to_host{nullptr};
SRWLOCK g_names_lock = SRWLOCK_INIT;
std::map<std::string, std::string, std::less<>>* g_names = nullptr;

}

void SetGuestPathHooks(GuestNameHook to_guest, HostPathHook to_host) noexcept {
  g_to_guest.store(to_guest, std::memory_order_release);
  g_to_host.store(to_host, std::memory_order_release);
}

const char* GuestNameOf(const char* host_path) noexcept {
  if (host_path == nullptr || host_path[0] == '\0') return host_path;
  const GuestNameHook hook = g_to_guest.load(std::memory_order_acquire);
  if (hook == nullptr) return host_path;
  AcquireSRWLockShared(&g_names_lock);
  const char* known = nullptr;
  if (g_names != nullptr) {
    const auto found = g_names->find(std::string_view(host_path));
    if (found != g_names->end()) known = found->second.c_str();
  }
  ReleaseSRWLockShared(&g_names_lock);
  if (known != nullptr) return known;
  try {
    std::string guest_path;
    if (!hook(host_path, &guest_path) || guest_path.empty()) guest_path = host_path;
    AcquireSRWLockExclusive(&g_names_lock);
    const char* stored = host_path;
    try {
      if (g_names == nullptr) g_names = new std::map<std::string, std::string, std::less<>>();
      stored = g_names->emplace(host_path, std::move(guest_path)).first->second.c_str();
    } catch (...) {
    }
    ReleaseSRWLockExclusive(&g_names_lock);
    return stored;
  } catch (...) {
    return host_path;
  }
}

bool HostPathOf(const char* guest_path, std::string* host_path) noexcept {
  if (guest_path == nullptr || guest_path[0] != '/' || host_path == nullptr) return false;
  const HostPathHook hook = g_to_host.load(std::memory_order_acquire);
  if (hook == nullptr) return false;
  try {
    std::string resolved;
    if (!hook(guest_path, &resolved) || resolved.empty()) return false;
    *host_path = std::move(resolved);
    return true;
  } catch (...) {
    return false;
  }
}

std::vector<GuestCodeCheck> CheckGuestCodeAgainstFiles() {
  std::vector<CodeFileRange> ranges;
  {
    std::lock_guard<std::mutex> lock(g_writers);
    if (g_code_files != nullptr) ranges = *g_code_files;
  }
  constexpr std::size_t kChunk = 1u << 20;
  constexpr std::size_t kPage = 4096;
  std::vector<unsigned char> from_file(kChunk);
  std::vector<unsigned char> from_memory(kChunk);
  std::vector<GuestCodeCheck> checks;
  for (const CodeFileRange& range : ranges) {
    std::size_t index = 0;
    while (index < checks.size() && checks[index].path != range.path) ++index;
    if (index == checks.size()) {
      checks.emplace_back();
      checks.back().path = range.path;
    }
    GuestCodeCheck& check = checks[index];
    const HANDLE file = CreateFileW(WidePath(MappedFileOf(range.path)).c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      check.unreadable = true;
      continue;
    }
    const std::uint64_t mapped = std::min<std::uint64_t>(range.file_bytes, range.end - range.start);
    for (std::uint64_t done = 0; done < mapped;) {
      const auto want = static_cast<DWORD>(std::min<std::uint64_t>(kChunk, mapped - done));
      const std::uint64_t file_position = range.file_offset + done;
      OVERLAPPED at{};
      at.Offset = static_cast<DWORD>(file_position & 0xffffffffu);
      at.OffsetHigh = static_cast<DWORD>(file_position >> 32);
      DWORD got = 0;
      if (!ReadFile(file, from_file.data(), want, &got, &at) || got != want) {
        check.unreadable = true;
        break;
      }
      const std::uintptr_t address = range.start + static_cast<std::uintptr_t>(done);
      SIZE_T copied = 0;
      if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), from_memory.data(), want,
                             &copied) ||
          copied != want) {
        check.unreadable = true;
        break;
      }
      if (std::memcmp(from_file.data(), from_memory.data(), want) != 0) {
        for (std::size_t page = 0; page < want; page += kPage) {
          const std::size_t page_bytes = std::min<std::size_t>(kPage, want - page);
          if (std::memcmp(from_file.data() + page, from_memory.data() + page, page_bytes) == 0) continue;
          ++check.differing_pages;
          for (std::size_t i = page; i < page + page_bytes; ++i) {
            if (from_file[i] == from_memory[i]) continue;
            ++check.differing_bytes;
            if (check.first_difference == 0) check.first_difference = address + i;
          }
        }
      }
      check.compared_bytes += want;
      done += want;
    }
    CloseHandle(file);
  }
  return checks;
}

bool GuestCodeMatchesFiles(const std::vector<GuestCodeCheck>& checks) noexcept {
  for (const GuestCodeCheck& check : checks) {
    if (check.unreadable || check.differing_bytes != 0) return false;
  }
  return true;
}

}
