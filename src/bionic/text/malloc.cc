// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <cstdint>
#include <cstring>

#include "bionic/guest_constants.h"
#include "bionic/memory.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace elixir::bionic::text_internal {

struct GuestMallinfo {
  abi::g_size arena;
  abi::g_size ordblks;
  abi::g_size smblks;
  abi::g_size hblks;
  abi::g_size hblkhd;
  abi::g_size usmblks;
  abi::g_size fsmblks;
  abi::g_size uordblks;
  abi::g_size fordblks;
  abi::g_size keepcost;
};
static_assert(sizeof(GuestMallinfo) == 80, "bionic struct mallinfo is 10 size_t");

}

namespace {

namespace ti = elixir::bionic::text_internal;
namespace guest = elixir::bionic::guest;
using elixir::bionic::SetGuestErrno;
using ti::g_size;

constexpr g_size kMaxAlignment = static_cast<g_size>(1) << 30;

bool IsPowerOfTwo(g_size value) noexcept { return value != 0 && (value & (value - 1)) == 0; }

void* AllocateAligned(g_size alignment, g_size size) noexcept {
  if (alignment > kMaxAlignment) {
    SetGuestErrno(guest::kENOMEM);
    return nullptr;
  }
  void* block = elixir::bionic::GuestAlignedAlloc(alignment, size);
  if (block == nullptr) SetGuestErrno(guest::kENOMEM);
  return block;
}



constexpr int kMDecayTime = -100;
constexpr int kMPurge = -101;
constexpr int kMMemtagTuning = -102;
constexpr int kMThreadDisableMemInit = -103;
constexpr int kMPurgeAll = -104;
constexpr int kMCacheCountMax = -200;
constexpr int kMCacheSizeMax = -201;
constexpr int kMTsdsCountMax = -202;
constexpr int kMBionicZeroInit = -203;
constexpr int kMBionicSetHeapTaggingLevel = -204;
constexpr int kMLogStats = -205;

}

ELIXIR_BIONIC_FN(void*, malloc)(g_size size) noexcept {
  void* block = elixir::bionic::GuestMalloc(size);
  if (block == nullptr) SetGuestErrno(guest::kENOMEM);
  return block;
}

ELIXIR_BIONIC_FN(void, free)(void* block) noexcept { elixir::bionic::GuestFree(block); }

ELIXIR_BIONIC_FN(void*, calloc)(g_size count, g_size size) noexcept {
  void* block = elixir::bionic::GuestCalloc(count, size);
  if (block == nullptr) SetGuestErrno(guest::kENOMEM);
  return block;
}

ELIXIR_BIONIC_FN(void*, realloc)(void* block, g_size size) noexcept {
  if (block != nullptr && size == 0) {
    elixir::bionic::GuestFree(block);
    return nullptr;
  }
  void* result = elixir::bionic::GuestRealloc(block, size);
  if (result == nullptr) SetGuestErrno(guest::kENOMEM);
  return result;
}

ELIXIR_BIONIC_FN(void*, reallocarray)(void* block, g_size count, g_size size) noexcept {
  g_size total;
  if (__builtin_mul_overflow(count, size, &total)) {
    SetGuestErrno(guest::kENOMEM);
    return nullptr;
  }
  return elixir_bionic_realloc(block, total);
}

ELIXIR_BIONIC_FN(void*, memalign)(g_size alignment, g_size size) noexcept {
  if (alignment == 0) alignment = 1;
  if (!IsPowerOfTwo(alignment)) {
    if (alignment > kMaxAlignment) {
      SetGuestErrno(guest::kENOMEM);
      return nullptr;
    }
    g_size rounded = 1;
    while (rounded < alignment) rounded <<= 1;
    alignment = rounded;
  }
  return AllocateAligned(alignment, size);
}

ELIXIR_BIONIC_FN(int, posix_memalign)(void** result, g_size alignment, g_size size) noexcept {
  if (!IsPowerOfTwo(alignment) || alignment % sizeof(void*) != 0) return guest::kEINVAL;
  if (alignment > kMaxAlignment) return guest::kENOMEM;
  void* block = elixir::bionic::GuestAlignedAlloc(alignment, size);
  if (block == nullptr) return guest::kENOMEM;
  *result = block;
  return 0;
}

ELIXIR_BIONIC_FN(void*, aligned_alloc)(g_size alignment, g_size size) noexcept {
  if (!IsPowerOfTwo(alignment) || size % alignment != 0) {
    SetGuestErrno(guest::kEINVAL);
    return nullptr;
  }
  return AllocateAligned(alignment, size);
}

ELIXIR_BIONIC_FN(void*, valloc)(g_size size) noexcept { return AllocateAligned(ti::kGuestPageSize, size); }

ELIXIR_BIONIC_FN(void*, pvalloc)(g_size size) noexcept {
  const g_size page = ti::kGuestPageSize;
  if (size > static_cast<g_size>(-1) - page) {
    SetGuestErrno(guest::kENOMEM);
    return nullptr;
  }
  return AllocateAligned(page, (size + page - 1) & ~(page - 1));
}

ELIXIR_BIONIC_FN(g_size, malloc_usable_size)(const void* block) noexcept {
  return elixir::bionic::GuestUsableSize(block);
}

ELIXIR_BIONIC_FN(ti::GuestMallinfo, mallinfo)() noexcept {
  const elixir::bionic::GuestHeapStats stats = elixir::bionic::GuestHeapStatistics();
  ti::GuestMallinfo info = {};
  info.arena = stats.live_bytes;
  info.uordblks = stats.live_bytes;
  info.usmblks = stats.live_bytes;
  return info;
}

ELIXIR_BIONIC_FN(int, mallopt)(int param, int value) noexcept {
  switch (param) {
    case kMDecayTime:
    case kMPurge:
    case kMPurgeAll:
    case kMMemtagTuning:
    case kMThreadDisableMemInit:
    case kMCacheCountMax:
    case kMCacheSizeMax:
    case kMTsdsCountMax:
    case kMBionicZeroInit:
    case kMBionicSetHeapTaggingLevel:
    case kMLogStats:
      return 1;
    default:
      return 0;
  }
}

ELIXIR_BIONIC_FN(int, malloc_trim)(g_size pad) noexcept { return 0; }

namespace elixir::bionic::text_internal {

void RegisterMallocExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, malloc);
  ELIXIR_TEXT_EXPORT(table, kLibc, free);
  ELIXIR_TEXT_EXPORT(table, kLibc, calloc);
  ELIXIR_TEXT_EXPORT(table, kLibc, realloc);
  ELIXIR_TEXT_EXPORT(table, kLibc, reallocarray);
  ELIXIR_TEXT_EXPORT(table, kLibc, memalign);
  ELIXIR_TEXT_EXPORT(table, kLibc, posix_memalign);
  ELIXIR_TEXT_EXPORT(table, kLibc, aligned_alloc);
  ELIXIR_TEXT_EXPORT(table, kLibc, valloc);
  ELIXIR_TEXT_EXPORT(table, kLibc, pvalloc);
  ELIXIR_TEXT_EXPORT(table, kLibc, malloc_usable_size);
  ELIXIR_TEXT_EXPORT(table, kLibc, mallinfo);
  ELIXIR_TEXT_ALIAS(table, kLibc, mallinfo2, mallinfo);
  ELIXIR_TEXT_EXPORT(table, kLibc, mallopt);
  ELIXIR_TEXT_EXPORT(table, kLibc, malloc_trim);
}

}
