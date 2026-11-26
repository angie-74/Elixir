// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// posix_madvise() is adapted from AOSP bionic libc/bionic/posix_madvise.cpp:
//
// Copyright (C) 2014 The Android Open Source Project
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <mutex>

#include "bionic/mm/mm_internal.h"

namespace elixir::bionic::mm {
namespace {

std::mutex g_foreign_lock_mutex;
std::map<std::uintptr_t, std::uintptr_t> g_foreign_locked;

std::uintptr_t ForeignLockedBytes(std::uintptr_t lo, std::uintptr_t hi) {
  std::lock_guard<std::mutex> guard(g_foreign_lock_mutex);
  std::uintptr_t total = 0;
  for (const auto& [start, end] : g_foreign_locked) {
    const std::uintptr_t a = std::max(lo, start), b = std::min(hi, end);
    if (a < b) total += b - a;
  }
  return total;
}

std::uintptr_t AddForeignLocked(std::uintptr_t lo, std::uintptr_t hi) {
  const std::uintptr_t already = ForeignLockedBytes(lo, hi);
  std::lock_guard<std::mutex> guard(g_foreign_lock_mutex);
  std::uintptr_t start = lo, end = hi;
  for (auto it = g_foreign_locked.begin(); it != g_foreign_locked.end();) {
    if (it->second >= lo && it->first <= hi) {
      start = std::min(start, it->first);
      end = std::max(end, it->second);
      it = g_foreign_locked.erase(it);
    } else {
      ++it;
    }
  }
  g_foreign_locked[start] = end;
  return (hi - lo) - already;
}

std::uintptr_t RemoveForeignLocked(std::uintptr_t lo, std::uintptr_t hi) {
  std::lock_guard<std::mutex> guard(g_foreign_lock_mutex);
  std::uintptr_t removed = 0;
  std::map<std::uintptr_t, std::uintptr_t> kept;
  for (const auto& [start, end] : g_foreign_locked) {
    const std::uintptr_t a = std::max(lo, start), b = std::min(hi, end);
    if (a >= b) {
      kept[start] = end;
      continue;
    }
    removed += b - a;
    if (start < a) kept[start] = a;
    if (b < end) kept[b] = end;
  }
  g_foreign_locked.swap(kept);
  return removed;
}

bool ValidAdvice(int advice) {
  using namespace guest;
  switch (advice) {
    case kMADV_NORMAL: case kMADV_RANDOM: case kMADV_SEQUENTIAL: case kMADV_WILLNEED: case kMADV_DONTNEED:
    case kMADV_FREE: case kMADV_REMOVE: case kMADV_DONTFORK: case kMADV_DOFORK: case kMADV_MERGEABLE:
    case kMADV_UNMERGEABLE: case kMADV_HUGEPAGE: case kMADV_NOHUGEPAGE: case kMADV_DONTDUMP: case kMADV_DODUMP:
    case kMADV_WIPEONFORK: case kMADV_KEEPONFORK: case kMADV_COLD: case kMADV_PAGEOUT: case kMADV_POPULATE_READ:
    case kMADV_POPULATE_WRITE: case kMADV_COLLAPSE: case kMADV_DONTNEED_LOCKED: case kMADV_HWPOISON:
    case kMADV_SOFT_OFFLINE:
      return true;
    default:
      return false;
  }
}

std::uint32_t FlagForAdvice(int advice, bool* set) {
  using namespace guest;
  *set = true;
  switch (advice) {
    case kMADV_RANDOM: return kFlagRandom;
    case kMADV_SEQUENTIAL: return kFlagSequential;
    case kMADV_DONTFORK: return kFlagDontFork;
    case kMADV_DOFORK: *set = false; return kFlagDontFork;
    case kMADV_DONTDUMP: return kFlagDontDump;
    case kMADV_DODUMP: *set = false; return kFlagDontDump;
    case kMADV_WIPEONFORK: return kFlagWipeOnFork;
    case kMADV_KEEPONFORK: *set = false; return kFlagWipeOnFork;
    case kMADV_HUGEPAGE: return kFlagHugePage;
    case kMADV_NOHUGEPAGE: return kFlagNoHugePage;
    case kMADV_MERGEABLE: return kFlagMergeable;
    case kMADV_UNMERGEABLE: *set = false; return kFlagMergeable;
    default: return 0;
  }
}

void Prefetch(std::uintptr_t lo, std::uintptr_t hi) {
  if (const auto prefetch = Api().prefetch) {
    WIN32_MEMORY_RANGE_ENTRY entry{reinterpret_cast<void*>(lo), hi - lo};
    prefetch(GetCurrentProcess(), 1, &entry, 0);
  }
}

void TrimWorkingSet(std::uintptr_t lo, std::uintptr_t hi) {
  for (const ForeignRun& run : QueryRuns(lo, hi)) {
    if (run.state == MEM_COMMIT) VirtualUnlock(reinterpret_cast<void*>(run.lo), run.hi - run.lo);
  }
}

bool DiscardPrivate(std::uintptr_t lo, std::uintptr_t hi, const Region& region) {
  for (const ForeignRun& run : QueryRuns(lo, hi)) {
    if (run.state != MEM_COMMIT) continue;
    void* address = reinterpret_cast<void*>(run.lo);
    const std::uintptr_t size = run.hi - run.lo;
    if (!VirtualFree(address, size, MEM_DECOMMIT)) return false;
    if ((region.flags & kFlagLazy) != 0) continue;
    if (VirtualAlloc(address, size, MEM_COMMIT, WinProtection(region.prot, false)) == nullptr) return false;
  }
  if ((region.flags & kFlagLocked) != 0 && (region.flags & kFlagLockOnFault) == 0) LockPages(lo, hi);
  return true;
}

int DiscardAnonymousFast(Work& work, std::uintptr_t start, std::uintptr_t end, bool* handled) {
  *handled = false;
  struct Piece {
    std::uintptr_t lo, hi;
    DWORD protection;
    bool recommit;
    const Region* region;
  };
  std::vector<Piece> pieces;
  for (std::uintptr_t cursor = start; cursor < end;) {
    auto it = work.Find(cursor);
    if (it == work.regions.end()) return 0;
    const Region& region = it->second;
    if (region.kind != Kind::kAnonPrivate || region.backing != Backing::kPrivate ||
        (region.flags & kFlagLocked) != 0) {
      return 0;
    }
    const std::uintptr_t hi = std::min(end, region.end);
    pieces.push_back({cursor, hi, WinProtection(region.prot, false),
                      (region.prot & kProtRWX) != 0 && (region.flags & kFlagLazy) == 0, &region});
    cursor = hi;
  }
  *handled = true;
  if (!VirtualFree(reinterpret_cast<void*>(start), end - start, MEM_DECOMMIT)) {
    for (const Piece& piece : pieces) {
      if (!VirtualFree(reinterpret_cast<void*>(piece.lo), piece.hi - piece.lo, MEM_DECOMMIT) &&
          !DiscardPrivate(piece.lo, piece.hi, *piece.region)) {
        return guest::kENOMEM;
      }
    }
  }
  for (std::size_t i = 0; i < pieces.size();) {
    if (!pieces[i].recommit) {
      ++i;
      continue;
    }
    std::size_t j = i + 1;
    while (j < pieces.size() && pieces[j].recommit && pieces[j].protection == pieces[i].protection) ++j;
    const std::uintptr_t lo = pieces[i].lo, hi = pieces[j - 1].hi;
    if (VirtualAlloc(reinterpret_cast<void*>(lo), hi - lo, MEM_COMMIT, pieces[i].protection) == nullptr) {
      for (std::size_t k = i; k < j; ++k) {
        if (VirtualAlloc(reinterpret_cast<void*>(pieces[k].lo), pieces[k].hi - pieces[k].lo, MEM_COMMIT,
                         pieces[k].protection) == nullptr) {
          return guest::kENOMEM;
        }
      }
    }
    i = j;
  }
  return 0;
}

void TouchPages(std::uintptr_t lo, std::uintptr_t hi, bool write) {
  for (std::uintptr_t page = lo; page < hi; page += kPageSize) {
    auto* p = reinterpret_cast<volatile char*>(page);
    if (write) {
      _InterlockedOr8(const_cast<char*>(p), 0);
    } else {
      (void)*p;
    }
  }
}

int DoMadvise(std::uintptr_t start, std::uint64_t len_in, int advice) {
  using namespace guest;
  if (!ValidAdvice(advice)) return kEINVAL;
  if ((start & (kPageSize - 1)) != 0) return kEINVAL;
  const std::uintptr_t len = PageUp(static_cast<std::uintptr_t>(len_in));
  if (len_in != 0 && len == 0) return kEINVAL;
  const std::uintptr_t end = start + len;
  if (end < start) return kEINVAL;
  if (end == start) return 0;
  if (advice == kMADV_HWPOISON || advice == kMADV_SOFT_OFFLINE) return kEPERM;
  Space& space = Space::Get();
  std::unique_ptr<Work> work = space.Begin({{start, end}});
  if (advice == kMADV_DONTNEED) {
    bool handled = false;
    const int fast_error = DiscardAnonymousFast(*work, start, end, &handled);
    NoteDontNeed(handled);
    if (handled) {
      space.Publish(std::move(work));
      return fast_error;
    }
  }
  int unmapped = 0;
  int error = 0;
  const bool populate = advice == kMADV_POPULATE_READ || advice == kMADV_POPULATE_WRITE;
  std::uintptr_t cursor = start;
  while (cursor < end && error == 0) {
    auto it = work->Find(cursor);
    if (it == work->regions.end()) {
      auto next = work->regions.upper_bound(cursor);
      const std::uintptr_t gap_hi = std::min(end, next == work->regions.end() ? end : next->first);
      for (const ForeignRun& run : QueryRuns(cursor, gap_hi)) {
        if (run.state == MEM_FREE) {
          if (populate) {
            error = kENOMEM;
            break;
          }
          unmapped = kENOMEM;
          continue;
        }
        const bool file_like = run.type == MEM_IMAGE || run.type == MEM_MAPPED;
        const bool committed = run.state == MEM_COMMIT;
        const DWORD base = run.protect & 0xff;
        switch (advice) {
          case kMADV_WILLNEED:
            if (committed) Prefetch(run.lo, run.hi);
            break;
          case kMADV_PAGEOUT:
          case kMADV_COLD:
            if (committed && advice == kMADV_PAGEOUT) TrimWorkingSet(run.lo, run.hi);
            break;
          case kMADV_DONTNEED:
          case kMADV_DONTNEED_LOCKED:
            if (!committed) break;
            if (file_like) {
              TrimWorkingSet(run.lo, run.hi);
            } else if (VirtualFree(reinterpret_cast<void*>(run.lo), run.hi - run.lo, MEM_DECOMMIT)) {
              VirtualAlloc(reinterpret_cast<void*>(run.lo), run.hi - run.lo, MEM_COMMIT, run.protect);
            }
            break;
          case kMADV_FREE:
            if (file_like) {
              error = kEINVAL;
            } else if (committed) {
              VirtualAlloc(reinterpret_cast<void*>(run.lo), run.hi - run.lo, MEM_RESET, PAGE_NOACCESS);
            }
            break;
          case kMADV_REMOVE:
            error = kEINVAL;
            break;
          case kMADV_WIPEONFORK:
            if (file_like) error = kEINVAL;
            break;
          case kMADV_POPULATE_READ:
          case kMADV_POPULATE_WRITE: {
            const bool readable = committed && base != PAGE_NOACCESS && (run.protect & PAGE_GUARD) == 0;
            const bool writable = base == PAGE_READWRITE || base == PAGE_EXECUTE_READWRITE || base == PAGE_WRITECOPY ||
                                  base == PAGE_EXECUTE_WRITECOPY;
            if (!readable || (advice == kMADV_POPULATE_WRITE && !writable)) {
              error = kEINVAL;
            } else {
              TouchPages(run.lo, run.hi, advice == kMADV_POPULATE_WRITE);
            }
            break;
          }
          default:
            break;
        }
        if (error != 0) break;
      }
      cursor = gap_hi;
      continue;
    }
    if (!IsMapped(it->second)) {
      if (populate) {
        error = kENOMEM;
        break;
      }
      unmapped = kENOMEM;
      cursor = it->second.end;
      continue;
    }
    const std::uintptr_t a = std::max(cursor, it->first);
    const std::uintptr_t b = std::min(end, it->second.end);
    Region& region = it->second;
    switch (advice) {
      case kMADV_NORMAL:
      case kMADV_RANDOM:
      case kMADV_SEQUENTIAL:
      case kMADV_DONTFORK:
      case kMADV_DOFORK:
      case kMADV_DONTDUMP:
      case kMADV_DODUMP:
      case kMADV_WIPEONFORK:
      case kMADV_KEEPONFORK:
      case kMADV_HUGEPAGE:
      case kMADV_NOHUGEPAGE:
      case kMADV_MERGEABLE:
      case kMADV_UNMERGEABLE: {
        if (advice == kMADV_WIPEONFORK && region.kind != Kind::kAnonPrivate) {
          error = kEINVAL;
          break;
        }
        if (!work->SplitAt(a) || !work->SplitAt(b)) {
          error = kENOMEM;
          break;
        }
        Region& target = work->Find(a)->second;
        if (advice == kMADV_NORMAL) {
          target.flags &= ~(kFlagRandom | kFlagSequential);
        } else {
          bool set = true;
          const std::uint32_t flag = FlagForAdvice(advice, &set);
          if (advice == kMADV_RANDOM) target.flags &= ~kFlagSequential;
          if (advice == kMADV_SEQUENTIAL) target.flags &= ~kFlagRandom;
          if (advice == kMADV_HUGEPAGE) target.flags &= ~kFlagNoHugePage;
          if (advice == kMADV_NOHUGEPAGE) target.flags &= ~kFlagHugePage;
          target.flags = set ? (target.flags | flag) : (target.flags & ~flag);
        }
        break;
      }
      case kMADV_COLLAPSE:
        break;
      case kMADV_WILLNEED:
        if (region.backing != Backing::kPlaceholder) Prefetch(a, b);
        break;
      case kMADV_COLD:
      case kMADV_PAGEOUT:
        if ((region.flags & kFlagLocked) != 0) {
          error = kEINVAL;
          break;
        }
        if (advice == kMADV_PAGEOUT && region.backing != Backing::kPlaceholder) TrimWorkingSet(a, b);
        break;
      case kMADV_DONTNEED:
      case kMADV_DONTNEED_LOCKED:
      case kMADV_FREE: {
        if (advice != kMADV_DONTNEED_LOCKED && (region.flags & kFlagLocked) != 0) {
          error = kEINVAL;
          break;
        }
        if (advice == kMADV_FREE && region.kind != Kind::kAnonPrivate) {
          error = kEINVAL;
          break;
        }
        if (region.backing == Backing::kPlaceholder) break;
        if (region.kind == Kind::kAnonPrivate) {
          if (advice == kMADV_FREE) {
            for (const ForeignRun& run : QueryRuns(a, b)) {
              if (run.state == MEM_COMMIT) {
                VirtualAlloc(reinterpret_cast<void*>(run.lo), run.hi - run.lo, MEM_RESET, PAGE_NOACCESS);
              }
            }
          } else if (!DiscardPrivate(a, b, region)) {
            error = kENOMEM;
          }
        } else if (region.kind == Kind::kFilePrivate) {
          if (region.backing == Backing::kView && (region.flags & kFlagEverWritable) == 0) break;
          if (!work->SplitAt(a) || !work->SplitAt(b)) {
            error = kENOMEM;
            break;
          }
          std::vector<std::pair<std::uintptr_t, Region>> olds;
          for (auto r = work->regions.lower_bound(a); r != work->regions.end() && r->first < b; ++r) {
            olds.emplace_back(r->first, r->second);
          }
          if (!work->Unmap(a, b)) {
            error = work->error != 0 ? work->error : kENOMEM;
            break;
          }
          for (auto& [s, old] : olds) {
            Region attrs = old;
            attrs.flags &= ~(kFlagLazy | kFlagEverWritable);
            if (!work->MapObject(s, old.end, attrs)) {
              error = work->error != 0 ? work->error : kENOMEM;
              break;
            }
            if ((attrs.flags & (kFlagLocked | kFlagLockOnFault)) == kFlagLocked) LockPages(s, old.end);
          }
        } else {
          if ((region.flags & kFlagLocked) == 0) TrimWorkingSet(a, b);
        }
        break;
      }
      case kMADV_REMOVE: {
        if ((region.flags & kFlagLocked) != 0) {
          error = kEINVAL;
          break;
        }
        if (region.kind == Kind::kAnonPrivate) {
          error = kEINVAL;
          break;
        }
        if (!IsShared(region.kind) || (region.max_prot & kPROT_WRITE) == 0 || (region.prot & kPROT_WRITE) == 0) {
          error = kEACCES;
          break;
        }
        if (region.backing != Backing::kView) break;
        DWORD previous = 0;
        for (const ForeignRun& run : QueryRuns(a, b)) {
          if (run.state != MEM_COMMIT) continue;
          void* address = reinterpret_cast<void*>(run.lo);
          const std::uintptr_t size = run.hi - run.lo;
          VirtualProtect(address, size, PAGE_READWRITE, &previous);
          std::memset(address, 0, size);
          VirtualProtect(address, size, previous, &previous);
        }
        break;
      }
      case kMADV_POPULATE_READ:
      case kMADV_POPULATE_WRITE: {
        const bool write = advice == kMADV_POPULATE_WRITE;
        if (!ProtPermits(region.prot, write ? 1 : 0)) {
          error = kEINVAL;
          break;
        }
        if (region.backing == Backing::kPlaceholder) {
          error = kEFAULT;
          break;
        }
        for (const ForeignRun& run : QueryRuns(a, b)) {
          if (run.state == MEM_RESERVE &&
              VirtualAlloc(reinterpret_cast<void*>(run.lo), run.hi - run.lo, MEM_COMMIT, RegionProtection(region)) ==
                  nullptr) {
            error = kENOMEM;
            break;
          }
        }
        if (error == 0) TouchPages(a, b, write);
        break;
      }
      default:
        break;
    }
    cursor = b;
  }
  space.Publish(std::move(work));
  return error != 0 ? error : unmapped;
}

int DoMincore(std::uintptr_t start, std::uint64_t length, unsigned char* vec) {
  if ((start & (kPageSize - 1)) != 0) return guest::kEINVAL;
  if (start + length < start || static_cast<std::int64_t>(start + length) < 0) return guest::kENOMEM;
  const std::uintptr_t pages = static_cast<std::uintptr_t>(length >> 12) + ((length & (kPageSize - 1)) != 0 ? 1 : 0);
  const auto vec_address = reinterpret_cast<std::uintptr_t>(vec);
  if (vec_address + pages < vec_address || static_cast<std::int64_t>(vec_address + pages) < 0) return guest::kEFAULT;
  if (pages == 0) return 0;
  const std::uintptr_t end = start + pages * kPageSize;
  if (vec == nullptr) {
    bool mapped = false;
    {
      Space& space = Space::Get();
      SharedLock guard(space);
      auto it = space.regions.upper_bound(start);
      if (it != space.regions.begin() && std::prev(it)->second.end > start) {
        mapped = IsMapped(std::prev(it)->second);
      } else {
        mapped = QueryRuns(start, start + kPageSize).front().state != MEM_FREE;
      }
    }
    return mapped ? guest::kEFAULT : guest::kENOMEM;
  }
  struct Piece {
    std::uintptr_t lo, hi;
    bool mapped;
    bool beyond_eof;
  };
  std::vector<Piece> pieces;
  {
    Space& space = Space::Get();
    SharedLock guard(space);
    auto it = space.regions.upper_bound(start);
    if (it != space.regions.begin()) {
      auto previous = std::prev(it);
      if (previous->second.end > start) it = previous;
    }
    for (; it != space.regions.end() && it->first < end; ++it) {
      pieces.push_back({std::max(start, it->first), std::min(end, it->second.end), IsMapped(it->second),
                        it->second.backing == Backing::kPlaceholder});
    }
  }
  std::uintptr_t cursor = start;
  std::size_t index = 0;
  auto emit_foreign = [&](std::uintptr_t lo, std::uintptr_t hi) -> bool {
    for (const ForeignRun& run : QueryRuns(lo, hi)) {
      if (run.state == MEM_FREE) return false;
      const std::size_t count = (run.hi - run.lo) / kPageSize;
      if (run.state == MEM_COMMIT) {
        ResidentPages(run.lo, run.hi, vec + (run.lo - start) / kPageSize);
      } else {
        std::memset(vec + (run.lo - start) / kPageSize, 0, count);
      }
    }
    return true;
  };
  for (const Piece& piece : pieces) {
    if (piece.lo > cursor && !emit_foreign(cursor, piece.lo)) return guest::kENOMEM;
    if (!piece.mapped) return guest::kENOMEM;
    index = (piece.lo - start) / kPageSize;
    if (piece.beyond_eof) {
      std::memset(vec + index, 0, (piece.hi - piece.lo) / kPageSize);
    } else {
      ResidentPages(piece.lo, piece.hi, vec + index);
    }
    cursor = piece.hi;
  }
  if (cursor < end && !emit_foreign(cursor, end)) return guest::kENOMEM;
  return 0;
}

int DoMsync(std::uintptr_t start, std::uint64_t length, int flags) {
  using namespace guest;
  if ((flags & ~(kMS_ASYNC | kMS_INVALIDATE | kMS_SYNC)) != 0) return kEINVAL;
  if ((start & (kPageSize - 1)) != 0) return kEINVAL;
  if ((flags & kMS_ASYNC) != 0 && (flags & kMS_SYNC) != 0) return kEINVAL;
  const std::uintptr_t len = PageUp(static_cast<std::uintptr_t>(length));
  const std::uintptr_t end = start + len;
  if (end < start) return kENOMEM;
  if (end == start) return 0;
  struct Piece {
    std::uintptr_t lo, hi;
    bool mapped, locked, sync;
    FilePtr file;
  };
  std::vector<Piece> pieces;
  {
    Space& space = Space::Get();
    SharedLock guard(space);
    auto it = space.regions.upper_bound(start);
    if (it != space.regions.begin()) {
      auto previous = std::prev(it);
      if (previous->second.end > start) it = previous;
    }
    for (; it != space.regions.end() && it->first < end; ++it) {
      const Region& r = it->second;
      pieces.push_back({std::max(start, it->first), std::min(end, r.end), IsMapped(r), (r.flags & kFlagLocked) != 0,
                        r.kind == Kind::kFileShared && r.backing == Backing::kView, r.file});
    }
  }
  int unmapped = 0;
  std::uintptr_t cursor = start;
  auto hole = [&]() -> bool {
    if (flags == kMS_ASYNC) return false;
    unmapped = kENOMEM;
    return true;
  };
  auto foreign = [&](std::uintptr_t lo, std::uintptr_t hi) -> int {
    for (const ForeignRun& run : QueryRuns(lo, hi)) {
      if (run.state == MEM_FREE) {
        if (run.hi >= end) return kENOMEM;
        if (!hole()) return kENOMEM;
        continue;
      }
      if ((flags & kMS_SYNC) != 0 && run.type == MEM_MAPPED) {
        FlushViewOfFile(reinterpret_cast<void*>(run.lo), run.hi - run.lo);
      }
    }
    return 0;
  };
  for (const Piece& piece : pieces) {
    if (piece.lo > cursor) {
      if (const int e = foreign(cursor, piece.lo); e != 0) return e;
    }
    if (!piece.mapped) {
      if (piece.hi >= end) return kENOMEM;
      if (!hole()) return kENOMEM;
      cursor = piece.hi;
      continue;
    }
    if ((flags & kMS_INVALIDATE) != 0 && piece.locked) return kEBUSY;
    if ((flags & kMS_SYNC) != 0 && piece.sync && piece.file != nullptr) {
      if (!FlushViewOfFile(reinterpret_cast<void*>(piece.lo), piece.hi - piece.lo)) return kEIO;
      if (piece.file->file != nullptr && piece.file->path.rfind("/memfd:", 0) != 0 &&
          !FlushFileBuffers(piece.file->file) && GetLastError() != ERROR_ACCESS_DENIED) {
        return kEIO;
      }
    }
    cursor = piece.hi;
  }
  if (cursor < end) {
    if (const int e = foreign(cursor, end); e != 0) return e;
  }
  return unmapped;
}

std::uint64_t LockedBytesIn(Work& work, std::uintptr_t lo, std::uintptr_t hi) {
  std::uint64_t total = 0;
  for (auto it = work.regions.begin(); it != work.regions.end(); ++it) {
    if ((it->second.flags & kFlagLocked) == 0) continue;
    const std::uintptr_t a = std::max(lo, it->first), b = std::min(hi, it->second.end);
    if (a < b) total += b - a;
  }
  return total + ForeignLockedBytes(lo, hi);
}

int ApplyLock(std::uintptr_t start, std::uintptr_t len, bool lock, bool on_fault) {
  const std::uintptr_t end = start + len;
  Space& space = Space::Get();
  std::unique_ptr<Work> work;
  if (lock) {
    const std::uint64_t limit_pages = MemlockLimit() >> 12;
    std::uint64_t locked = (len >> 12) + (space.locked_bytes.load() >> 12);
    if (locked > limit_pages) {
      if (end > start) {
        work = space.Begin({{start, end}});
        locked -= LockedBytesIn(*work, start, end) >> 12;
      }
      if (locked > limit_pages) {
        if (work != nullptr) space.Abandon(std::move(work));
        return guest::kENOMEM;
      }
    }
  }
  if (end <= start) {
    if (work != nullptr) space.Abandon(std::move(work));
    return end < start ? guest::kEINVAL : 0;
  }
  if (work == nullptr) work = space.Begin({{start, end}});
  int error = 0;
  bool populate_failed = false;
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
        if (lock) {
          space.locked_bytes.fetch_add(AddForeignLocked(run.lo, run.hi));
          if (!on_fault && run.state == MEM_COMMIT && !LockPages(run.lo, run.hi)) populate_failed = true;
        } else {
          space.locked_bytes.fetch_sub(RemoveForeignLocked(run.lo, run.hi));
          if (run.state == MEM_COMMIT) UnlockPages(run.lo, run.hi);
        }
      }
      cursor = gap_hi;
      continue;
    }
    if (!IsMapped(it->second)) {
      error = guest::kENOMEM;
      break;
    }
    const std::uintptr_t split_lo = std::max(cursor, it->first);
    const std::uintptr_t split_hi = std::min(end, it->second.end);
    if (!work->SplitAt(split_lo) || !work->SplitAt(split_hi)) {
      error = guest::kENOMEM;
      break;
    }
    it = work->Find(cursor);
    Region& region = it->second;
    const bool was_locked = (region.flags & kFlagLocked) != 0;
    if (lock) {
      region.flags |= kFlagLocked;
      region.flags = on_fault ? (region.flags | kFlagLockOnFault) : (region.flags & ~kFlagLockOnFault);
      if (!on_fault && region.backing != Backing::kPlaceholder) {
        if ((region.flags & kFlagLazy) != 0 && ProtPermits(region.prot, 0)) {
          for (const ForeignRun& run : QueryRuns(it->first, region.end)) {
            if (run.state == MEM_RESERVE &&
                VirtualAlloc(reinterpret_cast<void*>(run.lo), run.hi - run.lo, MEM_COMMIT,
                             RegionProtection(region)) == nullptr) {
              populate_failed = true;
            }
          }
        }
        if (!LockPages(it->first, region.end)) populate_failed = true;
      }
    } else {
      if (was_locked) UnlockPages(it->first, region.end);
      region.flags &= ~(kFlagLocked | kFlagLockOnFault);
    }
    cursor = region.end;
  }
  space.Publish(std::move(work));
  if (error != 0) return error;
  return populate_failed ? guest::kEAGAIN : 0;
}

