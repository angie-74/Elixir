// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <set>

#include "bionic/mm/mm.h"
#include "bionic/mm/mm_internal.h"

namespace elixir::bionic::mm {

thread_local int t_lock_depth = 0;

namespace {

std::uint64_t LockedSize(std::uintptr_t start, const Region& region) {
  return IsMapped(region) && (region.flags & kFlagLocked) != 0 ? region.end - start : 0;
}

Region FreeRegion(std::uintptr_t end, std::uintptr_t arena) {
  Region r;
  r.end = end;
  r.arena = arena;
  r.kind = Kind::kFree;
  r.backing = Backing::kPlaceholder;
  return r;
}

thread_local std::uintptr_t t_retry_address = 0;
thread_local int t_retry_count = 0;

LONG CALLBACK MmVectoredHandler(EXCEPTION_POINTERS* pointers) {
  EXCEPTION_RECORD* record = pointers->ExceptionRecord;
  if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  return Space::Get().OnAccessViolation(record);
}

}

void InstallFaultHandler() noexcept {
  static std::once_flag once;
  std::call_once(once, [] { AddVectoredExceptionHandler(1, MmVectoredHandler); });
}

Space& Space::Get() {
  static Space* space = new Space();
  return *space;
}

Space::Space() {
  Api();
  InstallFaultHandler();
}

void Space::Track(std::uintptr_t lo, std::uintptr_t hi) noexcept {
  std::uintptr_t current = low_.load(std::memory_order_relaxed);
  while (lo < current && !low_.compare_exchange_weak(current, lo)) {
  }
  current = high_.load(std::memory_order_relaxed);
  while (hi > current && !high_.compare_exchange_weak(current, hi)) {
  }
}

Range Space::Expand(Range range) const {
  bool changed = true;
  while (changed) {
    changed = false;
    auto it = regions.upper_bound(range.lo);
    if (it != regions.begin()) {
      auto previous = std::prev(it);
      if (previous->second.end > range.lo) it = previous;
    }
    for (; it != regions.end() && it->first < range.hi; ++it) {
      std::uintptr_t start = it->first;
      std::uintptr_t end = it->second.end;
      if (it->second.backing == Backing::kView) {
        const std::uintptr_t view = it->second.view;
        start = view;
        auto right = it;
        for (auto next = std::next(right); next != regions.end() && next->first == right->second.end &&
                                           next->second.backing == Backing::kView && next->second.view == view;
             next = std::next(next)) {
          right = next;
        }
        end = right->second.end;
      }
      if (start < range.lo) {
        range.lo = start;
        changed = true;
      }
      if (end > range.hi) {
        range.hi = end;
        changed = true;
      }
    }
  }
  return range;
}

bool Space::Overlaps(const Range& range, DWORD* owner) const {
  bool foreign = false, own = false;
  for (const Inflight& entry : inflight_) {
    if (entry.lo < range.hi && range.lo < entry.hi) {
      if (entry.owner == GetCurrentThreadId()) {
        own = true;
      } else {
        foreign = true;
      }
    }
  }
  if (owner != nullptr) *owner = foreign ? 0 : (own ? GetCurrentThreadId() : 0);
  return foreign || own;
}

std::unique_ptr<Work> Space::Begin(std::vector<Range> ranges) {
  auto work = std::make_unique<Work>(this);
  ExclusiveLock guard(*this);
  for (;;) {
    std::vector<Range> merged;
    if (ranges.size() == 1) {
      merged.push_back(Expand(ranges.front()));
    } else {
      std::vector<Range> expanded;
      expanded.reserve(ranges.size());
      for (const Range& range : ranges) expanded.push_back(Expand(range));
      std::sort(expanded.begin(), expanded.end(), [](const Range& a, const Range& b) { return a.lo < b.lo; });
      for (const Range& range : expanded) {
        if (!merged.empty() && range.lo <= merged.back().hi) {
          merged.back().hi = std::max(merged.back().hi, range.hi);
        } else {
          merged.push_back(range);
        }
      }
    }
    bool conflict = false;
    for (const Range& range : merged) {
      if (Overlaps(range, nullptr)) {
        conflict = true;
        break;
      }
    }
    if (!conflict) {
      work->claims = std::move(merged);
      break;
    }
    SleepConditionVariableSRW(&cv, &lock, INFINITE, 0);
  }
  for (const Range& claim : work->claims) {
    inflight_.push_back({claim.lo, claim.hi, GetCurrentThreadId(), work.get()});
    for (auto it = regions.lower_bound(claim.lo); it != regions.end() && it->first < claim.hi; ++it) {
      work->regions.emplace(it->first, it->second);
    }
    auto arena = arenas.upper_bound(claim.lo);
    if (arena != arenas.begin()) --arena;
    for (; arena != arenas.end() && arena->first < claim.hi; ++arena) {
      if (arena->second > claim.lo) work->arenas.emplace(arena->first, arena->second);
    }
  }
  return work;
}

void Space::Abandon(std::unique_ptr<Work> work) {
  ExclusiveLock guard(*this);
  inflight_.erase(std::remove_if(inflight_.begin(), inflight_.end(),
                                 [&](const Inflight& entry) { return entry.work == work.get(); }),
                  inflight_.end());
  WakeAllConditionVariable(&cv);
}

void Space::Publish(std::unique_ptr<Work> work) {
  struct Release {
    std::uintptr_t base, end;
    std::vector<std::uintptr_t> pieces;
  };
  std::vector<Release> releases;
  {
    ExclusiveLock guard(*this);
    std::uint64_t locked_removed = 0, locked_added = 0;
    for (const Range& claim : work->claims) {
      auto it = regions.lower_bound(claim.lo);
      while (it != regions.end() && it->first < claim.hi) {
        if (work->arenas.count(it->second.arena) == 0) {
          ++it;
          continue;
        }
        locked_removed += LockedSize(it->first, it->second);
        it = regions.erase(it);
      }
    }
    for (const auto& [base, end] : work->new_arenas) {
      arenas[base] = end;
      Track(base, end);
    }
    for (const Work::ArenaRelease& dropped : work->dropped_arenas) {
      arenas.erase(dropped.base);
      inflight_.push_back({dropped.base, dropped.end, GetCurrentThreadId(), nullptr});
    }
    std::vector<std::uintptr_t>& touched = work->arenas_touched;
    for (const auto& [start, region] : work->regions) {
      locked_added += LockedSize(start, region);
      regions.insert_or_assign(start, region);
      if (touched.empty() || touched.back() != region.arena) {
        if (std::find(touched.begin(), touched.end(), region.arena) == touched.end()) touched.push_back(region.arena);
      }
    }
    locked_bytes.fetch_add(locked_added);
    locked_bytes.fetch_sub(locked_removed);
    inflight_.erase(std::remove_if(inflight_.begin(), inflight_.end(),
                                   [&](const Inflight& entry) { return entry.work == work.get(); }),
                    inflight_.end());
    for (std::uintptr_t base : touched) {
      if (base == pinned_arena.load()) continue;
      auto arena = arenas.find(base);
      if (arena == arenas.end()) continue;
      const Range whole{base, arena->second};
      if (Overlaps(whole, nullptr)) continue;
      bool empty = true;
      Release release{base, arena->second, {}};
      for (auto it = regions.lower_bound(base); it != regions.end() && it->first < whole.hi; ++it) {
        if (IsMapped(it->second)) {
          empty = false;
          break;
        }
        release.pieces.push_back(it->first);
      }
      if (!empty) continue;
      auto it = regions.lower_bound(base);
      while (it != regions.end() && it->first < whole.hi) it = regions.erase(it);
      arenas.erase(arena);
      inflight_.push_back({whole.lo, whole.hi, GetCurrentThreadId(), nullptr});
      releases.push_back(std::move(release));
    }
    WakeAllConditionVariable(&cv);
  }
  std::vector<Work::ArenaRelease> dropped = std::move(work->dropped_arenas);
  work.reset();
  if (releases.empty() && dropped.empty()) return;
  for (const Work::ArenaRelease& arena : dropped) {
    for (std::uintptr_t allocation : arena.allocations) VirtualFree(reinterpret_cast<void*>(allocation), 0, MEM_RELEASE);
    for (std::uintptr_t view : arena.views) Api().unmap_view2(GetCurrentProcess(), reinterpret_cast<void*>(view), 0);
  }
  for (const Release& release : releases) {
    bool released = false;
    if (release.pieces.size() > 1) {
      released = CoalescePlaceholders(release.base, release.end - release.base) &&
                 VirtualFree(reinterpret_cast<void*>(release.base), 0, MEM_RELEASE);
    } else {
      released = VirtualFree(reinterpret_cast<void*>(release.base), 0, MEM_RELEASE) != FALSE;
    }
    if (!released) {
      for (std::uintptr_t piece : release.pieces) VirtualFree(reinterpret_cast<void*>(piece), 0, MEM_RELEASE);
    }
  }
  ExclusiveLock guard(*this);
  const DWORD self = GetCurrentThreadId();
  inflight_.erase(std::remove_if(inflight_.begin(), inflight_.end(),
                                 [&](const Inflight& entry) {
                                   if (entry.work != nullptr || entry.owner != self) return false;
                                   for (const Release& release : releases) {
                                     if (entry.lo == release.base && entry.hi == release.end) return true;
                                   }
                                   for (const Work::ArenaRelease& arena : dropped) {
                                     if (entry.lo == arena.base && entry.hi == arena.end) return true;
                                   }
                                   return false;
                                 }),
                  inflight_.end());
  WakeAllConditionVariable(&cv);
}

void Space::InsertFresh(std::uintptr_t arena_base, std::uintptr_t arena_end, RegionMap fresh) {
  ExclusiveLock guard(*this);
  arenas[arena_base] = arena_end;
  Track(arena_base, arena_end);
  for (auto& [start, region] : fresh) {
    locked_bytes.fetch_add(LockedSize(start, region));
    regions[start] = std::move(region);
  }
}

std::vector<Vma> Space::Vmas(std::uintptr_t lo, std::uintptr_t hi) {
  std::vector<Vma> out;
  SharedLock guard(*this);
  auto it = regions.upper_bound(lo);
  if (it != regions.begin()) {
    auto previous = std::prev(it);
    if (previous->second.end > lo) it = previous;
  }
  while (it != regions.end() && it != regions.begin() && it->first < hi && IsMapped(it->second)) {
    auto previous = std::prev(it);
    if (previous->second.end != it->first || !Mergeable(previous->first, previous->second, it->second)) break;
    it = previous;
  }
  for (; it != regions.end() && it->first < hi; ++it) {
    if (!IsMapped(it->second)) continue;
    if (!out.empty() && out.back().end == it->first) {
      auto last = std::prev(it);
      if (Mergeable(last->first, last->second, it->second)) {
        out.back().end = it->second.end;
        continue;
      }
    }
    out.push_back({it->first, it->second.end, it->second});
  }
  return out;
}

bool Space::OwnsAny(std::uintptr_t lo, std::uintptr_t hi) {
  SharedLock guard(*this);
  auto it = arenas.upper_bound(lo);
  if (it != arenas.begin()) {
    auto previous = std::prev(it);
    if (previous->second > lo) return true;
  }
  return it != arenas.end() && it->first < hi;
}

bool Space::InArena(std::uintptr_t address) {
  auto it = arenas.upper_bound(address);
  if (it == arenas.begin()) return false;
  --it;
  return address < it->second;
}

LONG Space::OnAccessViolation(EXCEPTION_RECORD* record) noexcept {
  const auto address = static_cast<std::uintptr_t>(record->ExceptionInformation[1]);
  const std::uint64_t access = record->ExceptionInformation[0];
  if (address < low_.load(std::memory_order_acquire) || address >= high_.load(std::memory_order_acquire)) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  if (t_lock_depth > 0) return EXCEPTION_CONTINUE_SEARCH;
  const std::uintptr_t page = PageDown(address);
  std::uintptr_t grow_region = 0;
  {
    SharedLock guard(*this);
    bool waited = false;
    for (;;) {
      DWORD owner = 0;
      if (!Overlaps({page, page + kPageSize}, &owner)) break;
      if (owner == GetCurrentThreadId()) return EXCEPTION_CONTINUE_SEARCH;
      SleepConditionVariableSRW(&cv, &lock, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
      waited = true;
    }
    if (waited) {
      if (t_retry_address == page) {
        if (++t_retry_count > 64) return EXCEPTION_CONTINUE_SEARCH;
      } else {
        t_retry_address = page;
        t_retry_count = 0;
      }
      return EXCEPTION_CONTINUE_EXECUTION;
    }
    auto it = regions.upper_bound(address);
    if (it == regions.begin()) return EXCEPTION_CONTINUE_SEARCH;
    --it;
    if (address >= it->second.end) return EXCEPTION_CONTINUE_SEARCH;
    const Region& region = it->second;
    if (!IsMapped(region)) {
      auto next = std::next(it);
      if (next != regions.end() && (next->second.flags & kFlagGrowsDown) != 0 && next->second.arena == region.arena &&
          ProtPermits(next->second.prot, access)) {
        grow_region = next->first;
      } else {
        return EXCEPTION_CONTINUE_SEARCH;
      }
    } else if (region.backing == Backing::kPlaceholder) {
      if (!ProtPermits(region.prot, access)) return EXCEPTION_CONTINUE_SEARCH;
      record->ExceptionCode = EXCEPTION_IN_PAGE_ERROR;
      if (record->NumberParameters < 3) record->NumberParameters = 3;
      record->ExceptionInformation[2] = static_cast<ULONG_PTR>(0xC0000011L);
      return EXCEPTION_CONTINUE_SEARCH;
    } else {
      MEMORY_BASIC_INFORMATION mbi{};
      if (VirtualQuery(reinterpret_cast<void*>(page), &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_RESERVE) {
        return EXCEPTION_CONTINUE_SEARCH;
      }
      if (region.backing == Backing::kView && (region.file == nullptr || !region.file->reserve_only)) {
        return EXCEPTION_CONTINUE_SEARCH;
      }
      const DWORD protection = RegionProtection(region);
      if (!ProtPermits(region.prot, access)) {
        if (VirtualAlloc(reinterpret_cast<void*>(page), kPageSize, MEM_COMMIT, protection) == nullptr) {
          return EXCEPTION_CONTINUE_SEARCH;
        }
        return EXCEPTION_CONTINUE_EXECUTION;
      }
      const std::uintptr_t run_lo = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
      const std::uintptr_t run_hi = run_lo + mbi.RegionSize;
      std::uintptr_t lo = std::max({GranuleDown(page), it->first, run_lo});
      std::uintptr_t hi = std::min({GranuleDown(page) + kGranularity, region.end, run_hi});
      if (VirtualAlloc(reinterpret_cast<void*>(lo), hi - lo, MEM_COMMIT, protection) == nullptr &&
          VirtualAlloc(reinterpret_cast<void*>(page), kPageSize, MEM_COMMIT, protection) == nullptr) {
        return EXCEPTION_CONTINUE_SEARCH;
      }
      if ((region.prot & guest::kPROT_EXEC) != 0) FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  }
  return GrowDown(page, grow_region);
}

LONG Space::GrowDown(std::uintptr_t page, std::uintptr_t region_start) noexcept {
  try {
    const std::uint64_t limit = StackLimit();
    std::unique_ptr<Work> work = Begin({{page, region_start + kPageSize}});
    auto top = work->Find(region_start);
    auto hole = work->Find(page);
    if (top == work->regions.end() || hole == work->regions.end() || IsMapped(hole->second) ||
        hole->second.end != region_start || (top->second.flags & kFlagGrowsDown) == 0) {
      Abandon(std::move(work));
      return EXCEPTION_CONTINUE_SEARCH;
    }
    std::uintptr_t stack_end = top->second.end;
    {
      SharedLock guard(*this);
      for (auto it = regions.find(top->second.end); it != regions.end() && (it->second.flags & kFlagGrowsDown) != 0 &&
                                                    Mergeable(std::prev(it)->first, std::prev(it)->second, it->second);
           ++it) {
        stack_end = it->second.end;
      }
    }
    bool allowed = stack_end - page <= limit;
    if (allowed) {
      SharedLock guard(*this);
      auto below = regions.lower_bound(page);
      while (below != regions.begin()) {
        --below;
        if (IsMapped(below->second)) {
          if (below->second.end + kStackGuardGap > page && (below->second.flags & kFlagGrowsDown) == 0) {
            allowed = false;
          }
          break;
        }
        if (below->second.end + kStackGuardGap <= page) break;
      }
    }
    const std::uint32_t lock_flags = top->second.flags & (kFlagLocked | kFlagLockOnFault);
    if (allowed && lock_flags != 0) {
      const std::uint64_t memlock = MemlockLimit();
      allowed = memlock == ~std::uint64_t{0} || locked_bytes.load() + (region_start - page) <= memlock;
    }
    if (!allowed) {
      Abandon(std::move(work));
      return EXCEPTION_CONTINUE_SEARCH;
    }
    if (!work->MakePrivate(page, region_start, top->second, true)) {
      Publish(std::move(work));
      return EXCEPTION_CONTINUE_SEARCH;
    }
    Publish(std::move(work));
    if (lock_flags == kFlagLocked) LockPages(page, region_start);
    return EXCEPTION_CONTINUE_EXECUTION;
  } catch (...) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
}

Work::~Work() {
  for (auto& entry : sections_) {
    if (entry.second.first != nullptr) CloseHandle(entry.second.first);
  }
}

bool Work::Fail(int guest_errno) {
  if (error == 0) error = guest_errno;
  return false;
}

RegionMap::iterator Work::Find(std::uintptr_t address) {
  auto it = regions.upper_bound(address);
  if (it == regions.begin()) return regions.end();
  --it;
  return address < it->second.end ? it : regions.end();
}

bool Work::SplitAt(std::uintptr_t address) {
  auto it = Find(address);
  if (it == regions.end() || it->first == address) return true;
  Region& region = it->second;
  if (region.backing == Backing::kPlaceholder && !SplitPlaceholder(address, region.end - address)) {
    return Fail(guest::kENOMEM);
  }
  Region tail = region;
  tail.offset += address - it->first;
  region.end = address;
  regions.emplace(address, std::move(tail));
  return true;
}

bool Work::Carve(std::uintptr_t lo, std::uintptr_t hi) {
  auto first = Find(lo);
  auto last = Find(hi - 1);
  if (first == regions.end() || last == regions.end()) return Fail(guest::kENOMEM);
  const std::uintptr_t arena = first->second.arena;
  std::size_t pieces = 0;
  for (auto it = first;; ++it) {
    if (IsMapped(it->second) || it->second.arena != arena) return Fail(guest::kENOMEM);
    ++pieces;
    if (it == last) break;
    if (std::next(it) == regions.end() || std::next(it)->first != it->second.end) return Fail(guest::kENOMEM);
  }
  if (pieces > 1) {
    const std::uintptr_t start = first->first;
    const std::uintptr_t end = last->second.end;
    if (!CoalescePlaceholders(start, end - start)) return Fail(guest::kENOMEM);
    auto it = std::next(first);
    while (it != regions.end() && it->first < end) it = regions.erase(it);
    first->second.end = end;
  }
  return SplitAt(lo) && SplitAt(hi);
}

bool Work::MakePrivate(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs, bool commit) {
  if (!Carve(lo, hi)) return false;
  const bool do_commit = commit && (attrs.prot & kProtRWX) != 0;
  if (!ReplaceWithPrivate(lo, hi - lo, do_commit, WinProtection(attrs.prot, false))) {
    return Fail(ErrnoForAllocation(GetLastError()));
  }
  Region& region = regions[lo];
  const std::uintptr_t arena = region.arena;
  region = attrs;
  region.end = hi;
  region.arena = arena;
  region.backing = Backing::kPrivate;
  region.view = 0;
  if (!do_commit && (attrs.prot & kProtRWX) != 0) region.flags |= kFlagLazy;
  if (std::find(arenas_touched.begin(), arenas_touched.end(), arena) == arenas_touched.end()) {
    arenas_touched.push_back(arena);
  }
  return true;
}

HANDLE Work::SectionFor(const FilePtr& file, std::uint64_t* object_size) {
  *object_size = 0;
  if (file == nullptr) return nullptr;
  if (file->section != nullptr) {
    *object_size = file->size;
    return file->section;
  }
  for (auto& entry : sections_) {
    if (entry.first == file.get()) {
      *object_size = entry.second.second;
      return entry.second.first;
    }
  }
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file->file, &size)) return nullptr;
  HANDLE section = nullptr;
  if (size.QuadPart > 0) {
    DWORD exec_variant, plain_variant;
    if (file->copy_on_write) {
      exec_variant = PAGE_EXECUTE_WRITECOPY;
      plain_variant = PAGE_WRITECOPY;
    } else if (file->writable) {
      exec_variant = PAGE_EXECUTE_READWRITE;
      plain_variant = PAGE_READWRITE;
    } else {
      exec_variant = PAGE_EXECUTE_READ;
      plain_variant = PAGE_READONLY;
    }
    if (file->executable) {
      section = CreateFileMappingW(file->file, nullptr, exec_variant, 0, 0, nullptr);
      if (section == nullptr) file->executable = false;
    }
    if (section == nullptr) section = CreateFileMappingW(file->file, nullptr, plain_variant, 0, 0, nullptr);
    if (section == nullptr) return nullptr;
  }
  sections_.push_back({file.get(), {section, static_cast<std::uint64_t>(size.QuadPart)}});
  *object_size = static_cast<std::uint64_t>(size.QuadPart);
  return section;
}

bool Work::MakeView(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs) {
  std::uint64_t object_size = 0;
  HANDLE section = SectionFor(attrs.file, &object_size);
  if (section == nullptr || attrs.offset >= object_size) return Fail(guest::kENOMEM);
  if (!Api().page_granular_views && ((lo % kGranularity) != 0 || (attrs.offset % kGranularity) != 0)) {
    return Fail(guest::kEINVAL);
  }
  const std::uint64_t available = object_size - attrs.offset;
  const std::uintptr_t bytes = available < hi - lo ? static_cast<std::uintptr_t>(available) : hi - lo;
  if (PageUp(bytes) != hi - lo) return Fail(guest::kENOMEM);
  if (!Carve(lo, hi)) return false;
  const bool copy_on_write = attrs.kind == Kind::kFilePrivate;
  const bool exec = attrs.file->executable;
  DWORD view_protection;
  if (copy_on_write) {
    view_protection = exec ? PAGE_EXECUTE_WRITECOPY : PAGE_WRITECOPY;
  } else if (attrs.file->writable) {
    view_protection = exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
  } else {
    view_protection = exec ? PAGE_EXECUTE_READ : PAGE_READONLY;
  }
  if (!MapViewIntoPlaceholder(section, lo, attrs.offset, bytes, view_protection)) {
    return Fail(ErrnoForAllocation(GetLastError()));
  }
  Region& region = regions[lo];
  const std::uintptr_t arena = region.arena;
  region = attrs;
  region.end = hi;
  region.arena = arena;
  region.backing = Backing::kView;
  region.view = lo;
  region.flags &= ~kFlagLazy;
  if (attrs.file->reserve_only) {
    region.flags |= kFlagLazy;
  } else {
    const DWORD wanted = WinProtection(attrs.prot, copy_on_write);
    DWORD previous = 0;
    if (wanted != view_protection && !VirtualProtect(reinterpret_cast<void*>(lo), hi - lo, wanted, &previous)) {
      return Fail(guest::kENOMEM);
    }
  }
  if (copy_on_write && (attrs.prot & guest::kPROT_WRITE) != 0) region.flags |= kFlagEverWritable;
  if (std::find(arenas_touched.begin(), arenas_touched.end(), arena) == arenas_touched.end()) {
    arenas_touched.push_back(arena);
  }
  return true;
}

bool Work::MakeEof(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs) {
  if (!Carve(lo, hi)) return false;
  Region& region = regions[lo];
  const std::uintptr_t arena = region.arena;
  region = attrs;
  region.end = hi;
  region.arena = arena;
  region.backing = Backing::kPlaceholder;
  region.view = 0;
  region.flags &= ~(kFlagLazy | kFlagEverWritable);
  return true;
}

bool Work::MapObject(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs) {
  std::uint64_t object_size = 0;
  SectionFor(attrs.file, &object_size);
  std::uintptr_t cursor = lo;
  while (cursor < hi) {
    auto it = Find(cursor);
    if (it == regions.end()) return Fail(guest::kENOMEM);
    const std::uintptr_t arena = it->second.arena;
    std::uintptr_t piece_end = it->second.end;
    for (auto r = std::next(it); r != regions.end() && r->first < hi && r->second.arena == arena &&
                                 r->first == piece_end;
         ++r) {
      piece_end = r->second.end;
    }
    if (piece_end > hi) piece_end = hi;
    Region piece = attrs;
    piece.offset = attrs.offset + (cursor - lo);
    std::uintptr_t view_end = cursor;
    if (piece.offset < object_size) {
      const std::uint64_t available = object_size - piece.offset;
      const std::uint64_t span = std::min<std::uint64_t>(available, piece_end - cursor);
      view_end = cursor + PageUp(static_cast<std::uintptr_t>(span));
    }
    if (view_end > cursor && !MakeView(cursor, view_end, piece)) return false;
    if (view_end < piece_end) {
      Region eof = attrs;
      eof.offset = attrs.offset + (view_end - lo);
      if (!MakeEof(view_end, piece_end, eof)) return false;
    }
    cursor = piece_end;
  }
  return true;
}

bool Work::MapAnonymous(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs, bool commit) {
  std::uintptr_t cursor = lo;
  while (cursor < hi) {
    auto it = Find(cursor);
    if (it == regions.end()) return Fail(guest::kENOMEM);
    const std::uintptr_t arena = it->second.arena;
    std::uintptr_t piece_end = it->second.end;
    for (auto r = std::next(it); r != regions.end() && r->first < hi && r->second.arena == arena &&
                                 r->first == piece_end;
         ++r) {
      piece_end = r->second.end;
    }
    if (piece_end > hi) piece_end = hi;
    Region piece = attrs;
    piece.offset = attrs.offset + (cursor - lo);
    if (!MakePrivate(cursor, piece_end, piece, commit)) {
      if (!commit || error != guest::kENOMEM) return false;
      MEMORYSTATUSEX status{};
      status.dwLength = sizeof(status);
      if (!GlobalMemoryStatusEx(&status) || piece_end - cursor > status.ullTotalPageFile) return false;
      error = 0;
      if (!MakePrivate(cursor, piece_end, piece, false)) return false;
    }
    cursor = piece_end;
  }
  return true;
}

bool Work::ReserveArena(std::uintptr_t base, std::uintptr_t end) {
  std::uintptr_t placed = 0;
  if (!ReservePlaceholder(base, end - base, false, false, &placed)) return Fail(guest::kENOMEM);
  new_arenas.push_back({base, end});
  regions[base] = FreeRegion(end, base);
  return true;
}

bool Work::OwnRange(std::uintptr_t lo, std::uintptr_t hi, int* guest_errno) {
  std::uintptr_t cursor = lo;
  while (cursor < hi) {
    auto it = Find(cursor);
    if (it != regions.end()) {
      cursor = it->second.end;
      continue;
    }
    auto next = regions.upper_bound(cursor);
    const std::uintptr_t gap_hi = std::min(hi, next == regions.end() ? hi : next->first);
    const std::uintptr_t base = GranuleDown(cursor);
    std::uintptr_t end = GranuleUp(gap_hi);
    if (next != regions.end() && end > next->first) end = next->first;
    for (const ForeignRun& run : QueryRuns(base, end)) {
      if (run.state != MEM_FREE) {
        *guest_errno = run.lo < gap_hi && run.hi > cursor ? guest::kEEXIST : guest::kENOMEM;
        return false;
      }
    }
    if ((base % kGranularity) != 0 || (end % kGranularity) != 0) {
      *guest_errno = guest::kENOMEM;
      return false;
    }
    if (!ReserveArena(base, end)) {
      *guest_errno = guest::kENOMEM;
      return false;
    }
    cursor = gap_hi;
  }
  return true;
}

std::vector<Range> Work::DirtyRuns(std::uintptr_t lo, std::uintptr_t hi) {
  std::vector<Range> dirty;
  DWORD previous = 0;
  if (!VirtualProtect(reinterpret_cast<void*>(lo), hi - lo, PAGE_WRITECOPY, &previous)) return dirty;
  std::vector<unsigned char> is_dirty((hi - lo) / kPageSize, 0);
  for (const ForeignRun& run : QueryRuns(lo, hi)) {
    const DWORD base = run.protect & 0xff;
    if (base == PAGE_READWRITE || base == PAGE_EXECUTE_READWRITE) {
      for (std::uintptr_t p = run.lo; p < run.hi; p += kPageSize) is_dirty[(p - lo) / kPageSize] = 1;
    }
  }
  VirtualProtect(reinterpret_cast<void*>(lo), hi - lo, PAGE_READONLY, &previous);
  if (const auto query = Api().query_working_set_ex) {
    constexpr std::size_t kBatch = 256;
    PSAPI_WORKING_SET_EX_INFORMATION info[kBatch];
    for (std::uintptr_t page = lo; page < hi;) {
      std::size_t count = 0;
      for (; count < kBatch && page + count * kPageSize < hi; ++count) {
        info[count].VirtualAddress = reinterpret_cast<void*>(page + count * kPageSize);
        info[count].VirtualAttributes.Flags = 0;
      }
      if (query(GetCurrentProcess(), info, static_cast<DWORD>(count * sizeof(info[0])))) {
        for (std::size_t i = 0; i < count; ++i) {
          if (info[i].VirtualAttributes.Valid && !info[i].VirtualAttributes.Shared) {
            is_dirty[(page - lo) / kPageSize + i] = 1;
          }
        }
      }
      page += count * kPageSize;
    }
  }
  for (std::size_t i = 0; i < is_dirty.size();) {
    if (!is_dirty[i]) {
      ++i;
      continue;
    }
    std::size_t j = i;
    while (j < is_dirty.size() && is_dirty[j]) ++j;
    dirty.push_back({lo + i * kPageSize, lo + j * kPageSize});
    i = j;
  }
  return dirty;
}

bool Work::CopyContent(std::uintptr_t src, std::uintptr_t dst, std::uintptr_t size) {
  for (const ForeignRun& run : QueryRuns(src, src + size)) {
    if (run.state != MEM_COMMIT) continue;
    const std::uintptr_t length = run.hi - run.lo;
    void* target = reinterpret_cast<void*>(dst + (run.lo - src));
    if (VirtualAlloc(target, length, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
      return Fail(ErrnoForAllocation(GetLastError()));
    }
    const DWORD base = run.protect & 0xff;
    DWORD previous = 0;
    const bool unreadable = base == PAGE_NOACCESS || base == PAGE_EXECUTE || (run.protect & PAGE_GUARD) != 0;
    if (unreadable) VirtualProtect(reinterpret_cast<void*>(run.lo), length, PAGE_READONLY, &previous);
    std::memcpy(target, reinterpret_cast<const void*>(run.lo), length);
    if (unreadable) VirtualProtect(reinterpret_cast<void*>(run.lo), length, previous, &previous);
  }
  return true;
}

bool Work::RebuildView(std::uintptr_t view, std::uintptr_t hole_lo, std::uintptr_t hole_hi) {
  struct Piece {
    std::uintptr_t lo, hi;
    Region attrs;
    bool dirty;
    std::uintptr_t scratch_offset;
  };
  auto first = regions.find(view);
  if (first == regions.end()) return Fail(guest::kENOMEM);
  std::uintptr_t view_end = view;
  std::vector<Piece> pieces;
  std::uintptr_t scratch_size = 0;
  for (auto it = first; it != regions.end() && it->first == view_end && it->second.backing == Backing::kView &&
                        it->second.view == view;
       ++it) {
    const std::uintptr_t start = it->first;
    const std::uintptr_t end = it->second.end;
    view_end = end;
    const Range kept[2] = {{start, std::min(end, hole_lo)}, {std::max(start, hole_hi), end}};
    for (const Range& part : kept) {
      if (part.lo >= part.hi) continue;
      Region attrs = it->second;
      attrs.offset = it->second.offset + (part.lo - start);
      std::vector<Range> dirty;
      if (attrs.kind == Kind::kFilePrivate && (attrs.flags & kFlagEverWritable) != 0) {
        dirty = DirtyRuns(part.lo, part.hi);
      }
      std::uintptr_t cursor = part.lo;
      for (const Range& d : dirty) {
        if (d.lo > cursor) {
          Region clean = attrs;
          clean.offset = attrs.offset + (cursor - part.lo);
          pieces.push_back({cursor, d.lo, clean, false, 0});
        }
        Region copied = attrs;
        copied.offset = attrs.offset + (d.lo - part.lo);
        pieces.push_back({d.lo, d.hi, copied, true, scratch_size});
        scratch_size += d.hi - d.lo;
        cursor = d.hi;
      }
      if (cursor < part.hi) {
        Region clean = attrs;
        clean.offset = attrs.offset + (cursor - part.lo);
        pieces.push_back({cursor, part.hi, clean, false, 0});
      }
    }
  }
  unsigned char* scratch = nullptr;
  if (scratch_size > 0) {
    scratch = static_cast<unsigned char*>(VirtualAlloc(nullptr, scratch_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (scratch == nullptr) return Fail(guest::kENOMEM);
    for (const Piece& piece : pieces) {
      if (piece.dirty) {
        std::memcpy(scratch + piece.scratch_offset, reinterpret_cast<const void*>(piece.lo), piece.hi - piece.lo);
      }
    }
  }
  const std::uintptr_t arena = first->second.arena;
  if (!UnmapViewToPlaceholder(view)) {
    if (scratch != nullptr) VirtualFree(scratch, 0, MEM_RELEASE);
    return Fail(guest::kENOMEM);
  }
  auto it = regions.find(view);
  while (it != regions.end() && it->first < view_end) it = regions.erase(it);
  regions[view] = FreeRegion(view_end, arena);
  bool ok = true;
  for (const Piece& piece : pieces) {
    if (!piece.dirty) {
      ok = MakeView(piece.lo, piece.hi, piece.attrs);
    } else {
      Region copied = piece.attrs;
      copied.flags &= ~kFlagEverWritable;
      ok = MakePrivate(piece.lo, piece.hi, copied, false);
      if (ok) {
        ok = VirtualAlloc(reinterpret_cast<void*>(piece.lo), piece.hi - piece.lo, MEM_COMMIT, PAGE_READWRITE) != nullptr;
        if (ok) {
          std::memcpy(reinterpret_cast<void*>(piece.lo), scratch + piece.scratch_offset, piece.hi - piece.lo);
          DWORD previous = 0;
          VirtualProtect(reinterpret_cast<void*>(piece.lo), piece.hi - piece.lo, WinProtection(copied.prot, false),
                         &previous);
          regions[piece.lo].flags &= ~kFlagLazy;
        } else {
          Fail(guest::kENOMEM);
        }
      }
    }
    if (!ok) break;
    if ((piece.attrs.flags & kFlagLocked) != 0) LockPages(piece.lo, piece.hi);
  }
  if (scratch != nullptr) VirtualFree(scratch, 0, MEM_RELEASE);
  return ok;
}

bool Work::ReleaseArena(std::uintptr_t base, std::uintptr_t end) {
  ArenaRelease release;
  release.base = base;
  release.end = end;
  std::set<std::uintptr_t> listed;
  std::uintptr_t last_private_end = 0;
  for (auto it = regions.lower_bound(base); it != regions.end() && it->first < end; ++it) {
    const Region& region = it->second;
    const std::uintptr_t start = it->first;
    switch (region.backing) {
      case Backing::kPlaceholder:
        release.allocations.push_back(start);
        break;
      case Backing::kPrivate: {
        std::uintptr_t allocation = start;
        if (last_private_end == start) {
          MEMORY_BASIC_INFORMATION mbi{};
          if (VirtualQuery(reinterpret_cast<void*>(start), &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State == MEM_FREE) {
            return Fail(guest::kENOMEM);
          }
          allocation = reinterpret_cast<std::uintptr_t>(mbi.AllocationBase);
        }
        last_private_end = region.end;
        if (listed.insert(allocation).second) release.allocations.push_back(allocation);
        break;
      }
      case Backing::kView:
        if (listed.insert(region.view).second) release.views.push_back(region.view);
        break;
    }
  }
  regions.erase(regions.lower_bound(base), regions.lower_bound(end));
  dropped_arenas.push_back(std::move(release));
  return true;
}

bool Work::Unmap(std::uintptr_t lo, std::uintptr_t hi, bool release_arenas) {
  for (auto arena = arenas.begin(); release_arenas && arena != arenas.end(); ++arena) {
    if (arena->first < lo || arena->second > hi) continue;
    if (arena->first == space_->pinned_arena.load()) continue;
    const std::uintptr_t arena_base = arena->first;
    if (std::any_of(dropped_arenas.begin(), dropped_arenas.end(),
                    [&](const ArenaRelease& dropped) { return dropped.base == arena_base; })) {
      continue;
    }
    auto first = regions.find(arena->first);
    if (first == regions.end()) continue;
    if (!ReleaseArena(arena->first, arena->second)) return false;
  }
  std::vector<std::uintptr_t> partial_views;
  {
    auto it = regions.upper_bound(lo);
    if (it != regions.begin()) {
      auto previous = std::prev(it);
      if (previous->second.end > lo) it = previous;
    }
    for (; it != regions.end() && it->first < hi; ++it) {
      if (it->second.backing != Backing::kView) continue;
      const std::uintptr_t view = it->second.view;
      if (!partial_views.empty() && partial_views.back() == view) continue;
      std::uintptr_t view_end = it->second.end;
      for (auto next = std::next(it); next != regions.end() && next->first == view_end &&
                                      next->second.backing == Backing::kView && next->second.view == view;
           ++next) {
        view_end = next->second.end;
      }
      if (view < lo || view_end > hi) partial_views.push_back(view);
    }
  }
  for (std::uintptr_t view : partial_views) {
    auto it = regions.find(view);
    std::uintptr_t view_end = it->second.end;
    for (auto next = std::next(it); next != regions.end() && next->first == view_end &&
                                    next->second.backing == Backing::kView && next->second.view == view;
         ++next) {
      view_end = next->second.end;
    }
    if (!RebuildView(view, std::max(lo, view), std::min(hi, view_end))) return false;
  }
  if (!SplitAt(lo) || !SplitAt(hi)) return false;
  for (auto it = regions.lower_bound(lo); it != regions.end() && it->first < hi;) {
    Region& region = it->second;
    const std::uintptr_t start = it->first;
    switch (region.backing) {
      case Backing::kPlaceholder:
        region = FreeRegion(region.end, region.arena);
        ++it;
        break;
      case Backing::kPrivate:
        if (!ReleasePrivateToPlaceholder(start, region.end - start)) return Fail(guest::kENOMEM);
        region = FreeRegion(region.end, region.arena);
        ++it;
        break;
      case Backing::kView: {
        const std::uintptr_t view = region.view;
        const std::uintptr_t arena = region.arena;
        std::uintptr_t view_end = region.end;
        auto next = std::next(it);
        for (; next != regions.end() && next->first == view_end && next->second.backing == Backing::kView &&
               next->second.view == view;
             ++next) {
          view_end = next->second.end;
        }
        if (!UnmapViewToPlaceholder(view)) return Fail(guest::kENOMEM);
        auto erase = regions.find(view);
        while (erase != regions.end() && erase->first < view_end) erase = regions.erase(erase);
        it = regions.emplace(view, FreeRegion(view_end, arena)).first;
        ++it;
        break;
      }
    }
  }
  return true;
}

namespace {
bool LazyCommitFromEnvironment() noexcept {
  wchar_t value[8] = {};
  const DWORD length = GetEnvironmentVariableW(L"ELIXIR_MM_LAZY_COMMIT", value, 8);
  return length > 0 && length < 8 && value[0] == L'1';
}
std::atomic<bool>& LazyCommitFlag() {
  static std::atomic<bool> flag{LazyCommitFromEnvironment()};
  return flag;
}
}

void SetLazyCommit(bool lazy) noexcept { LazyCommitFlag().store(lazy); }
bool LazyCommit() noexcept { return LazyCommitFlag().load(); }

std::vector<Mapping> SnapshotMappings() {
  std::vector<Mapping> out;
  try {
    for (const Vma& vma : Space::Get().Vmas(0, ~std::uintptr_t{0})) {
      Mapping mapping;
      mapping.start = vma.start;
      mapping.end = vma.end;
      mapping.prot = vma.attrs.prot;
      mapping.shared = IsShared(vma.attrs.kind);
      mapping.offset = IsFileKind(vma.attrs.kind) ? vma.attrs.offset : 0;
      if (vma.attrs.file != nullptr) {
        mapping.dev = vma.attrs.file->dev;
        mapping.ino = vma.attrs.file->ino;
        mapping.name = vma.attrs.file->path;
      }
      if ((vma.attrs.flags & kFlagHeap) != 0) mapping.name = "[heap]";
      out.push_back(std::move(mapping));
    }
  } catch (...) {
  }
  return out;
}

bool IsGuestMemory(std::uintptr_t address) noexcept {
  Space& space = Space::Get();
  for (int attempt = 0; attempt < 1000; ++attempt) {
    if (TryAcquireSRWLockShared(&space.lock)) {
      const bool inside = space.InArena(address);
      ReleaseSRWLockShared(&space.lock);
      return inside;
    }
    YieldProcessor();
  }
  return false;
}

bool PrepareGuestBuffer(const void* address, std::size_t length, bool write) noexcept {
  if (length == 0) return true;
  try {
    Space& space = Space::Get();
    const auto lo = PageDown(reinterpret_cast<std::uintptr_t>(address));
    const auto hi = PageUp(reinterpret_cast<std::uintptr_t>(address) + length);
    SharedLock guard(space);
    auto it = space.regions.upper_bound(lo);
    if (it != space.regions.begin()) {
      auto previous = std::prev(it);
      if (previous->second.end > lo) it = previous;
    }
    bool ok = true;
    for (; it != space.regions.end() && it->first < hi; ++it) {
      const Region& region = it->second;
      if ((region.flags & kFlagLazy) == 0 || !ProtPermits(region.prot, write ? 1 : 0)) continue;
      const std::uintptr_t a = std::max(lo, it->first);
      const std::uintptr_t b = std::min(hi, region.end);
      for (const ForeignRun& run : QueryRuns(a, b)) {
        if (run.state != MEM_RESERVE) continue;
        if (VirtualAlloc(reinterpret_cast<void*>(run.lo), run.hi - run.lo, MEM_COMMIT, RegionProtection(region)) ==
            nullptr) {
          ok = false;
        }
      }
    }
    return ok;
  } catch (...) {
    return false;
  }
}

Statistics GetStatistics() {
  Statistics stats;
  Space& space = Space::Get();
  SharedLock guard(space);
  stats.arenas = space.arenas.size();
  stats.regions = space.regions.size();
  for (const auto& [start, region] : space.regions) {
    if (IsMapped(region)) stats.mapped_bytes += region.end - start;
  }
  stats.locked_bytes = space.locked_bytes.load();
  stats.page_granular_views = Api().page_granular_views;
  return stats;
}

}
