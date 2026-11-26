// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <psapi.h>

#include <cstring>
#include <mutex>

#include "bionic/mm/mm_internal.h"

extern "C" ELIXIR_SYSV int elixir_bionic_getrlimit(int resource, elixir::bionic::guest::Rlimit* limit) noexcept;

namespace elixir::bionic::mm {
namespace {

bool ProbePageGranularViews(const Win32Api& api) noexcept {
  HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 2 * kPageSize, nullptr);
  if (section == nullptr) return false;
  bool ok = false;
  if (void* whole = MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, 2 * kPageSize)) {
    static_cast<unsigned char*>(whole)[kPageSize] = 0x5A;
    UnmapViewOfFile(whole);
    auto* base = static_cast<unsigned char*>(api.virtual_alloc2(GetCurrentProcess(), nullptr, 2 * kPageSize,
                                                                MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
                                                                PAGE_NOACCESS, nullptr, 0));
    if (base != nullptr) {
      if (VirtualFree(base + kPageSize, kPageSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) {
        void* view = api.map_view3(section, GetCurrentProcess(), base + kPageSize, kPageSize, kPageSize,
                                   MEM_REPLACE_PLACEHOLDER, PAGE_READONLY, nullptr, 0);
        if (view == base + kPageSize) {
          ok = static_cast<const unsigned char*>(view)[0] == 0x5A;
          api.unmap_view2(GetCurrentProcess(), view, MEM_PRESERVE_PLACEHOLDER);
        } else if (view != nullptr) {
          api.unmap_view2(GetCurrentProcess(), view, 0);
        }
        VirtualFree(base + kPageSize, 0, MEM_RELEASE);
      }
      VirtualFree(base, 0, MEM_RELEASE);
    }
  }
  CloseHandle(section);
  return ok;
}

}

const Win32Api& Api() noexcept {
  static const Win32Api api = [] {
    Win32Api resolved;
    HMODULE kernelbase = GetModuleHandleW(L"kernelbase.dll");
    if (kernelbase == nullptr) return resolved;
    resolved.virtual_alloc2 = reinterpret_cast<VirtualAlloc2Fn>(GetProcAddress(kernelbase, "VirtualAlloc2"));
    resolved.map_view3 = reinterpret_cast<MapViewOfFile3Fn>(GetProcAddress(kernelbase, "MapViewOfFile3"));
    resolved.unmap_view2 = reinterpret_cast<UnmapViewOfFile2Fn>(GetProcAddress(kernelbase, "UnmapViewOfFile2"));
    resolved.query_working_set_ex =
        reinterpret_cast<QueryWorkingSetExFn>(GetProcAddress(kernelbase, "QueryWorkingSetEx"));
    resolved.prefetch = reinterpret_cast<PrefetchVirtualMemoryFn>(GetProcAddress(kernelbase, "PrefetchVirtualMemory"));
    resolved.placeholders =
        resolved.virtual_alloc2 != nullptr && resolved.map_view3 != nullptr && resolved.unmap_view2 != nullptr;
    if (resolved.placeholders) resolved.page_granular_views = ProbePageGranularViews(resolved);
    return resolved;
  }();
  return api;
}

bool ReservePlaceholder(std::uintptr_t address, std::uintptr_t size, bool low_2g, bool above_4g,
                        std::uintptr_t* placed) noexcept {
  const Win32Api& api = Api();
  if (!api.placeholders) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return false;
  }
  MEM_ADDRESS_REQUIREMENTS requirements{};
  MEM_EXTENDED_PARAMETER parameter{};
  ULONG parameters = 0;
  if (address == 0 && (low_2g || above_4g)) {
    if (low_2g) {
      requirements.LowestStartingAddress = reinterpret_cast<PVOID>(std::uintptr_t{0x40000000});
      requirements.HighestEndingAddress = reinterpret_cast<PVOID>(std::uintptr_t{0x7fffffff});
    } else {
      requirements.LowestStartingAddress = reinterpret_cast<PVOID>(std::uintptr_t{0x100000000});
    }
    parameter.Type = MemExtendedParameterAddressRequirements;
    parameter.Pointer = &requirements;
    parameters = 1;
  }
  void* result = api.virtual_alloc2(GetCurrentProcess(), reinterpret_cast<void*>(address), size,
                                    MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
                                    parameters != 0 ? &parameter : nullptr, parameters);
  if (result == nullptr) return false;
  if (address != 0 && reinterpret_cast<std::uintptr_t>(result) != address) {
    VirtualFree(result, 0, MEM_RELEASE);
    SetLastError(ERROR_INVALID_ADDRESS);
    return false;
  }
  *placed = reinterpret_cast<std::uintptr_t>(result);
  return true;
}

