// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_PLATFORM_WINDOWS_GUEST_TLS_H_
#define ELIXIR_PLATFORM_WINDOWS_GUEST_TLS_H_

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace elixir::platform {

inline constexpr int kTlsSlotSelf = 0;
inline constexpr int kTlsSlotThreadId = 1;
inline constexpr int kTlsSlotApp = 2;
inline constexpr int kTlsSlotOpenGl = 3;
inline constexpr int kTlsSlotOpenGlApi = 4;
inline constexpr int kTlsSlotStackGuard = 5;
inline constexpr int kTlsSlotSanitizer = 6;
inline constexpr int kTlsSlotArtThreadSelf = 7;
inline constexpr int kTlsSlotDtv = 8;
inline constexpr int kTlsSlotBionicTls = 9;
inline constexpr int kTlsSlotNativeBridgeGuestState = 10;
inline constexpr int kBionicTlsSlots = 11;

struct alignas(64) GuestTcb {
  void* slots[kBionicTlsSlots];
};
static_assert(offsetof(GuestTcb, slots) == 0, "TP points at slot 0");

struct TlsSegmentImage {
  const void* init_image = nullptr;
  std::size_t init_size = 0;
  std::size_t memory_size = 0;
  std::size_t alignment = 1;
};

struct TlsModuleRegistration {
  std::size_t module_id = 0;
  bool is_static = false;
  std::int64_t tp_offset = 0;
};

class GuestThreadState;

bool InitializeGuestTls(std::size_t static_tls_capacity = 16 * 1024);
bool GuestTlsAvailable() noexcept;
std::uintptr_t GuestStackGuard() noexcept;

TlsModuleRegistration RegisterTlsModule(const TlsSegmentImage& image, bool want_static);
void UnregisterTlsModule(std::size_t module_id);

void* GetTlsBlock(std::size_t module_id);
void* FindTlsBlock(std::size_t module_id) noexcept;

bool StaticTlsBounds(void** begin, void** end) noexcept;
using DynamicTlsVisitor = void (*)(void* begin, void* end, std::size_t module_id, void* context);
bool ForEachDynamicTlsBlock(std::uint32_t windows_thread_id, DynamicTlsVisitor visitor, void* context);
using DynamicTlsListener = void (*)(void* begin, void* end);
void SetDynamicTlsListeners(DynamicTlsListener on_creation, DynamicTlsListener on_destruction) noexcept;

class GuestThreadState {
 public:
  GuestTcb* tcb() noexcept { return tcb_; }
  void* thread_pointer() noexcept { return tcb_; }
  void set_owner(void* owner) noexcept { tcb_->slots[kTlsSlotThreadId] = owner; }
  void* owner() const noexcept { return tcb_->slots[kTlsSlotThreadId]; }
  std::uint32_t windows_thread_id() const noexcept { return windows_thread_id_; }

 private:
  friend GuestThreadState* CreateGuestThreadState();
  friend void DestroyGuestThreadState(GuestThreadState*);
  friend void* GetTlsBlock(std::size_t);
  friend void* FindTlsBlock(std::size_t) noexcept;
  friend TlsModuleRegistration RegisterTlsModule(const TlsSegmentImage&, bool);
  friend void UnregisterTlsModule(std::size_t);
  friend bool EnsureDtvCapacityLocked(GuestThreadState*, std::size_t);
  friend bool StaticTlsBounds(void**, void**) noexcept;
  friend bool ForEachDynamicTlsBlock(std::uint32_t, DynamicTlsVisitor, void*);
  friend void FreeDynamicBlocksLocked(GuestThreadState*);

  void* allocation_ = nullptr;
  GuestTcb* tcb_ = nullptr;
  void** dtv_ = nullptr;
  std::size_t dtv_capacity_ = 0;
  std::uint64_t* dtv_generation_ = nullptr;
  std::uint32_t windows_thread_id_ = 0;
  GuestThreadState* registry_next_ = nullptr;
  GuestThreadState* registry_prev_ = nullptr;
};

GuestThreadState* CurrentGuestThread() noexcept;
GuestThreadState* EnsureGuestThread();
void ActivateGuestThreadPointer() noexcept;
void ReleaseCurrentGuestThread() noexcept;

using GuestThreadExitHook = void (*)(GuestThreadState* state);
void SetGuestThreadExitHook(GuestThreadExitHook hook) noexcept;

std::uint64_t FsRestoreCount() noexcept;

}

#endif
