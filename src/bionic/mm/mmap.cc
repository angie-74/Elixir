// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <algorithm>
#include <cstring>

#include "abi/sysv_va_list.h"
#include "abi/variadic_thunk.h"
#include "bionic/files/files.h"
#include "bionic/mm/mm.h"
#include "bionic/mm/mm_internal.h"
#include "bionic/vfs.h"

extern "C" ELIXIR_SYSV int elixir_bionic_fcntl(int fd, int command, std::uint64_t arg) noexcept;
extern "C" ELIXIR_SYSV int elixir_bionic_fstat(int fd, elixir::bionic::guest::Stat* st) noexcept;

namespace elixir::bionic::mm {
namespace {

constexpr std::uint64_t kDevZero = (1u << 8) | 5u;

constexpr std::uint64_t kLegacyMapMask =
    static_cast<std::uint64_t>(guest::kMAP_SHARED | guest::kMAP_PRIVATE | guest::kMAP_FIXED | guest::kMAP_ANONYMOUS |
                               guest::kMAP_DENYWRITE | guest::kMAP_EXECUTABLE | guest::kMAP_UNINITIALIZED |
                               guest::kMAP_GROWSDOWN | guest::kMAP_LOCKED | guest::kMAP_NORESERVE |
                               guest::kMAP_POPULATE | guest::kMAP_NONBLOCK | guest::kMAP_STACK |
                               guest::kMAP_HUGETLB | guest::kMAP_32BIT | guest::kMAP_ABOVE4G |
                               guest::kMAP_FIXED_NOREPLACE) |
    static_cast<std::uint64_t>(guest::kMAP_HUGE_2MB) | static_cast<std::uint64_t>(guest::kMAP_HUGE_1GB);

struct FdInfo {
  HANDLE handle = nullptr;
  int access = 0;
  bool mmapable = false;
  bool dev_zero = false;
  std::uint64_t dev = 0, ino = 0;
};

bool InspectFd(int fd, FdInfo* info) {
  const int saved = GuestErrno();
  const int status = elixir_bionic_fcntl(fd, guest::kF_GETFL, 0);
  if (status < 0 || (status & guest::kO_PATH) != 0) {
    SetGuestErrno(saved);
    return false;
  }
  info->access = status & guest::kO_ACCMODE;
  guest::Stat st{};
  if (elixir_bionic_fstat(fd, &st) == 0) {
    const int type = static_cast<int>(st.st_mode) & guest::kS_IFMT;
    if (type == guest::kS_IFCHR && st.st_rdev == kDevZero) {
      info->dev_zero = true;
    } else if (type == guest::kS_IFREG) {
      vfs::FdKind kind = vfs::FdKind::kFile;
      HANDLE handle = static_cast<HANDLE>(vfs::HandleOf(fd, &kind));
      if (handle != nullptr && handle != INVALID_HANDLE_VALUE && kind == vfs::FdKind::kFile &&
          GetFileType(handle) == FILE_TYPE_DISK) {
        info->handle = handle;
        info->mmapable = true;
        info->dev = st.st_dev;
        info->ino = st.st_ino;
      }
    }
  }
  SetGuestErrno(saved);
  return true;
}

FilePtr NewSharedAnonymous(std::uintptr_t length, bool noreserve, int* guest_errno) {
  auto object = std::make_shared<FileObject>();
  const DWORD protect = PAGE_EXECUTE_READWRITE | (noreserve ? SEC_RESERVE : SEC_COMMIT);
  object->section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, protect, static_cast<DWORD>(length >> 32),
                                       static_cast<DWORD>(length & 0xffffffffu), nullptr);
  if (object->section == nullptr) {
    *guest_errno = guest::kENOMEM;
    return nullptr;
  }
  object->size = length;
  object->writable = true;
  object->executable = true;
  object->reserve_only = noreserve;
  object->path = "/dev/zero (deleted)";
  static std::atomic<std::uint64_t> next_inode{1};
  object->ino = next_inode.fetch_add(1);
  return object;
}

FilePtr NewFileObject(const FdInfo& info, bool shared) {
  auto object = std::make_shared<FileObject>();
  HANDLE duplicate = nullptr;
  if (!DuplicateHandle(GetCurrentProcess(), info.handle, GetCurrentProcess(), &duplicate, 0, FALSE,
                       DUPLICATE_SAME_ACCESS)) {
    return nullptr;
  }
  object->file = duplicate;
  object->copy_on_write = !shared;
  object->writable = shared && info.access == guest::kO_RDWR;
  object->executable = true;
  object->dev = info.dev;
  object->ino = info.ino;
  object->path = MemfdName(info.handle);
  return object;
}

void* FailMap(int guest_errno) {
  SetGuestErrno(guest_errno);
  return MapFailed();
}

void Populate(std::uintptr_t lo, std::uintptr_t hi) noexcept {
  constexpr std::uintptr_t kChunk = 64 * 1024;
  try {
    for (const ForeignRun& run : QueryRuns(lo, hi)) {
      if (run.state != MEM_COMMIT || (run.protect & 0xff) == PAGE_NOACCESS || (run.protect & PAGE_GUARD) != 0) {
        continue;
      }
      for (std::uintptr_t p = run.lo; p < run.hi; p += kChunk) {
        const std::uintptr_t n = std::min(kChunk, run.hi - p);
        if (VirtualLock(reinterpret_cast<void*>(p), n)) {
          VirtualUnlock(reinterpret_cast<void*>(p), n);
          continue;
        }
        for (std::uintptr_t page = p; page < p + n; page += kPageSize) {
          unsigned char byte = 0;
          SIZE_T got = 0;
          ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(page), &byte, 1, &got);
        }
      }
    }
  } catch (...) {
  }
}

