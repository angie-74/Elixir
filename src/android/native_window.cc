// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail stubs/libandroid_stub.cc (Apache-2.0, Copyright 2026
// Mocktail Project Authors). Modified by Elixir.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "android/android_internal.h"
#include "bionic/runtime.h"

#if defined(ELIXIR_ANDROID_WITH_VULKAN)
#include "graphics/vulkan/guest_vulkan.h"
#endif

namespace {

constexpr std::int32_t kFormatRgba8888 = 1;
constexpr std::int32_t kFormatRgbx8888 = 2;
constexpr std::int32_t kFormatRgb565 = 4;
constexpr std::int32_t kNoInit = -19;
constexpr std::int32_t kBadValue = -22;
constexpr std::int32_t kInvalidOperation = -38;
constexpr std::int32_t kNoMemory = -12;
constexpr int kStrideAlignmentPixels = 16;
constexpr int kMaxBufferDimension = 16384;

int BytesPerPixel(std::int32_t format) { return format == kFormatRgb565 ? 2 : 4; }

}

struct GuestARect {
  std::int32_t left, top, right, bottom;
};
struct GuestANativeWindowBuffer {
  std::int32_t width;
  std::int32_t height;
  std::int32_t stride;
  std::int32_t format;
  void* bits;
  std::uint32_t reserved[6];
};
static_assert(sizeof(GuestANativeWindowBuffer) == 48, "ANativeWindow_Buffer");

struct ANativeWindow {
  std::atomic<int> references{1};
  std::mutex mutex;
  HWND hwnd = nullptr;
  int default_width = 0;
  int default_height = 0;
  int user_width = 0;
  int user_height = 0;
  std::int32_t user_format = 0;
  bool abandoned = false;
  bool locked = false;
  bool surface_window = false;
  ANativeWindow* parent = nullptr;
  std::uint8_t* bits = nullptr;
  std::size_t bits_size = 0;
  int buffer_width = 0;
  int buffer_height = 0;
  int buffer_stride = 0;
  std::int32_t buffer_format = 0;
  bool geometry_changed = true;
  std::vector<std::uint8_t> present_scratch;
  std::uint64_t posted_frames = 0;
  std::uint64_t presented_frames = 0;

  int Width() const { return user_width != 0 ? user_width : default_width; }
  int Height() const { return user_height != 0 ? user_height : default_height; }
  std::int32_t Format() const { return user_format != 0 ? user_format : kFormatRgba8888; }
};