bool SplitPlaceholder(std::uintptr_t start, std::uintptr_t size) noexcept {
  return VirtualFree(reinterpret_cast<void*>(start), size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) != FALSE;
}

bool CoalescePlaceholders(std::uintptr_t start, std::uintptr_t size) noexcept {
  return VirtualFree(reinterpret_cast<void*>(start), size, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS) != FALSE;
}

bool ReplaceWithPrivate(std::uintptr_t start, std::uintptr_t size, bool commit, DWORD protection) noexcept {
  const ULONG type = MEM_RESERVE | MEM_REPLACE_PLACEHOLDER | (commit ? MEM_COMMIT : 0);
  void* result = Api().virtual_alloc2(GetCurrentProcess(), reinterpret_cast<void*>(start), size, type,
                                      commit ? protection : PAGE_NOACCESS, nullptr, 0);
  return result == reinterpret_cast<void*>(start);
}

bool ReleasePrivateToPlaceholder(std::uintptr_t start, std::uintptr_t size) noexcept {
  return VirtualFree(reinterpret_cast<void*>(start), size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) != FALSE;
}

bool MapViewIntoPlaceholder(HANDLE section, std::uintptr_t start, std::uint64_t offset, std::uintptr_t bytes,
                            DWORD protection) noexcept {
  void* result = Api().map_view3(section, GetCurrentProcess(), reinterpret_cast<void*>(start), offset, bytes,
                                 MEM_REPLACE_PLACEHOLDER, protection, nullptr, 0);
  if (result == nullptr) return false;
  if (result != reinterpret_cast<void*>(start)) {
    Api().unmap_view2(GetCurrentProcess(), result, 0);
    SetLastError(ERROR_INVALID_ADDRESS);
    return false;
  }
  return true;
}

bool UnmapViewToPlaceholder(std::uintptr_t view) noexcept {
  return Api().unmap_view2(GetCurrentProcess(), reinterpret_cast<void*>(view), MEM_PRESERVE_PLACEHOLDER) != FALSE;
}

DWORD WinProtection(int prot, bool copy_on_write) noexcept {
  const bool any = (prot & kProtRWX) != 0;
  const bool write = (prot & guest::kPROT_WRITE) != 0;
  const bool exec = (prot & guest::kPROT_EXEC) != 0;
  if (!any) return PAGE_NOACCESS;
  if (exec) return write ? (copy_on_write ? PAGE_EXECUTE_WRITECOPY : PAGE_EXECUTE_READWRITE) : PAGE_EXECUTE_READ;
  return write ? (copy_on_write ? PAGE_WRITECOPY : PAGE_READWRITE) : PAGE_READONLY;
}

bool ProtPermits(int prot, std::uint64_t access) noexcept {
  switch (access) {
    case 0: return (prot & kProtRWX) != 0;
    case 1: return (prot & guest::kPROT_WRITE) != 0;
    case 8: return (prot & guest::kPROT_EXEC) != 0;
    default: return false;
  }
}

bool Mergeable(std::uintptr_t a_start, const Region& a, const Region& b) noexcept {
  if (!IsMapped(a) || a.kind != b.kind || a.prot != b.prot || a.max_prot != b.max_prot) return false;
  if ((a.flags & kVmaFlagMask) != (b.flags & kVmaFlagMask)) return false;
  if (a.file != b.file) return false;
  if (a.kind != Kind::kAnonPrivate) return a.offset + (a.end - a_start) == b.offset;
  return true;
}

int ErrnoForAllocation(DWORD error) noexcept {
  switch (error) {
    case ERROR_ACCESS_DENIED:
      return guest::kEACCES;
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_ADDRESS:
    case ERROR_COMMITMENT_LIMIT:
    case ERROR_COMMITMENT_MINIMUM:
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
    default:
      return guest::kENOMEM;
  }
}