void DiscardFresh(Work& work, std::uintptr_t base, std::uintptr_t end) {
  work.Unmap(base, end);
  std::size_t pieces = 0;
  for (auto it = work.regions.lower_bound(base); it != work.regions.end() && it->first < end; ++it) ++pieces;
  if (pieces > 1) CoalescePlaceholders(base, end - base);
  VirtualFree(reinterpret_cast<void*>(base), 0, MEM_RELEASE);
}

bool HasMappedOrForeign(Work& work, std::uintptr_t lo, std::uintptr_t hi) {
  std::uintptr_t cursor = lo;
  while (cursor < hi) {
    auto it = work.Find(cursor);
    if (it != work.regions.end()) {
      if (IsMapped(it->second)) return true;
      cursor = it->second.end;
      continue;
    }
    auto next = work.regions.upper_bound(cursor);
    const std::uintptr_t gap_hi = std::min(hi, next == work.regions.end() ? hi : next->first);
    for (const ForeignRun& run : QueryRuns(cursor, gap_hi)) {
      if (run.state != MEM_FREE) return true;
    }
    cursor = gap_hi;
  }
  return false;
}

struct Request {
  std::uintptr_t len = 0;
  Region attrs;
  bool anonymous_private = false;
  bool commit = true;
  bool populate = false;
  bool low_2g = false, above_4g = false;
};

bool Build(Work& work, std::uintptr_t start, const Request& request) {
  if (request.anonymous_private) {
    return work.MapAnonymous(start, start + request.len, request.attrs, request.commit);
  }
  return work.MapObject(start, start + request.len, request.attrs);
}

void Finalize(std::uintptr_t start, const Request& request) {
  if ((request.attrs.flags & (kFlagLocked | kFlagLockOnFault)) == kFlagLocked) {
    LockPages(start, start + request.len);
  } else if (request.populate) {
    Populate(start, start + request.len);
  }
}

int PlaceAt(std::uintptr_t start, const Request& request, bool noreplace, bool wait_for_races = true) {
  Space& space = Space::Get();
  for (int attempt = 0;; ++attempt) {
    std::unique_ptr<Work> work = space.Begin({{GranuleDown(start), GranuleUp(start + request.len)}});
    if (noreplace && HasMappedOrForeign(*work, start, start + request.len)) {
      const bool racing = wait_for_races && space.pending_fresh.load() > 0;
      space.Abandon(std::move(work));
      if (racing && attempt < 1000) {
        SwitchToThread();
        continue;
      }
      return guest::kEEXIST;
    }
    if (!noreplace && !work->Unmap(start, start + request.len)) {
      const int error = work->error;
      space.Publish(std::move(work));
      return error != 0 ? error : guest::kENOMEM;
    }
    int own_error = 0;
    if (!work->OwnRange(start, start + request.len, &own_error)) {
      const bool racing = wait_for_races && space.pending_fresh.load() > 0 && own_error == guest::kEEXIST;
      space.Publish(std::move(work));
      if (racing && attempt < 1000) {
        SwitchToThread();
        continue;
      }
      return guest::kENOMEM;
    }
    if (!Build(*work, start, request)) {
      const int error = work->error != 0 ? work->error : guest::kENOMEM;
      work->Unmap(start, start + request.len);
      space.Publish(std::move(work));
      return error;
    }
    space.Publish(std::move(work));
    return 0;
  }
}

int PlaceFresh(const Request& request, std::uintptr_t* placed) {
  Space& space = Space::Get();
  const bool grows_down = (request.attrs.flags & kFlagGrowsDown) != 0;
  const std::uintptr_t headroom = grows_down ? kGrowsDownHeadroom : 0;
  if (request.len > kTaskSize - headroom) return guest::kENOMEM;
  const std::uintptr_t size = GranuleUp(request.len + headroom);
  space.pending_fresh.fetch_add(1);
  std::uintptr_t base = 0;
  if (!ReservePlaceholder(0, size, request.low_2g, request.above_4g, &base)) {
    space.pending_fresh.fetch_sub(1);
    return guest::kENOMEM;
  }
  const std::uintptr_t start = grows_down ? base + size - request.len : base;
  Work work(&space);
  Region free_region;
  free_region.end = base + size;
  free_region.arena = base;
  work.regions.emplace(base, free_region);
  if (!Build(work, start, request)) {
    const int error = work.error != 0 ? work.error : guest::kENOMEM;
    DiscardFresh(work, base, base + size);
    space.pending_fresh.fetch_sub(1);
    return error;
  }
  space.InsertFresh(base, base + size, std::move(work.regions));
  space.pending_fresh.fetch_sub(1);
  *placed = start;
  return 0;
}

