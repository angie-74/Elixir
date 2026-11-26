// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_WINDOW_WIN32_TEXT_OVERLAY_WINDOW_H_
#define ELIXIR_WINDOW_WIN32_TEXT_OVERLAY_WINDOW_H_

#include <windows.h>

#include <atomic>
#include <cstdint>

#include "mocktail/status.h"

namespace mocktail::runtime {
class RobloxTextSurfaceOverlay;
}

namespace elixir::window {

struct TextOverlayWindowStats {
  bool visible = false;
  RECT screen_rect{};
  std::uint64_t revision = 0;
  std::uint64_t presents = 0;
  std::uint64_t shows = 0;
  std::uint64_t raises = 0;
  std::uint64_t opaque_pixels = 0;
};

class Win32TextOverlayWindow final {
 public:
  Win32TextOverlayWindow() = default;
  ~Win32TextOverlayWindow();

  Win32TextOverlayWindow(const Win32TextOverlayWindow&) = delete;
  Win32TextOverlayWindow& operator=(const Win32TextOverlayWindow&) = delete;

  mocktail::Status Start(HWND owner,
                         mocktail::runtime::RobloxTextSurfaceOverlay* overlay,
                         float density_scale);
  void UpdateDensity(float density_scale);
  void Refresh();
  void Stop();

  bool started() const { return hwnd_ != nullptr; }
  HWND hwnd() const { return hwnd_; }
  TextOverlayWindowStats stats() const { return stats_; }

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                  LPARAM lparam);
  static void OnOverlayChanged(void* context);
  static void CALLBACK OnLocationChange(HWINEVENTHOOK hook, DWORD event,
                                        HWND hwnd, LONG object, LONG child,
                                        DWORD thread, DWORD time);
  void Present();
  void Hide();
  void RaiseAboveOwner();
  bool EnsureBitmap(int width, int height);
  void ReleaseBitmap();

  HWND owner_ = nullptr;
  HWND hwnd_ = nullptr;
  HWINEVENTHOOK location_hook_ = nullptr;
  mocktail::runtime::RobloxTextSurfaceOverlay* overlay_ = nullptr;
  std::atomic<bool> repaint_posted_{false};
  HDC bitmap_dc_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  HGDIOBJ previous_bitmap_ = nullptr;
  void* bits_ = nullptr;
  int bitmap_width_ = 0;
  int bitmap_height_ = 0;
  bool shown_ = false;
  TextOverlayWindowStats stats_;
};

}

#endif