namespace elixir::android {
namespace {

struct SurfaceAssociation {
  ANativeWindow* window = nullptr;
  bool holds_reference = false;
};

struct Registry {
  std::mutex mutex;
  std::unordered_map<ANativeWindow*, std::uint64_t> live;
  std::unordered_map<const void*, SurfaceAssociation> surfaces;
  std::uint64_t next_order = 1;
};

Registry& GetRegistry() {
  static Registry* registry = new Registry();
  return *registry;
}

void FreeWindow(ANativeWindow* window) noexcept {
  {
    Registry& registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    registry.live.erase(window);
    for (auto it = registry.surfaces.begin(); it != registry.surfaces.end();) {
      it = it->second.window == window ? registry.surfaces.erase(it) : std::next(it);
    }
    for (const auto& [other, order] : registry.live) {
      if (other->parent == window) other->parent = nullptr;
    }
  }
#if defined(ELIXIR_ANDROID_WITH_VULKAN)
  vulkan::UnregisterNativeWindow(window);
#endif
  if (window->bits != nullptr) VirtualFree(window->bits, 0, MEM_RELEASE);
  delete window;
}

void Acquire(ANativeWindow* window) noexcept { window->references.fetch_add(1, std::memory_order_relaxed); }

bool TryAcquire(ANativeWindow* window) noexcept {
  int count = window->references.load(std::memory_order_relaxed);
  while (count > 0) {
    if (window->references.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel)) return true;
  }
  return false;
}

void Release(ANativeWindow* window) noexcept {
  if (window->references.fetch_sub(1, std::memory_order_acq_rel) == 1) FreeWindow(window);
}

ANativeWindow* AcquirePrimary() noexcept {
  Registry& registry = GetRegistry();
  std::lock_guard<std::mutex> lock(registry.mutex);
  ANativeWindow* best = nullptr;
  std::uint64_t best_order = 0;
  for (const auto& [window, order] : registry.live) {
    if (order <= best_order || window->surface_window) continue;
    std::lock_guard<std::mutex> window_lock(window->mutex);
    if (!window->abandoned && window->references.load(std::memory_order_relaxed) > 0) {
      best = window;
      best_order = order;
    }
  }
  if (best != nullptr && !TryAcquire(best)) best = nullptr;
  return best;
}

ANativeWindow* AcquireAssociated(const void* surface) noexcept {
  Registry& registry = GetRegistry();
  std::lock_guard<std::mutex> lock(registry.mutex);
  const auto it = registry.surfaces.find(surface);
  return it != registry.surfaces.end() && TryAcquire(it->second.window) ? it->second.window : nullptr;
}

ANativeWindow* CreateSurfaceWindow(ANativeWindow* parent, const void* surface) noexcept {
  auto* window = new (std::nothrow) ANativeWindow();
  if (window == nullptr) return nullptr;
  window->surface_window = true;
  window->references.store(2, std::memory_order_relaxed);
  ANativeWindow* existing = nullptr;
  bool published = false;
  {
    Registry& registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    const auto it = registry.surfaces.find(surface);
    if (it != registry.surfaces.end() && TryAcquire(it->second.window)) {
      existing = it->second.window;
    } else {
      std::lock_guard<std::mutex> parent_lock(parent->mutex);
      if (!parent->abandoned) {
        window->hwnd = parent->hwnd;
        window->default_width = parent->default_width;
        window->default_height = parent->default_height;
        try {
          registry.live.emplace(window, registry.next_order++);
          try {
            registry.surfaces[surface] = SurfaceAssociation{window, true};
          } catch (...) {
            registry.live.erase(window);
            throw;
          }
          window->parent = parent;
          published = true;
        } catch (...) {
        }
#if defined(ELIXIR_ANDROID_WITH_VULKAN)
        if (published && window->hwnd != nullptr) vulkan::RegisterNativeWindow(window, window->hwnd);
#endif
      }
    }
  }
  if (!published) {
    delete window;
    return existing;
  }
  Log(kLogInfo, "ANativeWindow %p for Java Surface %p (HWND %p, %dx%d)", static_cast<void*>(window), surface,
      static_cast<void*>(window->hwnd), window->default_width, window->default_height);
  return window;
}

void SetDefaultSize(ANativeWindow* window, int width, int height) noexcept {
  std::lock_guard<std::mutex> lock(window->mutex);
  if (window->default_width != width || window->default_height != height) window->geometry_changed = true;
  window->default_width = width;
  window->default_height = height;
}

void Abandon(ANativeWindow* window) noexcept {
  {
    std::lock_guard<std::mutex> lock(window->mutex);
    window->abandoned = true;
    window->hwnd = nullptr;
  }
#if defined(ELIXIR_ANDROID_WITH_VULKAN)
  vulkan::UnregisterNativeWindow(window);
#endif
}

void AbandonSurfaceWindowsOf(ANativeWindow* parent) noexcept {
  if (parent == nullptr) return;
  for (;;) {
    ANativeWindow* child = nullptr;
    bool association_reference = false;
    {
      Registry& registry = GetRegistry();
      std::lock_guard<std::mutex> lock(registry.mutex);
      for (const auto& [other, order] : registry.live) {
        if (other->parent != parent) continue;
        other->parent = nullptr;
        if (TryAcquire(other)) {
          child = other;
          break;
        }
      }
      if (child == nullptr) return;
      for (auto it = registry.surfaces.begin(); it != registry.surfaces.end(); ++it) {
        if (it->second.window == child && it->second.holds_reference) {
          registry.surfaces.erase(it);
          association_reference = true;
          break;
        }
      }
    }
    Abandon(child);
    if (association_reference) Release(child);
    Release(child);
  }
}

bool EnsureBuffer(ANativeWindow* window, int width, int height, std::int32_t format) noexcept {
  if (window->bits != nullptr && window->buffer_width == width && window->buffer_height == height &&
      window->buffer_format == format) {
    return true;
  }
  if (width <= 0 || height <= 0 || width > kMaxBufferDimension || height > kMaxBufferDimension) return false;
  const int stride = (width + kStrideAlignmentPixels - 1) / kStrideAlignmentPixels * kStrideAlignmentPixels;
  const std::size_t size = static_cast<std::size_t>(stride) * static_cast<std::size_t>(height) *
                           static_cast<std::size_t>(BytesPerPixel(format));
  if (window->bits != nullptr) VirtualFree(window->bits, 0, MEM_RELEASE);
  window->bits = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (window->bits == nullptr) {
    window->bits_size = 0;
    window->buffer_width = window->buffer_height = window->buffer_stride = 0;
    return false;
  }
  window->bits_size = size;
  window->buffer_width = width;
  window->buffer_height = height;
  window->buffer_stride = stride;
  window->buffer_format = format;
  window->geometry_changed = true;
  return true;
}

bool Present(ANativeWindow* window) noexcept {
  if (window->hwnd == nullptr || !IsWindow(window->hwnd)) return false;
  RECT client{};
  if (!GetClientRect(window->hwnd, &client)) return false;
  const int width = window->buffer_width;
  const int height = window->buffer_height;
  struct {
    BITMAPINFOHEADER header;
    DWORD masks[3];
  } info{};
  info.header.biSize = sizeof(BITMAPINFOHEADER);
  info.header.biWidth = window->buffer_stride;
  info.header.biHeight = -height;
  info.header.biPlanes = 1;
  const void* pixels = window->bits;
  if (window->buffer_format == kFormatRgb565) {
    info.header.biBitCount = 16;
    info.header.biCompression = BI_BITFIELDS;
    info.masks[0] = 0xF800;
    info.masks[1] = 0x07E0;
    info.masks[2] = 0x001F;
  } else {
    info.header.biBitCount = 32;
    info.header.biCompression = BI_RGB;
    const std::size_t pixel_count = static_cast<std::size_t>(window->buffer_stride) * static_cast<std::size_t>(height);
    try {
      window->present_scratch.resize(pixel_count * 4);
    } catch (...) {
      return false;
    }
    const std::uint8_t* source = window->bits;
    std::uint8_t* target = window->present_scratch.data();
    for (std::size_t i = 0; i < pixel_count; ++i, source += 4, target += 4) {
      target[0] = source[2];
      target[1] = source[1];
      target[2] = source[0];
      target[3] = 0xff;
    }
    pixels = window->present_scratch.data();
  }
  HDC dc = GetDC(window->hwnd);
  if (dc == nullptr) return false;
  SetStretchBltMode(dc, COLORONCOLOR);
  const int lines = StretchDIBits(dc, 0, 0, client.right - client.left, client.bottom - client.top, 0, 0, width,
                                  height, pixels, reinterpret_cast<const BITMAPINFO*>(&info), DIB_RGB_COLORS, SRCCOPY);
  GdiFlush();
  ReleaseDC(window->hwnd, dc);
  return lines > 0;
}

}