void* DoMmap(std::uintptr_t addr, std::uint64_t length, int prot, int flags, int fd, std::int64_t offset) {
  if ((static_cast<std::uint64_t>(offset) & (kPageSize - 1)) != 0) return FailMap(guest::kEINVAL);
  const bool anonymous = (flags & guest::kMAP_ANONYMOUS) != 0;
  FdInfo info;
  if (!anonymous) {
    if (!InspectFd(fd, &info)) return FailMap(guest::kEBADF);
    if ((flags & guest::kMAP_HUGETLB) != 0) return FailMap(guest::kEINVAL);
  } else if ((flags & guest::kMAP_HUGETLB) != 0) {
    return FailMap(guest::kEINVAL);
  }
  const auto fail_file_map = [&info, fd](int error) {
    if (info.mmapable) files::RecordContentCacheMapFailure(fd, info.handle, error);
    return FailMap(error);
  };
  if (length == 0) return FailMap(guest::kEINVAL);
  if ((flags & guest::kMAP_FIXED_NOREPLACE) != 0) flags |= guest::kMAP_FIXED;
  const bool fixed = (flags & guest::kMAP_FIXED) != 0;
  if (!fixed) {
    addr = PageDown(addr);
    if (addr != 0 && addr < kMmapMinAddr) addr = PageUp(kMmapMinAddr);
  }
  if (length > ~std::uint64_t{0} - (kPageSize - 1)) return FailMap(guest::kENOMEM);
  const std::uintptr_t len = PageUp(static_cast<std::uintptr_t>(length));
  const std::uint64_t pgoff = static_cast<std::uint64_t>(offset) >> 12;
  if (pgoff + (len >> 12) < pgoff) return FailMap(guest::kEOVERFLOW);
  if (len > kTaskSize) return FailMap(guest::kENOMEM);
  if (fixed) {
    if (addr > kTaskSize - len) return FailMap(guest::kENOMEM);
    if ((addr & (kPageSize - 1)) != 0) return FailMap(guest::kEINVAL);
    if (addr < kMmapMinAddr) return FailMap(guest::kEPERM);
    if ((flags & guest::kMAP_FIXED_NOREPLACE) != 0) {
      Space& space = Space::Get();
      std::unique_ptr<Work> probe = space.Begin({{addr, addr + len}});
      const bool busy = HasMappedOrForeign(*probe, addr, addr + len);
      space.Abandon(std::move(probe));
      if (busy) return FailMap(guest::kEEXIST);
    }
  }
  Space& space = Space::Get();
  const int lock_future = space.lock_future.load();
  const bool locked = (flags & guest::kMAP_LOCKED) != 0 || (lock_future & kMCL_FUTURE) != 0;
  const std::uint64_t memlock = MemlockLimit();
  if ((flags & guest::kMAP_LOCKED) != 0 && memlock == 0) return FailMap(guest::kEPERM);
  if (locked && memlock != ~std::uint64_t{0} && space.locked_bytes.load() + len > memlock) {
    return FailMap(guest::kEAGAIN);
  }

  Request request;
  request.len = len;
  Region& attrs = request.attrs;
  attrs.prot = static_cast<std::uint8_t>(prot & kProtRWX);
  attrs.max_prot = static_cast<std::uint8_t>(kProtRWX);
  const int type = flags & guest::kMAP_TYPE;
  const bool want_shared = type == guest::kMAP_SHARED || type == guest::kMAP_SHARED_VALIDATE;
  if (!anonymous) {
    switch (type) {
      case guest::kMAP_SHARED:
      case guest::kMAP_SHARED_VALIDATE:
        if (type == guest::kMAP_SHARED_VALIDATE &&
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(flags)) & ~kLegacyMapMask) != 0) {
          return FailMap(guest::kEOPNOTSUPP);
        }
        if ((prot & guest::kPROT_WRITE) != 0 && info.access != guest::kO_RDWR && info.access != guest::kO_WRONLY) {
          return fail_file_map(guest::kEACCES);
        }
        [[fallthrough]];
      case guest::kMAP_PRIVATE:
        if (info.access == guest::kO_WRONLY) return fail_file_map(guest::kEACCES);
        if (!info.mmapable && !info.dev_zero) return FailMap(guest::kENODEV);
        if ((flags & guest::kMAP_GROWSDOWN) != 0) return FailMap(guest::kEINVAL);
        break;
      default:
        return FailMap(guest::kEINVAL);
    }
  } else {
    switch (type) {
      case guest::kMAP_SHARED:
        if ((flags & guest::kMAP_GROWSDOWN) != 0) return FailMap(guest::kEINVAL);
        break;
      case guest::kMAP_PRIVATE:
        break;
      default:
        return FailMap(guest::kEINVAL);
    }
  }
  const bool noreserve = (flags & guest::kMAP_NORESERVE) != 0;
  if (noreserve) attrs.flags |= kFlagNoReserve;
  if ((flags & guest::kMAP_GROWSDOWN) != 0) attrs.flags |= kFlagGrowsDown;
  if (locked) {
    attrs.flags |= kFlagLocked;
    if ((flags & guest::kMAP_LOCKED) == 0 && (lock_future & kMCL_ONFAULT) != 0) attrs.flags |= kFlagLockOnFault;
  }
  request.populate = (flags & (guest::kMAP_POPULATE | guest::kMAP_NONBLOCK)) == guest::kMAP_POPULATE;
  request.low_2g = (flags & guest::kMAP_32BIT) != 0;
  request.above_4g = !request.low_2g && (flags & guest::kMAP_ABOVE4G) != 0;

  int object_error = 0;
  if (anonymous || info.dev_zero) {
    if (want_shared) {
      attrs.kind = Kind::kAnonShared;
      attrs.file = NewSharedAnonymous(len, noreserve, &object_error);
      if (attrs.file == nullptr) return FailMap(object_error);
    } else {
      attrs.kind = Kind::kAnonPrivate;
      request.anonymous_private = true;
      request.commit = !LazyCommit();
    }
  } else {
    attrs.kind = want_shared ? Kind::kFileShared : Kind::kFilePrivate;
    attrs.offset = static_cast<std::uint64_t>(offset);
    attrs.file = NewFileObject(info, want_shared);
    if (attrs.file == nullptr) return fail_file_map(guest::kENOMEM);
    if (want_shared && !attrs.file->writable) attrs.max_prot &= ~guest::kPROT_WRITE;
    if (attrs.offset > static_cast<std::uint64_t>(INT64_MAX) - len) return FailMap(guest::kEOVERFLOW);
    if (!Api().page_granular_views && (attrs.offset % kGranularity) != 0) return fail_file_map(guest::kEINVAL);
  }

  std::uintptr_t placed = 0;
  int error = 0;
  if (fixed) {
    error = PlaceAt(addr, request, (flags & guest::kMAP_FIXED_NOREPLACE) != 0);
    placed = addr;
  } else {
    error = -1;
    if (addr != 0) {
      const std::uintptr_t hint = PageDown(addr);
      constexpr std::uintptr_t kMap32BitEnd = 0x80000000u;
      const bool hint_fits_32bit = !request.low_2g || (len <= kMap32BitEnd && hint <= kMap32BitEnd - len);
      if (hint <= kTaskSize - len && hint_fits_32bit && (attrs.flags & kFlagGrowsDown) == 0) {
        error = PlaceAt(hint, request, true, false);
        placed = hint;
      }
    }
    if (error != 0) error = PlaceFresh(request, &placed);
  }
  if (error != 0) return fail_file_map(error);
  Finalize(placed, request);
  if ((attrs.prot & guest::kPROT_EXEC) != 0) FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
  return reinterpret_cast<void*>(placed);
}

