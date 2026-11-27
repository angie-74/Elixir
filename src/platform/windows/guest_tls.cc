// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "platform/windows/guest_tls.h"

#include "platform/windows/raw_syscall_trap.h"

#include <windows.h>
#include <bcrypt.h>
#include <immintrin.h>
#include <intrin.h>

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

namespace elixir::platform {
namespace {

constexpr std::size_t kMaxStaticAlignment = alignof(GuestTcb);
constexpr std::size_t kInitialDtvCapacity = 16;

struct ModuleSlot {
  bool in_use = false;
  bool is_static = false;
  std::int64_t tp_offset = 0;
  std::uint64_t generation = 0;
  TlsSegmentImage image;
};

bool g_available = false;
std::uintptr_t g_stack_guard = 0;
std::size_t g_static_capacity = 0;
std::size_t g_static_used = 0;
std::atomic<std::uint64_t> g_restores{0};
DWORD g_fls_index = FLS_OUT_OF_INDEXES;
PVOID g_veh_handle = nullptr;

SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<ModuleSlot>* g_modules = nullptr;
GuestThreadState* g_threads = nullptr;
std::uint64_t g_next_generation = 1;

thread_local GuestThreadState* t_current = nullptr;

class ExclusiveLock {
 public:
  ExclusiveLock() noexcept { AcquireSRWLockExclusive(&g_lock); }
  ~ExclusiveLock() { ReleaseSRWLockExclusive(&g_lock); }
  ExclusiveLock(const ExclusiveLock&) = delete;
  ExclusiveLock& operator=(const ExclusiveLock&) = delete;
};

std::size_t AlignUp(std::size_t value, std::size_t alignment) noexcept {
  return (value + alignment - 1) & ~(alignment - 1);
}

__attribute__((target("fsgsbase"), noinline)) std::uint64_t ReadFsBase() { return _readfsbase_u64(); }
__attribute__((target("fsgsbase"), noinline)) void WriteFsBase(std::uint64_t base) { _writefsbase_u64(base); }

bool ProbeFsGsBase() noexcept {
  int registers[4] = {};
  __cpuidex(registers, 0, 0);
  if (registers[0] < 7) return false;
  __cpuidex(registers, 7, 0);
  if ((registers[1] & 1) == 0) return false;
  __try {
    const std::uint64_t original = ReadFsBase();
    WriteFsBase(original);
  } __except (GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION ? EXCEPTION_EXECUTE_HANDLER
                                                                  : EXCEPTION_CONTINUE_SEARCH) {
    return false;
  }
  return true;
}

void InitializeBlock(void* block, const TlsSegmentImage& image) noexcept {
  if (image.init_size != 0) std::memcpy(block, image.init_image, image.init_size);
  if (image.memory_size > image.init_size) {
    std::memset(static_cast<char*>(block) + image.init_size, 0, image.memory_size - image.init_size);
  }
}

std::size_t DynamicBlockAlignment(const ModuleSlot& module) noexcept {
  return module.image.alignment < alignof(std::max_align_t) ? alignof(std::max_align_t) : module.image.alignment;
}
std::size_t DynamicBlockSize(const ModuleSlot& module) noexcept {
  return AlignUp(module.image.memory_size, DynamicBlockAlignment(module));
}

std::atomic<DynamicTlsListener> g_on_creation{nullptr};
std::atomic<DynamicTlsListener> g_on_destruction{nullptr};

void NotifyDestruction(void* block, const ModuleSlot& module) noexcept {
  if (DynamicTlsListener listener = g_on_destruction.load(std::memory_order_acquire)) {
    listener(block, static_cast<char*>(block) + DynamicBlockSize(module));
  }
}

}

void FreeDynamicBlocksLocked(GuestThreadState* state) {
  for (std::size_t id = 1; id < state->dtv_capacity_; ++id) {
    void* block = state->dtv_[id];
    if (block == nullptr) continue;
    const bool is_static_block = block >= state->allocation_ && block < static_cast<void*>(state->tcb_);
    if (is_static_block) continue;
    if (g_modules != nullptr && id < g_modules->size()) NotifyDestruction(block, (*g_modules)[id]);
    _aligned_free(block);
    state->dtv_[id] = nullptr;
    state->dtv_generation_[id] = 0;
  }
}

bool EnsureDtvCapacityLocked(GuestThreadState* state, std::size_t module_id);

namespace {

bool InstructionUsesFs(const std::uint8_t* rip) noexcept {
  for (int i = 0; i < 15; ++i) {
    const std::uint8_t byte = rip[i];
    if (byte == 0x64) return true;
    const bool legacy_prefix = byte == 0x66 || byte == 0x67 || byte == 0xF0 || byte == 0xF2 ||
                               byte == 0xF3 || byte == 0x2E || byte == 0x36 || byte == 0x3E ||
                               byte == 0x26 || byte == 0x65;
    const bool rex = (byte & 0xF0) == 0x40;
    if (!legacy_prefix && !rex) return false;
  }
  return false;
}

void SetGuestFloatingPointDefaults() noexcept {
  _mm_setcsr(0x1F80);
}

std::atomic<GuestThreadExitHook> g_exit_hook{nullptr};

void WINAPI OnFiberLocalStorageExit(void* data) {
  auto* state = static_cast<GuestThreadState*>(data);
  if (state == nullptr || state != t_current) return;
  if (GuestThreadExitHook hook = g_exit_hook.load(std::memory_order_acquire)) {
    ActivateGuestThreadPointer();
    hook(state);
  }
  if (state == t_current) ReleaseCurrentGuestThread();
}

LONG CALLBACK FsRestoreHandler(EXCEPTION_POINTERS* info) {
  const EXCEPTION_RECORD* record = info->ExceptionRecord;
  if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
  if (record->NumberParameters >= 1 && record->ExceptionInformation[0] == 8) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  if (!InstructionUsesFs(reinterpret_cast<const std::uint8_t*>(info->ContextRecord->Rip))) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  GuestThreadState* state = t_current;
  if (state == nullptr) {
    state = EnsureGuestThread();
    if (state == nullptr) return EXCEPTION_CONTINUE_SEARCH;
    ApplyRawSyscallBreakpoints(info->ContextRecord, true);
    g_restores.fetch_add(1, std::memory_order_relaxed);
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  const auto expected = reinterpret_cast<std::uint64_t>(state->thread_pointer());
  if (ReadFsBase() == expected) return EXCEPTION_CONTINUE_SEARCH;
  WriteFsBase(expected);
  ApplyRawSyscallBreakpoints(info->ContextRecord, false);
  g_restores.fetch_add(1, std::memory_order_relaxed);
  return EXCEPTION_CONTINUE_EXECUTION;
}

}

GuestThreadState* CreateGuestThreadState() {
  ExclusiveLock lock;
  const std::size_t static_bytes = AlignUp(g_static_capacity, kMaxStaticAlignment);
  const std::size_t total = static_bytes + sizeof(GuestTcb);
  void* allocation = _aligned_malloc(total, kMaxStaticAlignment);
  auto* state = new (std::nothrow) GuestThreadState();
  if (allocation == nullptr || state == nullptr) {
    _aligned_free(allocation);
    delete state;
    return nullptr;
  }
  std::memset(allocation, 0, total);
  state->allocation_ = allocation;
  state->tcb_ = reinterpret_cast<GuestTcb*>(static_cast<char*>(allocation) + static_bytes);
  state->windows_thread_id_ = GetCurrentThreadId();
  GuestTcb* tcb = state->tcb_;
  tcb->slots[kTlsSlotSelf] = tcb;
  tcb->slots[kTlsSlotStackGuard] = reinterpret_cast<void*>(g_stack_guard);

  const std::size_t modules = g_modules != nullptr ? g_modules->size() : 1;
  state->dtv_capacity_ = modules > kInitialDtvCapacity ? modules : kInitialDtvCapacity;
  state->dtv_ = static_cast<void**>(std::calloc(state->dtv_capacity_, sizeof(void*)));
  state->dtv_generation_ = static_cast<std::uint64_t*>(std::calloc(state->dtv_capacity_, sizeof(std::uint64_t)));
  if (state->dtv_ == nullptr || state->dtv_generation_ == nullptr) {
    std::free(state->dtv_);
    std::free(state->dtv_generation_);
    _aligned_free(allocation);
    delete state;
    return nullptr;
  }
  tcb->slots[kTlsSlotDtv] = state->dtv_;

  if (g_modules != nullptr) {
    for (std::size_t id = 1; id < g_modules->size(); ++id) {
      const ModuleSlot& module = (*g_modules)[id];
      if (!module.in_use || !module.is_static) continue;
      void* block = reinterpret_cast<char*>(tcb) + module.tp_offset;
      InitializeBlock(block, module.image);
      state->dtv_[id] = block;
      state->dtv_generation_[id] = module.generation;
    }
  }

  state->registry_next_ = g_threads;
  if (g_threads != nullptr) g_threads->registry_prev_ = state;
  g_threads = state;
  return state;
}

void DestroyGuestThreadState(GuestThreadState* state) {
  if (state == nullptr) return;
  {
    ExclusiveLock lock;
    if (state->registry_prev_ != nullptr) state->registry_prev_->registry_next_ = state->registry_next_;
    if (state->registry_next_ != nullptr) state->registry_next_->registry_prev_ = state->registry_prev_;
    if (g_threads == state) g_threads = state->registry_next_;
    for (std::size_t id = 1; id < state->dtv_capacity_; ++id) {
      void* block = state->dtv_[id];
      if (block == nullptr) continue;
      const bool is_static_block = block >= state->allocation_ && block < static_cast<void*>(state->tcb_);
      if (!is_static_block) _aligned_free(block);
    }
  }
  std::free(state->dtv_);
  std::free(state->dtv_generation_);
  _aligned_free(state->allocation_);
  delete state;
}

bool InitializeGuestTls(std::size_t static_tls_capacity) {
  ExclusiveLock lock;
  if (g_available) return true;
  if (!ProbeFsGsBase()) return false;
  if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&g_stack_guard),
                                      sizeof(g_stack_guard), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
    return false;
  }
  g_static_capacity = AlignUp(static_tls_capacity, kMaxStaticAlignment);
  g_modules = new std::vector<ModuleSlot>(1);
  g_fls_index = FlsAlloc(OnFiberLocalStorageExit);
  if (g_fls_index == FLS_OUT_OF_INDEXES) return false;
  g_veh_handle = AddVectoredExceptionHandler(1, FsRestoreHandler);
  if (g_veh_handle == nullptr) return false;
  g_available = true;
  return true;
}