ANativeWindow* CreateNativeWindow(HWND hwnd, int width, int height) {
  if ((width == 0 || height == 0) && hwnd != nullptr) {
    RECT client{};
    if (GetClientRect(hwnd, &client)) {
      width = client.right - client.left;
      height = client.bottom - client.top;
    }
  }
  auto* window = new (std::nothrow) ANativeWindow();
  if (window == nullptr) return nullptr;
  window->hwnd = hwnd;
  window->default_width = std::max(width, 0);
  window->default_height = std::max(height, 0);
  {
    Registry& registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    try {
      registry.live.emplace(window, registry.next_order++);
    } catch (...) {
      delete window;
      return nullptr;
    }
  }
#if defined(ELIXIR_ANDROID_WITH_VULKAN)
  if (hwnd != nullptr) vulkan::RegisterNativeWindow(window, hwnd);
#endif
  Log(kLogInfo, "ANativeWindow %p for HWND %p (%dx%d)", static_cast<void*>(window), static_cast<void*>(hwnd), width,
      height);
  return window;
}

void ResizeNativeWindow(ANativeWindow* window, int width, int height) {
  if (window == nullptr) return;
  width = std::max(width, 0);
  height = std::max(height, 0);
  SetDefaultSize(window, width, height);
  Registry& registry = GetRegistry();
  std::lock_guard<std::mutex> lock(registry.mutex);
  for (const auto& [other, order] : registry.live) {
    if (other->parent == window) SetDefaultSize(other, width, height);
  }
}