int DoMunmapRange(std::uintptr_t start, std::uintptr_t end) {
  Space& space = Space::Get();
  std::unique_ptr<Work> work = space.Begin({{start, end}});
  std::uintptr_t cursor = start;
  while (cursor < end) {
    auto it = work->Find(cursor);
    if (it != work->regions.end()) {
      cursor = it->second.end;
      continue;
    }
    auto next = work->regions.upper_bound(cursor);
    const std::uintptr_t gap_hi = std::min(end, next == work->regions.end() ? end : next->first);
    for (const ForeignRun& run : QueryRuns(cursor, gap_hi)) {
      if (run.state != MEM_FREE) {
        space.Abandon(std::move(work));
        return guest::kEINVAL;
      }
    }
    cursor = gap_hi;
  }
  const bool ok = work->Unmap(start, end, true);
  const int error = work->error;
  space.Publish(std::move(work));
  return ok ? 0 : (error != 0 ? error : guest::kENOMEM);
}

int DoMunmap(std::uintptr_t start, std::uint64_t length) {
  if ((start & (kPageSize - 1)) != 0 || start > kTaskSize || length > kTaskSize - start) return guest::kEINVAL;
  const std::uintptr_t end = start + PageUp(static_cast<std::uintptr_t>(length));
  if (end == start) return guest::kEINVAL;
  return DoMunmapRange(start, end);
}

bool ApplyProtection(Work& work, RegionMap::iterator it, int prot) {
  Region& region = it->second;
  const std::uintptr_t start = it->first;
  const DWORD protection = WinProtection(prot, region.kind == Kind::kFilePrivate && region.backing == Backing::kView);
  DWORD previous = 0;
  switch (region.backing) {
    case Backing::kPlaceholder:
      break;
    case Backing::kPrivate:
      for (const ForeignRun& run : QueryRuns(start, region.end)) {
        void* address = reinterpret_cast<void*>(run.lo);
        const std::uintptr_t size = run.hi - run.lo;
        if (run.state == MEM_COMMIT) {
          if (!VirtualProtect(address, size, protection, &previous)) return false;
        } else if ((prot & kProtRWX) != 0 && (region.flags & kFlagLazy) == 0) {
          if (LazyCommit()) {
            region.flags |= kFlagLazy;
          } else if (VirtualAlloc(address, size, MEM_COMMIT, protection) == nullptr) {
            MEMORYSTATUSEX status{};
            status.dwLength = sizeof(status);
            if (!GlobalMemoryStatusEx(&status) || size > status.ullTotalPageFile) {
              work.error = guest::kENOMEM;
              return false;
            }
            region.flags |= kFlagLazy;
          }
        }
      }
      break;
    case Backing::kView:
      if (region.file != nullptr && region.file->reserve_only) {
        for (const ForeignRun& run : QueryRuns(start, region.end)) {
          if (run.state == MEM_COMMIT &&
              !VirtualProtect(reinterpret_cast<void*>(run.lo), run.hi - run.lo, protection, &previous)) {
            return false;
          }
        }
      } else if (!VirtualProtect(reinterpret_cast<void*>(start), region.end - start, protection, &previous)) {
        return false;
      }
      if (region.kind == Kind::kFilePrivate && (prot & guest::kPROT_WRITE) != 0) region.flags |= kFlagEverWritable;
      break;
  }
  const bool now_accessible = (region.prot & kProtRWX) == 0 && (prot & kProtRWX) != 0;
  region.prot = static_cast<std::uint8_t>(prot);
  if (now_accessible && (region.flags & kFlagLocked) != 0 && (region.flags & kFlagLockOnFault) == 0) {
    LockPages(start, region.end);
  }
  return true;
}