bool GuestTlsAvailable() noexcept { return g_available; }
std::uintptr_t GuestStackGuard() noexcept { return g_stack_guard; }
std::uint64_t FsRestoreCount() noexcept { return g_restores.load(std::memory_order_relaxed); }
GuestThreadState* CurrentGuestThread() noexcept { return t_current; }

__declspec(noinline) void CommitAdoptedThreadStack() noexcept {
  ULONG_PTR low = 0, high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  volatile char marker = 0;
  const auto here = reinterpret_cast<std::uintptr_t>(&marker);
  constexpr std::uintptr_t kPage = 4096;
  constexpr std::uintptr_t kKeep = 64 * 1024;
  constexpr std::uintptr_t kMaxCommit = 8u * 1024 * 1024;
  std::uintptr_t floor = low + kKeep;
  if (here > kMaxCommit && here - kMaxCommit > floor) floor = here - kMaxCommit;
  for (std::uintptr_t page = (here & ~(kPage - 1)) - kPage; page >= floor; page -= kPage) {
    (void)*reinterpret_cast<volatile const char*>(page);
  }
}

GuestThreadState* EnsureGuestThread() {
  if (t_current != nullptr) {
    ActivateGuestThreadPointer();
    ArmRawSyscallsOnThisThread();
    return t_current;
  }
  if (!g_available) return nullptr;
  GuestThreadState* state = CreateGuestThreadState();
  if (state == nullptr) return nullptr;
  CommitAdoptedThreadStack();
  t_current = state;
  FlsSetValue(g_fls_index, state);
  SetGuestFloatingPointDefaults();
  ActivateGuestThreadPointer();
  ArmRawSyscallsOnThisThread();
  return state;
}