int DoMlock(std::uintptr_t address, std::uint64_t length, bool on_fault) {
  if (MemlockLimit() == 0) return guest::kEPERM;
  const std::uintptr_t start = PageDown(address);
  const std::uintptr_t len = PageUp(static_cast<std::uintptr_t>(length) + (address & (kPageSize - 1)));
  return ApplyLock(start, len, true, on_fault);
}

int DoMunlock(std::uintptr_t address, std::uint64_t length) {
  const std::uintptr_t start = PageDown(address);
  const std::uintptr_t len = PageUp(static_cast<std::uintptr_t>(length) + (address & (kPageSize - 1)));
  return ApplyLock(start, len, false, false);
}

std::uint64_t TotalVm() {
  std::uint64_t total = 0;
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  auto address = reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
  const auto limit = reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);
  while (address < limit) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) != sizeof(mbi)) break;
    const std::uintptr_t next = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (next <= address) break;
    if (mbi.State != MEM_FREE) total += mbi.RegionSize;
    address = next;
  }
  return total;
}

int ApplyLockAll(bool lock, bool on_fault) {
  std::vector<std::pair<std::uintptr_t, std::uintptr_t>> ranges;
  {
    Space& space = Space::Get();
    SharedLock guard(space);
    for (const auto& [start, region] : space.regions) {
      if (!IsMapped(region)) continue;
      if (!ranges.empty() && ranges.back().second == start) {
        ranges.back().second = region.end;
      } else {
        ranges.emplace_back(start, region.end);
      }
    }
  }
  for (const auto& [lo, hi] : ranges) ApplyLock(lo, hi - lo, lock, on_fault);
  return 0;
}

