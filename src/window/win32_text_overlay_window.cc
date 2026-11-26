// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "window/win32_text_overlay_window.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "mocktail/graphics/text_overlay_frame.h"
#include "runtime/roblox_text_surface_overlay.h"

namespace elixir::window {
namespace {

constexpr wchar_t kClassName[] = L"ElixirTextOverlay";
constexpr std::uint64_t kLoggedShows = 12;
constexpr UINT kRepaintMessage = WM_APP + 0x54;

Win32TextOverlayWindow* g_hooked = nullptr;

HINSTANCE ModuleOf(const void* address) {
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          static_cast<LPCWSTR>(address), &module)) {
    return GetModuleHandleW(nullptr);
  }
  return module;
}

}

Win32TextOverlayWindow::~Win32TextOverlayWindow() { Stop(); }

mocktail::Status Win32TextOverlayWindow::Start(
    HWND owner, mocktail::runtime::RobloxTextSurfaceOverlay* overlay,
    float density_scale) {
  using mocktail::Status;
  using mocktail::StatusCode;
  if (hwnd_ != nullptr) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "the text overlay window is already started");
  }
  if (owner == nullptr || !IsWindow(owner) || overlay == nullptr) {
    return Status::Error(StatusCode::kInvalidArgument,
                         "the text overlay window needs a game window and an overlay");
  }
  const HINSTANCE instance =
      ModuleOf(reinterpret_cast<const void*>(&Win32TextOverlayWindow::WndProc));
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = &Win32TextOverlayWindow::WndProc;
  window_class.hInstance = instance;
  window_class.lpszClassName = kClassName;
  if (RegisterClassExW(&window_class) == 0 &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return Status::Error(StatusCode::kPlatformError,
                         "cannot register the text overlay window class (error " +
                             std::to_string(GetLastError()) + ")");
  }
  hwnd_ = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
      kClassName, L"", WS_POPUP, 0, 0, 1, 1, owner, nullptr, instance, this);
  if (hwnd_ == nullptr) {
    return Status::Error(StatusCode::kPlatformError,
                         "cannot create the text overlay window (error " +
                             std::to_string(GetLastError()) + ")");
  }
  owner_ = owner;
  overlay_ = overlay;
  static_cast<void>(overlay_->SetRasterScale(density_scale));
  location_hook_ = SetWinEventHook(
      EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
      &Win32TextOverlayWindow::OnLocationChange, GetCurrentProcessId(),
      GetCurrentThreadId(), WINEVENT_OUTOFCONTEXT);
  if (location_hook_ != nullptr) {
    g_hooked = this;
  }
  overlay_->SetChangeCallback(&Win32TextOverlayWindow::OnOverlayChanged, this);
  OnOverlayChanged(this);
  return Status::Ok();
}

void Win32TextOverlayWindow::UpdateDensity(float density_scale) {
  if (overlay_ == nullptr) {
    return;
  }
  static_cast<void>(overlay_->SetRasterScale(density_scale));
  Refresh();
}

void Win32TextOverlayWindow::Refresh() {
  if (hwnd_ != nullptr) {
    Present();
  }
}