void ActivateGuestThreadPointer() noexcept {
  if (t_current != nullptr) WriteFsBase(reinterpret_cast<std::uint64_t>(t_current->thread_pointer()));
}

void ResumeGuestThread() noexcept {
  GuestThreadState* state = t_current;
  if (state == nullptr) return;
  WriteFsBase(reinterpret_cast<std::uint64_t>(state->thread_pointer()));
  ArmRawSyscallsOnThisThread();
}

void SetGuestThreadExitHook(GuestThreadExitHook hook) noexcept {
  g_exit_hook.store(hook, std::memory_order_release);
}

void ReleaseCurrentGuestThread() noexcept {
  GuestThreadState* state = t_current;
  if (state == nullptr) return;
  if (g_on_destruction.load(std::memory_order_acquire) != nullptr) {
    ActivateGuestThreadPointer();
    ExclusiveLock lock;
    FreeDynamicBlocksLocked(state);
  }
  t_current = nullptr;
  FlsSetValue(g_fls_index, nullptr);
  WriteFsBase(0);
  DestroyGuestThreadState(state);
}

bool EnsureDtvCapacityLocked(GuestThreadState* state, std::size_t module_id) {
  if (module_id < state->dtv_capacity_) return true;
  std::size_t capacity = state->dtv_capacity_ * 2;
  while (capacity <= module_id) capacity *= 2;
  auto* dtv = static_cast<void**>(std::calloc(capacity, sizeof(void*)));
  auto* generation = static_cast<std::uint64_t*>(std::calloc(capacity, sizeof(std::uint64_t)));
  if (dtv == nullptr || generation == nullptr) {
    std::free(dtv);
    std::free(generation);
    return false;
  }
  std::memcpy(dtv, state->dtv_, state->dtv_capacity_ * sizeof(void*));
  std::memcpy(generation, state->dtv_generation_, state->dtv_capacity_ * sizeof(std::uint64_t));
  std::free(state->dtv_);
  std::free(state->dtv_generation_);
  state->dtv_ = dtv;
  state->dtv_generation_ = generation;
  state->dtv_capacity_ = capacity;
  state->tcb_->slots[kTlsSlotDtv] = dtv;
  return true;
}