int DoMlockall(int flags) {
  if (flags == 0 || (flags & ~(kMCL_CURRENT | kMCL_FUTURE | kMCL_ONFAULT)) != 0 || flags == kMCL_ONFAULT) {
    return guest::kEINVAL;
  }
  const std::uint64_t limit = MemlockLimit();
  if (limit == 0) return guest::kEPERM;
  if ((flags & kMCL_CURRENT) != 0 && limit != ~std::uint64_t{0} && TotalVm() > limit) return guest::kENOMEM;
  Space& space = Space::Get();
  space.lock_future.store((flags & kMCL_FUTURE) != 0 ? (kMCL_FUTURE | (flags & kMCL_ONFAULT)) : 0);
  if ((flags & kMCL_CURRENT) != 0) ApplyLockAll(true, (flags & kMCL_ONFAULT) != 0);
  return 0;
}

int DoMunlockall() {
  Space& space = Space::Get();
  space.lock_future.store(0);
  ApplyLockAll(false, false);
  std::map<std::uintptr_t, std::uintptr_t> foreign;
  {
    std::lock_guard<std::mutex> guard(g_foreign_lock_mutex);
    foreign.swap(g_foreign_locked);
  }
  for (const auto& [lo, hi] : foreign) {
    space.locked_bytes.fetch_sub(hi - lo);
    UnlockPages(lo, hi);
  }
  return 0;
}

}

}