int DoMprotect(std::uintptr_t start, std::uint64_t length, int prot) {
  const int grows = prot & (guest::kPROT_GROWSDOWN | guest::kPROT_GROWSUP);
  prot &= ~(guest::kPROT_GROWSDOWN | guest::kPROT_GROWSUP);
  if (grows == (guest::kPROT_GROWSDOWN | guest::kPROT_GROWSUP)) return guest::kEINVAL;
  if ((start & (kPageSize - 1)) != 0) return guest::kEINVAL;
  if (length == 0) return 0;
  const std::uintptr_t len = PageUp(static_cast<std::uintptr_t>(length));
  std::uintptr_t end = start + len;
  if (end <= start) return guest::kENOMEM;
  if ((prot & ~(kProtRWX | guest::kPROT_SEM)) != 0) return guest::kEINVAL;
  prot &= kProtRWX;
  Space& space = Space::Get();
  if (grows != 0) {
    std::vector<Vma> vmas = space.Vmas(start, end);
    if (vmas.empty()) {
      if (space.OwnsAny(start, end) || QueryRuns(start, end).front().state != MEM_COMMIT) return guest::kENOMEM;
      return guest::kEINVAL;
    }
    if (vmas.front().start >= end) return guest::kENOMEM;
    if ((grows & guest::kPROT_GROWSUP) != 0) return guest::kEINVAL;
    if ((vmas.front().attrs.flags & kFlagGrowsDown) == 0) return guest::kEINVAL;
    start = vmas.front().start;
  }
  std::unique_ptr<Work> work = space.Begin({{start, end}});
  int error = 0;
  bool exec_changed = false;
  std::uintptr_t cursor = start;
  while (cursor < end && error == 0) {
    auto it = work->Find(cursor);
    if (it == work->regions.end()) {
      auto next = work->regions.upper_bound(cursor);
      const std::uintptr_t gap_hi = std::min(end, next == work->regions.end() ? end : next->first);
      for (const ForeignRun& run : QueryRuns(cursor, gap_hi)) {
        if (run.state == MEM_FREE) {
          error = guest::kENOMEM;
          break;
        }
        void* address = reinterpret_cast<void*>(run.lo);
        const std::uintptr_t size = run.hi - run.lo;
        DWORD previous = 0;
        if (run.state == MEM_COMMIT) {
          const DWORD base = run.protect & 0xff;
          const bool copy_on_write = run.type == MEM_IMAGE || base == PAGE_WRITECOPY || base == PAGE_EXECUTE_WRITECOPY;
          if (!VirtualProtect(address, size, WinProtection(prot, copy_on_write), &previous) &&
              !VirtualProtect(address, size, WinProtection(prot, !copy_on_write), &previous)) {
            error = guest::kEACCES;
            break;
          }
        } else if ((prot & kProtRWX) != 0) {
          if (VirtualAlloc(address, size, MEM_COMMIT, WinProtection(prot, false)) == nullptr) {
            error = guest::kENOMEM;
            break;
          }
        }
        exec_changed = exec_changed || (prot & guest::kPROT_EXEC) != 0;
      }
      cursor = gap_hi;
      continue;
    }
    if (!IsMapped(it->second)) {
      error = guest::kENOMEM;
      break;
    }
    if ((prot & ~it->second.max_prot & kProtRWX) != 0) {
      error = guest::kEACCES;
      break;
    }
    const std::uintptr_t split_lo = std::max(cursor, it->first);
    const std::uintptr_t split_hi = std::min(end, it->second.end);
    if (!work->SplitAt(split_lo) || !work->SplitAt(split_hi)) {
      error = guest::kENOMEM;
      break;
    }
    it = work->Find(cursor);
    const std::uintptr_t region_end = it->second.end;
    if ((it->second.prot ^ prot) & guest::kPROT_EXEC) exec_changed = true;
    if (!ApplyProtection(*work, it, prot)) {
      error = work->error != 0 ? work->error : guest::kENOMEM;
      break;
    }
    cursor = region_end;
  }
  space.Publish(std::move(work));
  if (exec_changed) FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(start), end - start);
  return error;
}

bool VmaAt(Work& work, std::uintptr_t address, std::uintptr_t* vma_start, std::uintptr_t* vma_end, Region* attrs) {
  auto it = work.Find(address);
  if (it == work.regions.end() || !IsMapped(it->second)) return false;
  *attrs = it->second;
  std::uintptr_t start = it->first;
  for (auto cur = it; cur != work.regions.begin();) {
    auto previous = std::prev(cur);
    if (previous->second.end != cur->first || !Mergeable(previous->first, previous->second, cur->second)) break;
    start = previous->first;
    cur = previous;
  }
  std::uintptr_t end = it->second.end;
  for (auto cur = it;;) {
    auto next = std::next(cur);
    if (next == work.regions.end() || next->first != cur->second.end || !Mergeable(cur->first, cur->second, next->second)) {
      break;
    }
    end = next->second.end;
    cur = next;
  }
  *vma_start = start;
  *vma_end = end;
  return true;
}

