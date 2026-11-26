// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_MM_MM_INTERNAL_H_
#define ELIXIR_BIONIC_MM_MM_INTERNAL_H_

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "bionic/guest_constants.h"
#include "bionic/guest_types.h"
#include "bionic/runtime.h"

namespace elixir::bionic::mm {

inline constexpr int kMREMAP_MAYMOVE = 1;
inline constexpr int kMREMAP_FIXED = 2;
inline constexpr int kMREMAP_DONTUNMAP = 4;
inline constexpr int kMCL_CURRENT = 1;
inline constexpr int kMCL_FUTURE = 2;
inline constexpr int kMCL_ONFAULT = 4;
inline constexpr int kMLOCK_ONFAULT = 1;
inline constexpr unsigned kMFD_CLOEXEC = 0x1;
inline constexpr unsigned kMFD_ALLOW_SEALING = 0x2;
inline constexpr unsigned kMFD_HUGETLB = 0x4;
inline constexpr unsigned kMFD_NOEXEC_SEAL = 0x8;
inline constexpr unsigned kMFD_EXEC = 0x10;
inline constexpr unsigned kMFD_HUGE_MASK = 0x3f;
inline constexpr unsigned kMFD_HUGE_SHIFT = 26;
inline constexpr std::size_t kMFD_NAME_MAX_LEN = 249;
inline constexpr int kPOSIX_MADV_DONTNEED = 4;

inline constexpr std::uintptr_t kPageSize = 4096;
inline constexpr std::uintptr_t kGranularity = 65536;
inline constexpr std::uintptr_t kTaskSize = 0x7ffffffff000ull;
inline constexpr std::uintptr_t kMmapMinAddr = 65536;
inline constexpr std::uintptr_t kStackGuardGap = 256 * kPageSize;
inline constexpr std::uintptr_t kGrowsDownHeadroom = 8 * 1024 * 1024 + kStackGuardGap;
inline constexpr std::uintptr_t kBrkArenaSize = std::uintptr_t{1} << 30;

inline constexpr std::uintptr_t PageDown(std::uintptr_t v) { return v & ~(kPageSize - 1); }
inline constexpr std::uintptr_t PageUp(std::uintptr_t v) { return (v + kPageSize - 1) & ~(kPageSize - 1); }
inline constexpr std::uintptr_t GranuleDown(std::uintptr_t v) { return v & ~(kGranularity - 1); }
inline constexpr std::uintptr_t GranuleUp(std::uintptr_t v) { return (v + kGranularity - 1) & ~(kGranularity - 1); }

inline constexpr int kProtRWX = guest::kPROT_READ | guest::kPROT_WRITE | guest::kPROT_EXEC;

using VirtualAlloc2Fn = PVOID(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
using MapViewOfFile3Fn = PVOID(WINAPI*)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG,
                                        MEM_EXTENDED_PARAMETER*, ULONG);
using UnmapViewOfFile2Fn = BOOL(WINAPI*)(HANDLE, PVOID, ULONG);
using QueryWorkingSetExFn = BOOL(WINAPI*)(HANDLE, PVOID, DWORD);
using PrefetchVirtualMemoryFn = BOOL(WINAPI*)(HANDLE, ULONG_PTR, PWIN32_MEMORY_RANGE_ENTRY, ULONG);

struct Win32Api {
  VirtualAlloc2Fn virtual_alloc2 = nullptr;
  MapViewOfFile3Fn map_view3 = nullptr;
  UnmapViewOfFile2Fn unmap_view2 = nullptr;
  QueryWorkingSetExFn query_working_set_ex = nullptr;
  PrefetchVirtualMemoryFn prefetch = nullptr;
  bool placeholders = false;
  bool page_granular_views = false;
};
const Win32Api& Api() noexcept;

bool ReservePlaceholder(std::uintptr_t address, std::uintptr_t size, bool low_2g, bool above_4g,
                        std::uintptr_t* placed) noexcept;
bool SplitPlaceholder(std::uintptr_t start, std::uintptr_t size) noexcept;
bool CoalescePlaceholders(std::uintptr_t start, std::uintptr_t size) noexcept;
bool ReplaceWithPrivate(std::uintptr_t start, std::uintptr_t size, bool commit, DWORD protection) noexcept;
bool ReleasePrivateToPlaceholder(std::uintptr_t start, std::uintptr_t size) noexcept;
bool MapViewIntoPlaceholder(HANDLE section, std::uintptr_t start, std::uint64_t offset, std::uintptr_t bytes,
                            DWORD protection) noexcept;
bool UnmapViewToPlaceholder(std::uintptr_t view) noexcept;

enum class Kind : std::uint8_t { kFree, kAnonPrivate, kAnonShared, kFilePrivate, kFileShared };
enum class Backing : std::uint8_t { kPlaceholder, kPrivate, kView };

enum RegionFlag : std::uint32_t {
  kFlagLazy = 1u << 0,
  kFlagLocked = 1u << 1,
  kFlagLockOnFault = 1u << 2,
  kFlagGrowsDown = 1u << 3,
  kFlagHeap = 1u << 4,
  kFlagDontDump = 1u << 5,
  kFlagDontFork = 1u << 6,
  kFlagWipeOnFork = 1u << 7,
  kFlagNoReserve = 1u << 8,
  kFlagEverWritable = 1u << 9,
  kFlagRandom = 1u << 10,
  kFlagSequential = 1u << 11,
  kFlagHugePage = 1u << 12,
  kFlagNoHugePage = 1u << 13,
  kFlagMergeable = 1u << 14,
};

inline constexpr std::uint32_t kVmaFlagMask = ~(kFlagLazy | kFlagEverWritable);

struct FileObject {
  FileObject() = default;
  FileObject(const FileObject&) = delete;
  FileObject& operator=(const FileObject&) = delete;
  ~FileObject();