namespace mm = elixir::bionic::mm;
namespace guest = elixir::bionic::guest;
using elixir::bionic::FailWith;
using elixir::bionic::GuestErrno;
using elixir::bionic::SetGuestErrno;
using elixir::abi::g_size;

ELIXIR_BIONIC_FN(int, madvise)(void* address, g_size length, int advice) noexcept {
  mm::MmTimer timer(mm::MmOp::kMadvise);
  return mm::Guarded(-1, [&] {
    const int error = mm::DoMadvise(reinterpret_cast<std::uintptr_t>(address), length, advice);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}

ELIXIR_BIONIC_FN(int, posix_madvise)(void* address, g_size length, int advice) noexcept {
  const int saved = GuestErrno();
  if (advice == mm::kPOSIX_MADV_DONTNEED) return 0;
  const int result = mm::Guarded(guest::kENOMEM, [&] {
    return mm::DoMadvise(reinterpret_cast<std::uintptr_t>(address), length, advice);
  });
  SetGuestErrno(saved);
  return result;
}

ELIXIR_BIONIC_FN(elixir::abi::g_ssize, process_madvise)(int pidfd, const guest::Iovec* iov, g_size count, int advice,
                                                       unsigned flags) noexcept {
  if (flags != 0) return FailWith(guest::kEINVAL, -1);
  const auto segments = static_cast<std::uint32_t>(count);
  if (segments > static_cast<std::uint32_t>(guest::kIOV_MAX)) return FailWith(guest::kEINVAL, -1);
  if (segments > 0 && iov == nullptr) return FailWith(guest::kEFAULT, -1);
  auto access_ok = [](const void* base, std::uint64_t len) {
    const auto start = reinterpret_cast<std::uint64_t>(base);
    return start + len >= start && static_cast<std::int64_t>(start + len) >= 0;
  };
  for (std::uint32_t i = 0; i < segments; ++i) {
    if (static_cast<std::int64_t>(iov[i].iov_len) < 0) return FailWith(guest::kEINVAL, -1);
  }
  constexpr std::uint64_t kMaxRwCount = 0x7ffff000;
  for (std::uint32_t i = 0; i < segments; ++i) {
    std::uint64_t len = iov[i].iov_len;
    if (segments == 1 && len > kMaxRwCount) len = kMaxRwCount;
    if (!access_ok(iov[i].iov_base, len)) return FailWith(guest::kEFAULT, -1);
  }
  (void)pidfd;
  (void)advice;
  return FailWith(guest::kEBADF, -1);
}

ELIXIR_BIONIC_FN(int, mincore)(void* address, g_size length, unsigned char* vec) noexcept {
  return mm::Guarded(-1, [&] {
    const int error = mm::DoMincore(reinterpret_cast<std::uintptr_t>(address), length, vec);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}

ELIXIR_BIONIC_FN(int, msync)(void* address, g_size length, int flags) noexcept {
  return mm::Guarded(-1, [&] {
    const int error = mm::DoMsync(reinterpret_cast<std::uintptr_t>(address), length, flags);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}

ELIXIR_BIONIC_FN(int, mlock)(const void* address, g_size length) noexcept {
  return mm::Guarded(-1, [&] {
    const int error = mm::DoMlock(reinterpret_cast<std::uintptr_t>(address), length, false);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}

ELIXIR_BIONIC_FN(int, mlock2)(const void* address, g_size length, int flags) noexcept {
  return mm::Guarded(-1, [&] {
    if ((flags & ~mm::kMLOCK_ONFAULT) != 0) return FailWith(guest::kEINVAL, -1);
    const int error = mm::DoMlock(reinterpret_cast<std::uintptr_t>(address), length, (flags & mm::kMLOCK_ONFAULT) != 0);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}

ELIXIR_BIONIC_FN(int, munlock)(const void* address, g_size length) noexcept {
  return mm::Guarded(-1, [&] {
    const int error = mm::DoMunlock(reinterpret_cast<std::uintptr_t>(address), length);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}

ELIXIR_BIONIC_FN(int, mlockall)(int flags) noexcept {
  return mm::Guarded(-1, [&] {
    const int error = mm::DoMlockall(flags);
    return error == 0 ? 0 : FailWith(error, -1);
  });
}

ELIXIR_BIONIC_FN(int, munlockall)() noexcept {
  return mm::Guarded(-1, [&] { return mm::DoMunlockall(); });
}

namespace elixir::bionic::mm {

void RegisterAdviceExports(ExportTable& table) {
  table.Add(kLibc, "madvise", reinterpret_cast<void*>(&elixir_bionic_madvise));
  table.Add(kLibc, "posix_madvise", reinterpret_cast<void*>(&elixir_bionic_posix_madvise));
  table.Add(kLibc, "process_madvise", reinterpret_cast<void*>(&elixir_bionic_process_madvise));
  table.Add(kLibc, "mincore", reinterpret_cast<void*>(&elixir_bionic_mincore));
  table.Add(kLibc, "msync", reinterpret_cast<void*>(&elixir_bionic_msync));
  table.Add(kLibc, "mlock", reinterpret_cast<void*>(&elixir_bionic_mlock));
  table.Add(kLibc, "mlock2", reinterpret_cast<void*>(&elixir_bionic_mlock2));
  table.Add(kLibc, "munlock", reinterpret_cast<void*>(&elixir_bionic_munlock));
  table.Add(kLibc, "mlockall", reinterpret_cast<void*>(&elixir_bionic_mlockall));
  table.Add(kLibc, "munlockall", reinterpret_cast<void*>(&elixir_bionic_munlockall));
}

}