void DestroyNativeWindow(ANativeWindow* window) {
  if (window == nullptr) return;
  {
    std::lock_guard<std::mutex> lock(window->mutex);
    if (window->abandoned) return;
  }
  if (window->surface_window) {
    ANativeWindow* dropped = nullptr;
    {
      Registry& registry = GetRegistry();
      std::lock_guard<std::mutex> lock(registry.mutex);
      for (auto it = registry.surfaces.begin(); it != registry.surfaces.end(); ++it) {
        if (it->second.window == window && it->second.holds_reference) {
          registry.surfaces.erase(it);
          dropped = window;
          break;
        }
      }
    }
    Abandon(window);
    if (dropped != nullptr) Release(dropped);
    return;
  }
  {
    std::lock_guard<std::mutex> lock(window->mutex);
    if (window->abandoned) return;
    window->abandoned = true;
    window->hwnd = nullptr;
  }
#if defined(ELIXIR_ANDROID_WITH_VULKAN)
  vulkan::UnregisterNativeWindow(window);
#endif
  AbandonSurfaceWindowsOf(window);
  Release(window);
}

void AssociateSurface(const void* java_surface, ANativeWindow* window) {
  if (java_surface == nullptr) return;
  ANativeWindow* dropped = nullptr;
  {
    Registry& registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    const auto it = registry.surfaces.find(java_surface);
    if (window == nullptr) {
      if (it == registry.surfaces.end()) return;
      if (it->second.holds_reference) dropped = it->second.window;
      registry.surfaces.erase(it);
    } else if (it != registry.surfaces.end()) {
      if (it->second.holds_reference) dropped = it->second.window;
      it->second = SurfaceAssociation{window, false};
    } else {
      registry.surfaces.emplace(java_surface, SurfaceAssociation{window, false});
    }
  }
  if (dropped != nullptr) Release(dropped);
}

bool GetNativeWindowInfo(ANativeWindow* window, NativeWindowInfo* info) {
  if (window == nullptr || info == nullptr) return false;
  std::lock_guard<std::mutex> lock(window->mutex);
  info->hwnd = window->hwnd;
  info->width = window->Width();
  info->height = window->Height();
  info->format = window->Format();
  info->stride = window->buffer_stride;
  info->references = window->references.load(std::memory_order_relaxed);
  info->abandoned = window->abandoned;
  info->locked = window->locked;
  info->posted_frames = window->posted_frames;
  info->presented_frames = window->presented_frames;
  return true;
}

std::size_t LiveNativeWindowCount() {
  Registry& registry = GetRegistry();
  std::lock_guard<std::mutex> lock(registry.mutex);
  return registry.live.size();
}

bool PrimaryWindowGeometry(HWND* hwnd, int* width, int* height) noexcept {
  ANativeWindow* window = AcquirePrimary();
  if (window == nullptr) return false;
  {
    std::lock_guard<std::mutex> lock(window->mutex);
    if (hwnd != nullptr) *hwnd = window->hwnd;
    if (width != nullptr) *width = window->default_width;
    if (height != nullptr) *height = window->default_height;
  }
  Release(window);
  return true;
}

}

using elixir::android::GuardedVoid;
using elixir::android::Guarded;

ELIXIR_ANDROID_FN(void, ANativeWindow_acquire)(ANativeWindow* window) noexcept {
  if (window != nullptr) elixir::android::Acquire(window);
}

ELIXIR_ANDROID_FN(void, ANativeWindow_release)(ANativeWindow* window) noexcept {
  if (window != nullptr) elixir::android::Release(window);
}

ELIXIR_ANDROID_FN(ANativeWindow*, ANativeWindow_fromSurface)(void* env, void* surface) noexcept {
  using namespace elixir::android;
  return Guarded<ANativeWindow*>(nullptr, [&]() -> ANativeWindow* {
    if (surface == nullptr) return AcquirePrimary();
    if (ANativeWindow* associated = AcquireAssociated(surface)) return associated;
    ANativeWindow* primary = AcquirePrimary();
    if (primary == nullptr) return nullptr;
    ANativeWindow* window = CreateSurfaceWindow(primary, surface);
    if (window == nullptr) return primary;
    Release(primary);
    return window;
  });
}

ELIXIR_ANDROID_FN(void*, ANativeWindow_toSurface)(void* env, ANativeWindow* window) noexcept {
  using namespace elixir::android;
  return Guarded<void*>(nullptr, [&]() -> void* {
    Registry& registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    for (const auto& [surface, association] : registry.surfaces) {
      if (association.window == window) return const_cast<void*>(surface);
    }
    return nullptr;
  });
}

ELIXIR_ANDROID_FN(std::int32_t, ANativeWindow_getWidth)(ANativeWindow* window) noexcept {
  if (window == nullptr) return kBadValue;
  std::lock_guard<std::mutex> lock(window->mutex);
  return window->abandoned ? kNoInit : window->Width();
}

