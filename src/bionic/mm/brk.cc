// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The wrappers follow bionic's libc/bionic/brk.cpp:
//
// Copyright (C) 2008 The Android Open Source Project
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

#include <atomic>
#include <cstdint>
#include <mutex>

#include "bionic/mm/mm.h"
#include "bionic/mm/mm_internal.h"

namespace elixir::bionic::mm {
namespace {

struct BrkState {
  std::mutex mutex;
  bool initialized = false;
  std::uintptr_t start_brk = 0;
  std::uintptr_t arena_end = 0;
  std::uintptr_t brk = 0;
};
BrkState& State() {
  static BrkState* state = new BrkState();
  return *state;
}

void InitializeLocked(BrkState& state) {
  if (state.initialized) return;
  state.initialized = true;
  std::uintptr_t base = 0;
  if (!ReservePlaceholder(0, kBrkArenaSize, false, false, &base)) return;
  RegionMap fresh;
  Region free_region;
  free_region.end = base + kBrkArenaSize;
  free_region.arena = base;
  fresh.emplace(base, free_region);
  Space::Get().pinned_arena.store(base);
  Space::Get().InsertFresh(base, base + kBrkArenaSize, std::move(fresh));
  state.start_brk = base;
  state.brk = base;
  state.arena_end = base + kBrkArenaSize;
}

std::uintptr_t KernelBrk(std::uintptr_t requested) {
  BrkState& state = State();
  std::lock_guard<std::mutex> guard(state.mutex);
  InitializeLocked(state);
  if (state.start_brk == 0 || requested < state.start_brk) return state.brk;
  const std::uintptr_t new_page = PageUp(requested);
  const std::uintptr_t old_page = PageUp(state.brk);
  if (new_page == old_page) {
    state.brk = requested;
    return state.brk;
  }
  Space& space = Space::Get();
  if (requested <= state.brk) {
    std::unique_ptr<Work> work = space.Begin({{new_page, old_page}});
    bool mapped = false;
    for (auto it = work->regions.begin(); it != work->regions.end() && !mapped; ++it) {
      mapped = IsMapped(it->second) && it->first < old_page && it->second.end > new_page;
    }
    if (!mapped) {
      space.Abandon(std::move(work));
      return state.brk;
    }
    state.brk = requested;
    work->Unmap(new_page, old_page);
    space.Publish(std::move(work));
    return state.brk;
  }
  if (new_page < old_page || new_page + kPageSize > state.arena_end) return state.brk;
  std::unique_ptr<Work> work = space.Begin({{old_page, new_page + kPageSize}});
  for (std::uintptr_t cursor = old_page; cursor < new_page + kPageSize;) {
    auto it = work->Find(cursor);
    if (it == work->regions.end() || IsMapped(it->second)) {
      space.Abandon(std::move(work));
      return state.brk;
    }
    cursor = it->second.end;
  }
  Region heap;
  heap.kind = Kind::kAnonPrivate;
  heap.prot = static_cast<std::uint8_t>(guest::kPROT_READ | guest::kPROT_WRITE);
  heap.max_prot = static_cast<std::uint8_t>(kProtRWX);
  heap.flags = kFlagHeap;
  const int lock_future = space.lock_future.load();
  if ((lock_future & kMCL_FUTURE) != 0) {
    heap.flags |= kFlagLocked | ((lock_future & kMCL_ONFAULT) != 0 ? kFlagLockOnFault : 0);
    const std::uint64_t limit = MemlockLimit();
    if (limit != ~std::uint64_t{0} && space.locked_bytes.load() + (new_page - old_page) > limit) {
      space.Abandon(std::move(work));
      return state.brk;
    }
  }
  const bool ok = work->MapAnonymous(old_page, new_page, heap, !LazyCommit());
  space.Publish(std::move(work));
  if (!ok) return state.brk;
  if ((heap.flags & (kFlagLocked | kFlagLockOnFault)) == kFlagLocked) LockPages(old_page, new_page);
  state.brk = requested;
  return state.brk;
}

std::atomic<std::uintptr_t> g_bionic_brk{0};

}
}

namespace mm = elixir::bionic::mm;
namespace guest = elixir::bionic::guest;
using elixir::bionic::SetGuestErrno;

ELIXIR_BIONIC_FN(int, brk)(void* end_data) noexcept {
  return mm::Guarded(-1, [&] {
    const auto requested = reinterpret_cast<std::uintptr_t>(end_data);
    mm::g_bionic_brk.store(mm::KernelBrk(requested));
    if (mm::g_bionic_brk.load() < requested) {
      SetGuestErrno(guest::kENOMEM);
      return -1;
    }
    return 0;
  });
}

ELIXIR_BIONIC_FN(void*, sbrk)(std::intptr_t increment) noexcept {
  return mm::Guarded(reinterpret_cast<void*>(-1), [&]() -> void* {
    if (mm::g_bionic_brk.load() == 0) mm::g_bionic_brk.store(mm::KernelBrk(0));
    if (increment == 0) return reinterpret_cast<void*>(mm::g_bionic_brk.load());
    const std::uintptr_t old_brk = mm::g_bionic_brk.load();
    if ((increment > 0 && static_cast<std::uintptr_t>(increment) > (UINTPTR_MAX - old_brk)) ||
        (increment < 0 && static_cast<std::uintptr_t>(-increment) > old_brk)) {
      SetGuestErrno(guest::kENOMEM);
      return reinterpret_cast<void*>(-1);
    }
    const std::uintptr_t desired = old_brk + static_cast<std::uintptr_t>(increment);
    mm::g_bionic_brk.store(mm::KernelBrk(desired));
    if (mm::g_bionic_brk.load() < desired) {
      SetGuestErrno(guest::kENOMEM);
      return reinterpret_cast<void*>(-1);
    }
    return reinterpret_cast<void*>(old_brk);
  });
}

namespace elixir::bionic::mm {

void RegisterBrkExports(ExportTable& table) {
  table.Add(kLibc, "brk", reinterpret_cast<void*>(&elixir_bionic_brk));
  table.Add(kLibc, "sbrk", reinterpret_cast<void*>(&elixir_bionic_sbrk));
}

}