TlsModuleRegistration RegisterTlsModule(const TlsSegmentImage& image, bool want_static) {
  TlsModuleRegistration result;
  if (image.memory_size == 0) return result;
  ExclusiveLock lock;
  if (g_modules == nullptr) return result;
  const std::size_t alignment = image.alignment == 0 ? 1 : image.alignment;
  std::size_t id = 1;
  while (id < g_modules->size() && (*g_modules)[id].in_use) ++id;
  if (id == g_modules->size()) g_modules->emplace_back();
  ModuleSlot& module = (*g_modules)[id];
  module = ModuleSlot{};
  module.in_use = true;
  module.image = image;
  module.image.alignment = alignment;
  module.generation = g_next_generation++;

  if (want_static && alignment <= kMaxStaticAlignment) {
    const std::size_t end = AlignUp(g_static_used + image.memory_size, alignment);
    if (end <= g_static_capacity) {
      g_static_used = end;
      module.is_static = true;
      module.tp_offset = -static_cast<std::int64_t>(end);
      for (GuestThreadState* thread = g_threads; thread != nullptr; thread = thread->registry_next_) {
        if (!EnsureDtvCapacityLocked(thread, id)) continue;
        void* block = reinterpret_cast<char*>(thread->tcb_) + module.tp_offset;
        InitializeBlock(block, image);
        thread->dtv_[id] = block;
        thread->dtv_generation_[id] = module.generation;
      }
    }
  }
  result.module_id = id;
  result.is_static = module.is_static;
  result.tp_offset = module.tp_offset;
  return result;
}