bool MovePages(Work& work, std::uintptr_t old, std::uintptr_t length, std::uintptr_t dst) {
  if (!work.SplitAt(old) || !work.SplitAt(old + length)) return false;
  std::vector<std::pair<std::uintptr_t, Region>> sources;
  for (auto it = work.regions.lower_bound(old); it != work.regions.end() && it->first < old + length; ++it) {
    sources.emplace_back(it->first, it->second);
  }
  for (const auto& source : sources) {
    const std::uintptr_t start = source.first;
    const Region& region = source.second;
    const std::uintptr_t size = region.end - start;
    const std::uintptr_t target = dst + (start - old);
    Region attrs = region;
    switch (region.backing) {
      case Backing::kPrivate: {
        if (!work.MakePrivate(target, target + size, attrs, false)) return false;
        if (!work.CopyContent(start, target, size)) return false;
        auto made = work.Find(target);
        made->second.flags = (made->second.flags & ~kFlagLazy) | (region.flags & kFlagLazy);
        for (const ForeignRun& run : QueryRuns(target, target + size)) {
          DWORD previous = 0;
          if (run.state == MEM_COMMIT) {
            VirtualProtect(reinterpret_cast<void*>(run.lo), run.hi - run.lo, WinProtection(region.prot, false),
                           &previous);
          }
        }
        break;
      }
      case Backing::kView: {
        std::vector<Range> dirty;
        if (region.kind == Kind::kFilePrivate && (region.flags & kFlagEverWritable) != 0) {
          dirty = work.DirtyRuns(start, region.end);
        }
        std::uintptr_t cursor = start;
        auto map_clean = [&](std::uintptr_t a, std::uintptr_t b) {
          Region clean = attrs;
          clean.offset = region.offset + (a - start);
          return work.MapObject(dst + (a - old), dst + (b - old), clean);
        };
        for (const Range& d : dirty) {
          if (d.lo > cursor && !map_clean(cursor, d.lo)) return false;
          Region copied = attrs;
          copied.offset = region.offset + (d.lo - start);
          copied.flags &= ~kFlagEverWritable;
          const std::uintptr_t t = dst + (d.lo - old);
          if (!work.MakePrivate(t, t + (d.hi - d.lo), copied, false) || !work.CopyContent(d.lo, t, d.hi - d.lo)) {
            return false;
          }
          work.Find(t)->second.flags &= ~kFlagLazy;
          DWORD previous = 0;
          VirtualProtect(reinterpret_cast<void*>(t), d.hi - d.lo, WinProtection(region.prot, false), &previous);
          cursor = d.hi;
        }
        if (cursor < region.end && !map_clean(cursor, region.end)) return false;
        break;
      }
      case Backing::kPlaceholder:
        if (!work.MapObject(target, target + size, attrs)) return false;
        break;
    }
    if ((region.flags & (kFlagLocked | kFlagLockOnFault)) == kFlagLocked) LockPages(target, target + size);
  }
  return true;
}

bool ExtendMapping(Work& work, std::uintptr_t from, std::uintptr_t to, const Region& last, std::uintptr_t last_start) {
  Region attrs = last;
  attrs.offset = last.offset + (from - last_start);
  attrs.flags &= ~(kFlagLazy | kFlagEverWritable);
  bool ok;
  if (attrs.kind == Kind::kAnonPrivate) {
    ok = work.MapAnonymous(from, to, attrs, !LazyCommit());
  } else {
    ok = work.MapObject(from, to, attrs);
  }
  if (ok && (attrs.flags & (kFlagLocked | kFlagLockOnFault)) == kFlagLocked) LockPages(from, to);
  return ok;
}

bool MunmapRangeValid(std::uintptr_t start, std::uintptr_t len) {
  return start <= kTaskSize && len <= kTaskSize - start;
}

bool RangeIsFree(Work& work, std::uintptr_t lo, std::uintptr_t hi) {
  return !HasMappedOrForeign(work, lo, hi);
}