ELIXIR_ANDROID_FN(std::int32_t, ANativeWindow_getHeight)(ANativeWindow* window) noexcept {
  if (window == nullptr) return kBadValue;
  std::lock_guard<std::mutex> lock(window->mutex);
  return window->abandoned ? kNoInit : window->Height();
}

ELIXIR_ANDROID_FN(std::int32_t, ANativeWindow_getFormat)(ANativeWindow* window) noexcept {
  if (window == nullptr) return kBadValue;
  std::lock_guard<std::mutex> lock(window->mutex);
  return window->abandoned ? kNoInit : window->Format();
}

ELIXIR_ANDROID_FN(std::int32_t, ANativeWindow_setBuffersGeometry)(ANativeWindow* window, std::int32_t width,
                                                                   std::int32_t height, std::int32_t format) noexcept {
  if (window == nullptr) return kBadValue;
  if (width < 0 || height < 0 || (width == 0) != (height == 0)) return kBadValue;
  if (format != 0 && format != kFormatRgba8888 && format != kFormatRgbx8888 && format != kFormatRgb565) {
    return kBadValue;
  }
  std::lock_guard<std::mutex> lock(window->mutex);
  if (window->abandoned) return kNoInit;
  if (window->user_width != width || window->user_height != height || window->user_format != format) {
    window->geometry_changed = true;
  }
  window->user_width = width;
  window->user_height = height;
  window->user_format = format;
  return 0;
}

ELIXIR_ANDROID_FN(std::int32_t, ANativeWindow_lock)(ANativeWindow* window, GuestANativeWindowBuffer* out_buffer,
                                                    GuestARect* in_out_dirty_bounds) noexcept {
  if (window == nullptr || out_buffer == nullptr) return kBadValue;
  std::lock_guard<std::mutex> lock(window->mutex);
  if (window->abandoned) return kNoInit;
  if (window->locked) return kInvalidOperation;
  const int width = window->Width();
  const int height = window->Height();
  if (width <= 0 || height <= 0) return kBadValue;
  if (!elixir::android::EnsureBuffer(window, width, height, window->Format())) return kNoMemory;
  window->locked = true;
  out_buffer->width = window->buffer_width;
  out_buffer->height = window->buffer_height;
  out_buffer->stride = window->buffer_stride;
  out_buffer->format = window->buffer_format;
  out_buffer->bits = window->bits;
  std::memset(out_buffer->reserved, 0, sizeof(out_buffer->reserved));
  if (in_out_dirty_bounds != nullptr && window->geometry_changed) {
    *in_out_dirty_bounds = GuestARect{0, 0, window->buffer_width, window->buffer_height};
  }
  window->geometry_changed = false;
  return 0;
}

ELIXIR_ANDROID_FN(std::int32_t, ANativeWindow_unlockAndPost)(ANativeWindow* window) noexcept {
  if (window == nullptr) return kBadValue;
  std::lock_guard<std::mutex> lock(window->mutex);
  if (!window->locked) return kInvalidOperation;
  window->locked = false;
  if (window->abandoned) return kNoInit;
  ++window->posted_frames;
  if (elixir::android::Present(window)) ++window->presented_frames;
  return 0;
}

struct AHardwareBuffer;
ELIXIR_ANDROID_FN(void, AHardwareBuffer_acquire)(AHardwareBuffer* buffer) noexcept {}
ELIXIR_ANDROID_FN(void, AHardwareBuffer_release)(AHardwareBuffer* buffer) noexcept {}

namespace elixir::android {

void RegisterNativeWindowExports(bionic::ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibandroid, #name, reinterpret_cast<void*>(&elixir_android_##name))
  ELIXIR_ADD(ANativeWindow_acquire);
  ELIXIR_ADD(ANativeWindow_release);
  ELIXIR_ADD(ANativeWindow_fromSurface);
  ELIXIR_ADD(ANativeWindow_toSurface);
  ELIXIR_ADD(ANativeWindow_getWidth);
  ELIXIR_ADD(ANativeWindow_getHeight);
  ELIXIR_ADD(ANativeWindow_getFormat);
  ELIXIR_ADD(ANativeWindow_setBuffersGeometry);
  ELIXIR_ADD(ANativeWindow_lock);
  ELIXIR_ADD(ANativeWindow_unlockAndPost);
  ELIXIR_ADD(AHardwareBuffer_acquire);
  ELIXIR_ADD(AHardwareBuffer_release);
#undef ELIXIR_ADD
}

}