void UnregisterTlsModule(std::size_t module_id) {
  ExclusiveLock lock;
  if (g_modules == nullptr || module_id == 0 || module_id >= g_modules->size()) return;
  ModuleSlot& module = (*g_modules)[module_id];
  if (!module.in_use) return;
  for (GuestThreadState* thread = g_threads; thread != nullptr; thread = thread->registry_next_) {
    if (module_id >= thread->dtv_capacity_) continue;
    void* block = thread->dtv_[module_id];
    if (block != nullptr && !module.is_static) {
      NotifyDestruction(block, module);
      _aligned_free(block);
    }
    thread->dtv_[module_id] = nullptr;
    thread->dtv_generation_[module_id] = 0;
  }
  module.in_use = false;
}

void* FindTlsBlock(std::size_t module_id) noexcept {
  GuestThreadState* state = t_current;
  if (state == nullptr || module_id == 0 || module_id >= state->dtv_capacity_) return nullptr;
  return state->dtv_[module_id];
}

void* GetTlsBlock(std::size_t module_id) {
  GuestThreadState* state = t_current != nullptr ? t_current : EnsureGuestThread();
  if (state == nullptr || module_id == 0) return nullptr;
  if (module_id < state->dtv_capacity_) {
    void* block = state->dtv_[module_id];
    if (block != nullptr) return block;
  }
  ExclusiveLock lock;
  if (module_id >= g_modules->size() || !(*g_modules)[module_id].in_use) return nullptr;
  const ModuleSlot& module = (*g_modules)[module_id];
  if (!EnsureDtvCapacityLocked(state, module_id)) return nullptr;
  if (state->dtv_[module_id] != nullptr) return state->dtv_[module_id];
  void* block = nullptr;
  if (module.is_static) {
    block = reinterpret_cast<char*>(state->tcb_) + module.tp_offset;
  } else {
    block = _aligned_malloc(DynamicBlockSize(module), DynamicBlockAlignment(module));
    if (block == nullptr) return nullptr;
  }
  InitializeBlock(block, module.image);
  state->dtv_[module_id] = block;
  state->dtv_generation_[module_id] = module.generation;
  if (!module.is_static) {
    if (DynamicTlsListener listener = g_on_creation.load(std::memory_order_acquire)) {
      listener(block, static_cast<char*>(block) + DynamicBlockSize(module));
    }
  }
  return block;
}

bool StaticTlsBounds(void** begin, void** end) noexcept {
  GuestThreadState* state = t_current;
  if (state == nullptr) return false;
  *begin = state->allocation_;
  *end = reinterpret_cast<char*>(state->tcb_) + sizeof(GuestTcb);
  return true;
}

bool ForEachDynamicTlsBlock(std::uint32_t windows_thread_id, DynamicTlsVisitor visitor, void* context) {
  if (windows_thread_id == 0) windows_thread_id = GetCurrentThreadId();
  struct Block {
    void* begin;
    void* end;
    std::size_t module_id;
  };
  std::vector<Block> blocks;
  bool found = false;
  {
    AcquireSRWLockShared(&g_lock);
    try {
      GuestThreadState* state = g_threads;
      while (state != nullptr && state->windows_thread_id_ != windows_thread_id) state = state->registry_next_;
      found = state != nullptr;
      if (state != nullptr && g_modules != nullptr) {
        for (std::size_t id = 1; id < state->dtv_capacity_ && id < g_modules->size(); ++id) {
          void* block = state->dtv_[id];
          if (block == nullptr) continue;
          const bool is_static_block = block >= state->allocation_ && block < static_cast<void*>(state->tcb_);
          if (is_static_block) continue;
          blocks.push_back({block, static_cast<char*>(block) + DynamicBlockSize((*g_modules)[id]), id});
        }
      }
    } catch (...) {
      ReleaseSRWLockShared(&g_lock);
      throw;
    }
    ReleaseSRWLockShared(&g_lock);
  }
  for (const Block& block : blocks) visitor(block.begin, block.end, block.module_id, context);
  return found;
}

void SetDynamicTlsListeners(DynamicTlsListener on_creation, DynamicTlsListener on_destruction) noexcept {
  g_on_creation.store(on_creation, std::memory_order_release);
  g_on_destruction.store(on_destruction, std::memory_order_release);
}

}