void* DoMremap(std::uintptr_t old, std::uint64_t old_size, std::uint64_t new_size, int flags, std::uintptr_t new_addr) {
  constexpr int kKnown = kMREMAP_MAYMOVE | kMREMAP_FIXED | kMREMAP_DONTUNMAP;
  if ((flags & ~kKnown) != 0) return FailMap(guest::kEINVAL);
  if ((flags & kMREMAP_FIXED) != 0 && (flags & kMREMAP_MAYMOVE) == 0) return FailMap(guest::kEINVAL);
  if ((flags & kMREMAP_DONTUNMAP) != 0 && ((flags & kMREMAP_MAYMOVE) == 0 || old_size != new_size)) {
    return FailMap(guest::kEINVAL);
  }
  if ((old & (kPageSize - 1)) != 0) return FailMap(guest::kEINVAL);
  std::uintptr_t old_len = PageUp(static_cast<std::uintptr_t>(old_size));
  const std::uintptr_t new_len = PageUp(static_cast<std::uintptr_t>(new_size));
  if (new_len == 0) return FailMap(guest::kEINVAL);
  const bool moving = (flags & (kMREMAP_FIXED | kMREMAP_DONTUNMAP)) != 0;
  int moving_error = 0;
  if (moving) {
    if ((new_addr & (kPageSize - 1)) != 0) {
      moving_error = guest::kEINVAL;
    } else if (new_len > kTaskSize || new_addr > kTaskSize - new_len) {
      moving_error = guest::kEINVAL;
    } else if (old + old_len > new_addr && new_addr + new_len > old) {
      moving_error = guest::kEINVAL;
    }
  }
  if (old >= kTaskSize) return FailMap(guest::kEFAULT);
  Space& space = Space::Get();
  const std::uintptr_t room = kTaskSize - old;
  std::uintptr_t span = kPageSize;
  if (old_len <= room) span = std::max(span, old_len);
  if (new_len <= room) span = std::max(span, new_len);
  std::vector<Range> claims = {{GranuleDown(old), GranuleUp(old + span)}};
  if (moving && moving_error == 0 && (flags & kMREMAP_FIXED) != 0) {
    claims.push_back({GranuleDown(new_addr), GranuleUp(new_addr + new_len)});
  }
  std::unique_ptr<Work> work = space.Begin(claims);
  auto finish = [&](int error, std::uintptr_t result) -> void* {
    space.Publish(std::move(work));
    if (error != 0) return FailMap(error);
    return reinterpret_cast<void*>(result);
  };
  std::uintptr_t vma_start = 0, vma_end = 0;
  Region vma;
  if (!VmaAt(*work, old, &vma_start, &vma_end, &vma)) return finish(guest::kEFAULT, 0);
  if (moving_error != 0) return finish(moving_error, 0);

  auto vma_to_resize = [&](std::uintptr_t length, std::uintptr_t grown) -> int {
    if (length == 0 && !IsShared(vma.kind)) return guest::kEINVAL;
    if (length > vma_end - old) return guest::kEFAULT;
    if (grown == length) return 0;
    std::uint64_t pgoff = old >> 12;
    if (vma.kind != Kind::kAnonPrivate) {
      const auto at = work->Find(old);
      pgoff = (at->second.offset + (old - at->first)) >> 12;
    }
    if (pgoff + (grown >> 12) < pgoff) return guest::kEINVAL;
    if ((vma.flags & kFlagLocked) != 0) {
      const std::uint64_t limit = MemlockLimit();
      if (limit != ~std::uint64_t{0} && space.locked_bytes.load() + (grown - length) > limit) return guest::kEAGAIN;
    }
    return 0;
  };

  auto move_to = [&](std::uintptr_t length, std::uintptr_t dst) -> int {
    if (length > 0) {
      if (!MovePages(*work, old, length, dst)) return work->error != 0 ? work->error : guest::kENOMEM;
    }
    if (new_len > length) {
      auto last = work->Find(old + (length > 0 ? length - kPageSize : 0));
      Region tail = last->second;
      const std::uintptr_t last_start = last->first;
      Region attrs = tail;
      attrs.offset = tail.offset + ((old + length) - last_start);
      attrs.flags &= ~(kFlagLazy | kFlagEverWritable);
      bool ok = attrs.kind == Kind::kAnonPrivate ? work->MapAnonymous(dst + length, dst + new_len, attrs, !LazyCommit())
                                                 : work->MapObject(dst + length, dst + new_len, attrs);
      if (!ok) return work->error != 0 ? work->error : guest::kENOMEM;
      if ((attrs.flags & (kFlagLocked | kFlagLockOnFault)) == kFlagLocked) LockPages(dst + length, dst + new_len);
    }
    if (length == 0) return 0;
    if ((flags & kMREMAP_DONTUNMAP) != 0) {
      if (!IsShared(vma.kind)) {
        std::vector<std::pair<std::uintptr_t, Region>> olds;
        for (auto it = work->regions.lower_bound(old); it != work->regions.end() && it->first < old + length; ++it) {
          olds.emplace_back(it->first, it->second);
        }
        if (!work->Unmap(old, old + length)) return work->error;
        for (auto& [start, region] : olds) {
          Region attrs = region;
          attrs.flags &= ~(kFlagLocked | kFlagLockOnFault | kFlagLazy | kFlagEverWritable);
          const bool ok = attrs.kind == Kind::kAnonPrivate ? work->MapAnonymous(start, region.end, attrs, !LazyCommit())
                                                           : work->MapObject(start, region.end, attrs);
          if (!ok) return work->error;
        }
      } else {
        for (auto it = work->regions.lower_bound(old); it != work->regions.end() && it->first < old + length; ++it) {
          if ((it->second.flags & kFlagLocked) != 0) UnlockPages(it->first, it->second.end);
          it->second.flags &= ~(kFlagLocked | kFlagLockOnFault);
        }
      }
      return 0;
    }
    return work->Unmap(old, old + length) ? 0 : work->error;
  };

  auto fresh_destination = [&](std::uintptr_t* dst) -> int {
    std::uintptr_t base = 0;
    if (new_len > kTaskSize) return guest::kENOMEM;
    const std::uintptr_t size = GranuleUp(new_len);
    if (!ReservePlaceholder(0, size, false, false, &base)) return guest::kENOMEM;
    work->new_arenas.push_back({base, base + size});
    Region free_region;
    free_region.end = base + size;
    free_region.arena = base;
    work->regions.emplace(base, free_region);
    *dst = base;
    return 0;
  };

  if (moving) {
    if ((flags & kMREMAP_FIXED) != 0) {
      if (!work->Unmap(new_addr, new_addr + new_len)) return finish(work->error, 0);
    }
    if (old_len > new_len) {
      if (!MunmapRangeValid(old + new_len, old_len - new_len)) return finish(guest::kEINVAL, 0);
      if (!work->Unmap(old + new_len, old + old_len)) return finish(work->error, 0);
      old_len = new_len;
    }
    if (const int error = vma_to_resize(old_len, new_len); error != 0) return finish(error, 0);
    std::uintptr_t dst = new_addr;
    if ((flags & kMREMAP_FIXED) != 0) {
      int err = 0;
      if (!work->OwnRange(new_addr, new_addr + new_len, &err)) return finish(guest::kENOMEM, 0);
    } else if (const int error = fresh_destination(&dst); error != 0) {
      return finish(error, 0);
    }
    return finish(move_to(old_len, dst), dst);
  }

  if (old_len >= new_len) {
    if (old_len == new_len) return finish(0, old);
    if (!MunmapRangeValid(old + new_len, old_len - new_len)) return finish(guest::kEINVAL, 0);
    if (!work->Unmap(old + new_len, old + old_len)) return finish(work->error, 0);
    return finish(0, old);
  }
  if (const int error = vma_to_resize(old_len, new_len); error != 0) return finish(error, 0);
  if (old_len == vma_end - old && old_len > 0 && new_len - old_len <= kTaskSize - vma_end &&
      RangeIsFree(*work, vma_end, old + new_len)) {
    int err = 0;
    if (work->OwnRange(vma_end, old + new_len, &err)) {
      auto last = work->Find(vma_end - kPageSize);
      const Region tail = last->second;
      if (!ExtendMapping(*work, vma_end, old + new_len, tail, last->first)) return finish(work->error, 0);
      return finish(0, old);
    }
  }
  if ((flags & kMREMAP_MAYMOVE) == 0) return finish(guest::kENOMEM, 0);
  std::uintptr_t dst = 0;
  if (const int error = fresh_destination(&dst); error != 0) return finish(error, 0);
  return finish(move_to(old_len, dst), dst);
}

}

}