std::vector<ForeignRun> QueryRuns(std::uintptr_t lo, std::uintptr_t hi) {
  std::vector<ForeignRun> runs;
  std::uintptr_t cursor = lo;
  while (cursor < hi) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<void*>(cursor), &mbi, sizeof(mbi)) != sizeof(mbi)) {
      runs.push_back({cursor, hi, MEM_FREE, 0, 0});
      break;
    }
    const std::uintptr_t end = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    const std::uintptr_t stop = end < hi ? end : hi;
    runs.push_back({cursor, stop, mbi.State, mbi.Protect, mbi.Type});
    if (stop <= cursor) break;
    cursor = stop;
  }
  return runs;
}

void ResidentPages(std::uintptr_t lo, std::uintptr_t hi, unsigned char* out) noexcept {
  const auto query = Api().query_working_set_ex;
  constexpr std::size_t kBatch = 512;
  PSAPI_WORKING_SET_EX_INFORMATION info[kBatch];
  std::size_t index = 0;
  for (std::uintptr_t page = lo; page < hi;) {
    std::size_t count = 0;
    for (; count < kBatch && page + count * kPageSize < hi; ++count) {
      info[count].VirtualAddress = reinterpret_cast<void*>(page + count * kPageSize);
      info[count].VirtualAttributes.Flags = 0;
    }
    const bool ok = query != nullptr &&
                    query(GetCurrentProcess(), info, static_cast<DWORD>(count * sizeof(info[0]))) != FALSE;
    for (std::size_t i = 0; i < count; ++i) out[index++] = ok && info[i].VirtualAttributes.Valid ? 1 : 0;
    page += count * kPageSize;
  }
}

std::uint64_t MemlockLimit() noexcept {
  guest::Rlimit limit{};
  if (elixir_bionic_getrlimit(guest::kRLIMIT_MEMLOCK, &limit) != 0) return 65536;
  return limit.rlim_cur;
}

std::uint64_t StackLimit() noexcept {
  guest::Rlimit limit{};
  if (elixir_bionic_getrlimit(guest::kRLIMIT_STACK, &limit) != 0) return 8 * 1024 * 1024;
  return limit.rlim_cur;
}

namespace {
std::mutex g_working_set_lock;

bool GrowWorkingSet(std::uintptr_t bytes) noexcept {
  std::lock_guard<std::mutex> guard(g_working_set_lock);
  SIZE_T minimum = 0, maximum = 0;
  DWORD flags = 0;
  if (!GetProcessWorkingSetSizeEx(GetCurrentProcess(), &minimum, &maximum, &flags)) return false;
  const SIZE_T wanted_min = minimum + bytes + 16 * kPageSize;
  const SIZE_T wanted_max = maximum > wanted_min + (1u << 20) ? maximum : wanted_min + (1u << 20);
  return SetProcessWorkingSetSizeEx(GetCurrentProcess(), wanted_min, wanted_max, flags) != FALSE;
}
}

bool LockPages(std::uintptr_t lo, std::uintptr_t hi) noexcept {
  try {
    for (const ForeignRun& run : QueryRuns(lo, hi)) {
      if (run.state != MEM_COMMIT) continue;
      if ((run.protect & 0xff) == PAGE_NOACCESS || (run.protect & PAGE_GUARD) != 0) continue;
      void* address = reinterpret_cast<void*>(run.lo);
      const SIZE_T size = run.hi - run.lo;
      if (VirtualLock(address, size)) continue;
      if (GetLastError() != ERROR_WORKING_SET_QUOTA || !GrowWorkingSet(size) || !VirtualLock(address, size)) {
        return false;
      }
    }
    return true;
  } catch (...) {
    return false;
  }
}

void UnlockPages(std::uintptr_t lo, std::uintptr_t hi) noexcept {
  try {
    for (const ForeignRun& run : QueryRuns(lo, hi)) {
      if (run.state != MEM_COMMIT) continue;
      VirtualUnlock(reinterpret_cast<void*>(run.lo), run.hi - run.lo);
    }
  } catch (...) {
  }
}

FileObject::~FileObject() {
  if (file != nullptr) CloseHandle(file);
  if (section != nullptr) CloseHandle(section);
}

}