void Win32TextOverlayWindow::Stop() {
  if (overlay_ != nullptr) {
    overlay_->SetChangeCallback(nullptr, nullptr);
  }
  if (location_hook_ != nullptr) {
    UnhookWinEvent(location_hook_);
    location_hook_ = nullptr;
  }
  if (g_hooked == this) {
    g_hooked = nullptr;
  }
  if (hwnd_ != nullptr) {
    Hide();
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  ReleaseBitmap();
  overlay_ = nullptr;
  owner_ = nullptr;
  shown_ = false;
  repaint_posted_.store(false);
  stats_.visible = false;
}

LRESULT CALLBACK Win32TextOverlayWindow::WndProc(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }
  auto* self = reinterpret_cast<Win32TextOverlayWindow*>(
      GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  switch (message) {
    case kRepaintMessage:
      if (self != nullptr) {
        self->repaint_posted_.store(false);
        self->Present();
      }
      return 0;
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_NCHITTEST:
      return HTTRANSPARENT;
    case WM_NCDESTROY:
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
      break;
    default:
      break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

void Win32TextOverlayWindow::OnOverlayChanged(void* context) {
  auto* self = static_cast<Win32TextOverlayWindow*>(context);
  if (self == nullptr || self->hwnd_ == nullptr) {
    return;
  }
  if (!self->repaint_posted_.exchange(true) &&
      !PostMessageW(self->hwnd_, kRepaintMessage, 0, 0)) {
    self->repaint_posted_.store(false);
  }
}

void CALLBACK Win32TextOverlayWindow::OnLocationChange(HWINEVENTHOOK, DWORD,
                                                       HWND hwnd, LONG object,
                                                       LONG child, DWORD,
                                                       DWORD) {
  Win32TextOverlayWindow* self = g_hooked;
  if (self == nullptr || hwnd != self->owner_ || object != OBJID_WINDOW ||
      child != CHILDID_SELF) {
    return;
  }
  self->Present();
}

void Win32TextOverlayWindow::Present() {
  if (hwnd_ == nullptr || overlay_ == nullptr) {
    return;
  }
  MocktailTextOverlayFrameInfo frame;
  if (!overlay_->QueryFrame(&frame) || frame.visible == 0 || frame.width == 0 ||
      frame.height == 0 || frame.coordinate_width == 0 ||
      frame.coordinate_height == 0 ||
      frame.rgba_bytes < static_cast<std::uint64_t>(frame.width) * frame.height * 4) {
    Hide();
    return;
  }
  RECT client{};
  POINT origin{0, 0};
  if (!IsWindowVisible(owner_) || IsIconic(owner_) ||
      !GetClientRect(owner_, &client) || client.right <= 0 ||
      client.bottom <= 0 || !ClientToScreen(owner_, &origin) ||
      !EnsureBitmap(static_cast<int>(frame.width),
                    static_cast<int>(frame.height))) {
    Hide();
    return;
  }
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(frame.rgba_bytes));
  if (!overlay_->CopyFrame(frame.revision, rgba.data(), rgba.size())) {
    SecureZeroMemory(rgba.data(), rgba.size());
    return;
  }
  auto* destination = static_cast<std::uint8_t*>(bits_);
  const std::size_t pixels =
      static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
  std::uint64_t opaque = 0;
  for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
    const std::uint8_t* source = rgba.data() + pixel * 4;
    std::uint8_t* target = destination + pixel * 4;
    const std::uint32_t alpha = source[3];
    target[0] = static_cast<std::uint8_t>((source[2] * alpha + 127U) / 255U);
    target[1] = static_cast<std::uint8_t>((source[1] * alpha + 127U) / 255U);
    target[2] = static_cast<std::uint8_t>((source[0] * alpha + 127U) / 255U);
    target[3] = static_cast<std::uint8_t>(alpha);
    opaque += alpha != 0 ? 1 : 0;
  }
  SecureZeroMemory(rgba.data(), rgba.size());

  const double scale_x =
      static_cast<double>(client.right) / static_cast<double>(frame.coordinate_width);
  const double scale_y =
      static_cast<double>(client.bottom) / static_cast<double>(frame.coordinate_height);
  const LONG left = origin.x + static_cast<LONG>(std::lround(frame.x * scale_x));
  const LONG top = origin.y + static_cast<LONG>(std::lround(frame.y * scale_y));
  const RECT box{left, top, left + static_cast<LONG>(frame.width),
                 top + static_cast<LONG>(frame.height)};
  const RECT area{origin.x, origin.y, origin.x + client.right,
                  origin.y + client.bottom};
  RECT visible{};
  if (!IntersectRect(&visible, &box, &area)) {
    Hide();
    return;
  }
  POINT position{visible.left, visible.top};
  SIZE size{visible.right - visible.left, visible.bottom - visible.top};
  POINT source{visible.left - box.left, visible.top - box.top};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  if (!UpdateLayeredWindow(hwnd_, nullptr, &position, &size, bitmap_dc_,
                           &source, 0, &blend, ULW_ALPHA)) {
    Hide();
    return;
  }
  if (!shown_) {
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    shown_ = true;
    if (stats_.shows < kLoggedShows) {
      std::fprintf(stderr,
                   "  [input] TextBox overlay shown at %ld,%ld %ldx%ld in a %ldx%ld client area (%s)\n",
                   static_cast<long>(visible.left - origin.x), static_cast<long>(visible.top - origin.y),
                   static_cast<long>(size.cx), static_cast<long>(size.cy), static_cast<long>(client.right),
                   static_cast<long>(client.bottom),
                   (GetWindowLongPtrW(owner_, GWL_STYLE) & WS_CAPTION) != 0 ? "windowed" : "fullscreen");
    }
    ++stats_.shows;
  }
  RaiseAboveOwner();
  stats_.visible = true;
  stats_.screen_rect = visible;
  stats_.revision = frame.revision;
  stats_.opaque_pixels = opaque;
  ++stats_.presents;
}

void Win32TextOverlayWindow::RaiseAboveOwner() {
  for (HWND above = GetWindow(owner_, GW_HWNDPREV); above != nullptr; above = GetWindow(above, GW_HWNDPREV)) {
    if (above == hwnd_) {
      return;
    }
  }
  const HWND above_owner = GetWindow(owner_, GW_HWNDPREV);
  if (SetWindowPos(hwnd_, above_owner != nullptr ? above_owner : HWND_TOP, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER)) {
    ++stats_.raises;
  }
}

void Win32TextOverlayWindow::Hide() {
  if (bits_ != nullptr) {
    SecureZeroMemory(bits_, static_cast<std::size_t>(bitmap_width_) *
                                static_cast<std::size_t>(bitmap_height_) * 4);
  }
  if (shown_ && hwnd_ != nullptr) {
    if (bits_ != nullptr) {
      SIZE size{bitmap_width_, bitmap_height_};
      POINT source{0, 0};
      BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
      UpdateLayeredWindow(hwnd_, nullptr, nullptr, &size, bitmap_dc_, &source,
                          0, &blend, ULW_ALPHA);
    }
    ShowWindow(hwnd_, SW_HIDE);
  }
  shown_ = false;
  stats_.visible = false;
}

bool Win32TextOverlayWindow::EnsureBitmap(int width, int height) {
  if (bits_ != nullptr && width == bitmap_width_ && height == bitmap_height_) {
    return true;
  }
  ReleaseBitmap();
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  bitmap_dc_ = CreateCompatibleDC(nullptr);
  if (bitmap_dc_ == nullptr) {
    return false;
  }
  void* bits = nullptr;
  bitmap_ = CreateDIBSection(bitmap_dc_, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bitmap_ == nullptr || bits == nullptr) {
    ReleaseBitmap();
    return false;
  }
  previous_bitmap_ = SelectObject(bitmap_dc_, bitmap_);
  bits_ = bits;
  bitmap_width_ = width;
  bitmap_height_ = height;
  return true;
}

void Win32TextOverlayWindow::ReleaseBitmap() {
  if (bits_ != nullptr) {
    SecureZeroMemory(bits_, static_cast<std::size_t>(bitmap_width_) *
                                static_cast<std::size_t>(bitmap_height_) * 4);
  }
  if (bitmap_dc_ != nullptr && previous_bitmap_ != nullptr) {
    SelectObject(bitmap_dc_, previous_bitmap_);
  }
  if (bitmap_ != nullptr) {
    DeleteObject(bitmap_);
  }
  if (bitmap_dc_ != nullptr) {
    DeleteDC(bitmap_dc_);
  }
  bitmap_dc_ = nullptr;
  bitmap_ = nullptr;
  previous_bitmap_ = nullptr;
  bits_ = nullptr;
  bitmap_width_ = 0;
  bitmap_height_ = 0;
}

}