namespace mm = elixir::bionic::mm;
namespace guest = elixir::bionic::guest;
using elixir::bionic::FailWith;

ELIXIR_BIONIC_FN(void*, mmap)(void* address, elixir::abi::g_size length, int prot, int flags, int fd,
                              elixir::abi::g_off offset) noexcept {
  mm::MmTimer timer(mm::MmOp::kMmap);
  return mm::Guarded(mm::MapFailed(), [&] {
    return mm::DoMmap(reinterpret_cast<std::uintptr_t>(address), length, prot, flags, fd, offset);
  });
}

ELIXIR_BIONIC_FN(int, munmap)(void* address, elixir::abi::g_size length) noexcept {
  mm::MmTimer timer(mm::MmOp::kMunmap);
  return mm::Guarded(-1, [&] {
    const int error = mm::DoMunmap(reinterpret_cast<std::uintptr_t>(address), length);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}

ELIXIR_BIONIC_FN(int, mprotect)(void* address, elixir::abi::g_size length, int prot) noexcept {
  mm::MmTimer timer(mm::MmOp::kMprotect);
  return mm::Guarded(-1, [&] {
    const int error = mm::DoMprotect(reinterpret_cast<std::uintptr_t>(address), length, prot);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}

extern "C" ELIXIR_SYSV ELIXIR_ASM_CALLED void* elixir_bionic_mremap_v(void* old_address, elixir::abi::g_size old_size,
                                                    elixir::abi::g_size new_size, int flags,
                                                    elixir::abi::SysvVaList* rest) noexcept {
  return mm::Guarded(mm::MapFailed(), [&] {
    std::uintptr_t new_address = 0;
    if ((flags & (mm::kMREMAP_FIXED | mm::kMREMAP_DONTUNMAP)) != 0) {
      elixir::abi::SysvVaCursor cursor(rest);
      new_address = static_cast<std::uintptr_t>(cursor.NextGp());
    }
    return mm::DoMremap(reinterpret_cast<std::uintptr_t>(old_address), old_size, new_size, flags, new_address);
  });
}
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_mremap, elixir_bionic_mremap_v, 4)

namespace elixir::bionic::mm {

void RegisterMappingExports(ExportTable& table) {
  table.Add(kLibc, "mmap", reinterpret_cast<void*>(&elixir_bionic_mmap));
  table.Add(kLibc, "mmap64", reinterpret_cast<void*>(&elixir_bionic_mmap));
  table.Add(kLibc, "munmap", reinterpret_cast<void*>(&elixir_bionic_munmap));
  table.Add(kLibc, "mprotect", reinterpret_cast<void*>(&elixir_bionic_mprotect));
  table.Add(kLibc, "mremap", reinterpret_cast<void*>(&elixir_bionic_mremap));
}

}