  HANDLE file = nullptr;
  HANDLE section = nullptr;
  bool copy_on_write = false;
  bool writable = false;
  bool executable = false;
  bool reserve_only = false;
  std::uint64_t size = 0;
  std::uint64_t dev = 0;
  std::uint64_t ino = 0;
  std::string path;
};
using FilePtr = std::shared_ptr<FileObject>;

struct Region {
  std::uintptr_t end = 0;
  std::uintptr_t arena = 0;
  std::uintptr_t view = 0;
  std::uint64_t offset = 0;
  FilePtr file;
  std::uint32_t flags = 0;
  Kind kind = Kind::kFree;
  Backing backing = Backing::kPlaceholder;
  std::uint8_t prot = 0;
  std::uint8_t max_prot = 7;
};
using RegionMap = std::map<std::uintptr_t, Region>;

inline bool IsMapped(const Region& r) { return r.kind != Kind::kFree; }
inline bool IsShared(Kind kind) { return kind == Kind::kAnonShared || kind == Kind::kFileShared; }
inline bool IsFileKind(Kind kind) { return kind == Kind::kFilePrivate || kind == Kind::kFileShared; }

DWORD WinProtection(int prot, bool copy_on_write) noexcept;
inline DWORD RegionProtection(const Region& r) {
  return WinProtection(r.prot, r.kind == Kind::kFilePrivate && r.backing == Backing::kView);
}

bool ProtPermits(int prot, std::uint64_t access) noexcept;
bool Mergeable(std::uintptr_t a_start, const Region& a, const Region& b) noexcept;

struct Range {
  std::uintptr_t lo = 0;
  std::uintptr_t hi = 0;
};

class Space;

class Work {
 public:
  explicit Work(Space* space) : space_(space) {}
  Work(const Work&) = delete;
  Work& operator=(const Work&) = delete;
  ~Work();

  RegionMap regions;
  std::vector<Range> claims;
  std::vector<std::pair<std::uintptr_t, std::uintptr_t>> new_arenas;
  std::vector<std::uintptr_t> arenas_touched;
  std::map<std::uintptr_t, std::uintptr_t> arenas;
  struct ArenaRelease {
    std::uintptr_t base = 0;
    std::uintptr_t end = 0;
    std::vector<std::uintptr_t> allocations;
    std::vector<std::uintptr_t> views;
  };
  std::vector<ArenaRelease> dropped_arenas;

  RegionMap::iterator Find(std::uintptr_t address);
  bool SplitAt(std::uintptr_t address);
  bool Unmap(std::uintptr_t lo, std::uintptr_t hi, bool release_arenas = false);
  bool Carve(std::uintptr_t lo, std::uintptr_t hi);
  bool MakePrivate(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs, bool commit);
  bool MakeView(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs);
  bool MakeEof(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs);
  bool MapObject(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs);
  bool MapAnonymous(std::uintptr_t lo, std::uintptr_t hi, const Region& attrs, bool commit);
  bool ReserveArena(std::uintptr_t base, std::uintptr_t end);
  bool OwnRange(std::uintptr_t lo, std::uintptr_t hi, int* guest_errno);
  bool CopyContent(std::uintptr_t src, std::uintptr_t dst, std::uintptr_t size);
  std::vector<Range> DirtyRuns(std::uintptr_t lo, std::uintptr_t hi);
  HANDLE SectionFor(const FilePtr& file, std::uint64_t* object_size);
  int error = 0;

 private:
  bool RebuildView(std::uintptr_t view, std::uintptr_t hole_lo, std::uintptr_t hole_hi);
  bool ReleaseArena(std::uintptr_t base, std::uintptr_t end);
  bool Fail(int guest_errno);
  Space* space_;
  std::vector<std::pair<FileObject*, std::pair<HANDLE, std::uint64_t>>> sections_;
};

struct Vma {
  std::uintptr_t start = 0;
  std::uintptr_t end = 0;
  Region attrs;
};

class Space {
 public:
  static Space& Get();

  std::unique_ptr<Work> Begin(std::vector<Range> ranges);
  void Publish(std::unique_ptr<Work> work);
  void Abandon(std::unique_ptr<Work> work);

  void InsertFresh(std::uintptr_t arena_base, std::uintptr_t arena_end, RegionMap regions);
  std::atomic<int> pending_fresh{0};

  std::vector<Vma> Vmas(std::uintptr_t lo, std::uintptr_t hi);
  bool OwnsAny(std::uintptr_t lo, std::uintptr_t hi);
  bool InArena(std::uintptr_t address);

  LONG OnAccessViolation(EXCEPTION_RECORD* record) noexcept;

  std::atomic<std::uint64_t> locked_bytes{0};
  std::atomic<int> lock_future{0};

  std::atomic<std::uintptr_t> pinned_arena{0};

  SRWLOCK lock = SRWLOCK_INIT;
  CONDITION_VARIABLE cv = CONDITION_VARIABLE_INIT;
  RegionMap regions;
  std::map<std::uintptr_t, std::uintptr_t> arenas;

 private:
  struct Inflight {
    std::uintptr_t lo, hi;
    DWORD owner;
    Work* work;
  };
  Space();
  Range Expand(Range range) const;
  bool Overlaps(const Range& range, DWORD* owner) const;
  void Track(std::uintptr_t lo, std::uintptr_t hi) noexcept;
  void ReleaseArenas(std::vector<std::pair<std::uintptr_t, std::uintptr_t>> arenas_to_release);
  LONG GrowDown(std::uintptr_t address, std::uintptr_t region_start) noexcept;
  std::vector<Inflight> inflight_;
  std::atomic<std::uintptr_t> low_{~std::uintptr_t{0}};
  std::atomic<std::uintptr_t> high_{0};
};

extern thread_local int t_lock_depth;

struct ExclusiveLock {
  explicit ExclusiveLock(Space& s) : space(s) {
    ++t_lock_depth;
    AcquireSRWLockExclusive(&space.lock);
  }
  ~ExclusiveLock() {
    ReleaseSRWLockExclusive(&space.lock);
    --t_lock_depth;
  }
  ExclusiveLock(const ExclusiveLock&) = delete;
  ExclusiveLock& operator=(const ExclusiveLock&) = delete;
  Space& space;
};
struct SharedLock {
  explicit SharedLock(Space& s) : space(s) {
    ++t_lock_depth;
    AcquireSRWLockShared(&space.lock);
  }
  ~SharedLock() {
    ReleaseSRWLockShared(&space.lock);
    --t_lock_depth;
  }
  SharedLock(const SharedLock&) = delete;
  SharedLock& operator=(const SharedLock&) = delete;
  Space& space;
};

void InstallFaultHandler() noexcept;

struct ForeignRun {
  std::uintptr_t lo, hi;
  DWORD state;
  DWORD protect;
  DWORD type;
};
std::vector<ForeignRun> QueryRuns(std::uintptr_t lo, std::uintptr_t hi);

std::string MemfdName(HANDLE handle);
std::size_t MemfdNameCount();

int ErrnoForAllocation(DWORD error) noexcept;
void ResidentPages(std::uintptr_t lo, std::uintptr_t hi, unsigned char* out) noexcept;
std::uint64_t MemlockLimit() noexcept;
std::uint64_t StackLimit() noexcept;
bool LockPages(std::uintptr_t lo, std::uintptr_t hi) noexcept;
void UnlockPages(std::uintptr_t lo, std::uintptr_t hi) noexcept;

template <typename R, typename F>
R Guarded(R failure, F&& body) noexcept {
  try {
    return body();
  } catch (const std::bad_alloc&) {
    SetGuestErrno(guest::kENOMEM);
  } catch (...) {
    SetGuestErrno(guest::kENOMEM);
  }
  return failure;
}

inline void* MapFailed() { return reinterpret_cast<void*>(~std::uintptr_t{0}); }

enum class MmOp : int { kMmap, kMunmap, kMprotect, kMadvise, kCount };
void NoteMmCall(MmOp op, std::int64_t ns) noexcept;
void NoteDontNeed(bool fast) noexcept;
class MmTimer {
 public:
  explicit MmTimer(MmOp op) noexcept;
  ~MmTimer();
  MmTimer(const MmTimer&) = delete;
  MmTimer& operator=(const MmTimer&) = delete;

 private:
  MmOp op_;
  std::int64_t start_ns_;
};

void RegisterMappingExports(ExportTable& table);
void RegisterAdviceExports(ExportTable& table);
void RegisterMemfdExports(ExportTable& table);
void RegisterBrkExports(ExportTable& table);
void RegisterMallocExtraExports(ExportTable& table);

}

#endif
