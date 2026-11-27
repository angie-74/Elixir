// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "ui/launch_progress.h"

#include <dwmapi.h>
#include <shobjidl.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/diagnostics.h"
#include "core/i18n.h"
#include "resource.h"
#include "ui/theme.h"

namespace elixir::launcher::ui {

namespace {

constexpr wchar_t kWindowClass[] = L"ElixirLaunchProgress";
constexpr wchar_t kCancelId[] = L"progress.cancel";
constexpr UINT_PTR kFrameTimer = 1;
constexpr UINT kFrameIntervalMs = 16;
constexpr DWORD kStyle = WS_OVERLAPPED | WS_CAPTION;
constexpr DWORD kExStyle = WS_EX_APPWINDOW;

HICON AppIcon(int size) {
  return static_cast<HICON>(
      LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ELIXIR), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
}

double NowMs() {
  LARGE_INTEGER frequency{};
  LARGE_INTEGER counter{};
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&counter);
  return static_cast<double>(counter.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart);
}

}

LaunchProgress::~LaunchProgress() {
  on_cancel_ = nullptr;
  if (hwnd_ != nullptr) DestroyWindow(hwnd_);
}

bool LaunchProgress::Show(View view, std::function<void()> on_cancel, bool offscreen) {
  view_ = std::move(view);
  on_cancel_ = std::move(on_cancel);
  cancelled_ = false;
  if (hwnd_ != nullptr) {
    InvalidateRect(hwnd_, nullptr, FALSE);
    ShowTaskbarProgress();
    return true;
  }
  if (!renderer_ready_) {
    renderer_ok_ = renderer_.Initialize();
    renderer_ready_ = true;
  }
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &LaunchProgress::WindowProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hIcon = AppIcon(GetSystemMetrics(SM_CXICON));
    window_class.hIconSm = AppIcon(GetSystemMetrics(SM_CXSMICON));
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    registered = true;
  }
  POINT cursor{};
  GetCursorPos(&cursor);
  const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  if (!GetMonitorInfoW(monitor, &info)) SystemParametersInfoW(SPI_GETWORKAREA, 0, &info.rcWork, 0);
  hwnd_ = CreateWindowExW(kExStyle, kWindowClass, L"Elixir", kStyle, info.rcWork.left, info.rcWork.top, 100, 100,
                          nullptr, nullptr, GetModuleHandleW(nullptr), this);
  if (hwnd_ == nullptr) return false;
  dpi_ = GetDpiForWindow(hwnd_);
  if (dpi_ == 0) dpi_ = 96;
  const int width = static_cast<int>(std::lround(kLaunchProgressWidth * static_cast<float>(dpi_) / 96.0f));
  const int height = static_cast<int>(std::lround(kLaunchProgressHeight * static_cast<float>(dpi_) / 96.0f));
  int x = info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - width) / 2;
  int y = info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - height) / 2;
  if (offscreen) {
    x = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) + 64;
    y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  }
  ApplyBackdrop(!offscreen);
  SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  opened_at_ = GetTickCount64();
  SetTimer(hwnd_, kFrameTimer, kFrameIntervalMs, nullptr);
  ShowWindow(hwnd_, offscreen ? SW_SHOWNOACTIVATE : SW_SHOWNORMAL);
  if (!offscreen) SetForegroundWindow(hwnd_);
  UpdateWindow(hwnd_);
  ShowTaskbarProgress();
  return true;
}

void LaunchProgress::Update(View view) {
  view_ = std::move(view);
  if (hwnd_ == nullptr) return;
  InvalidateRect(hwnd_, nullptr, FALSE);
  ShowTaskbarProgress();
}

void LaunchProgress::Close() {
  on_cancel_ = nullptr;
  if (hwnd_ != nullptr) DestroyWindow(hwnd_);
}

RECT LaunchProgress::cancel_rect() const {
  const float scale = static_cast<float>(dpi_) / 96.0f;
  return RECT{static_cast<LONG>(std::lround(cancel_.left * scale)), static_cast<LONG>(std::lround(cancel_.top * scale)),
              static_cast<LONG>(std::lround(cancel_.right * scale)),
              static_cast<LONG>(std::lround(cancel_.bottom * scale))};
}

LRESULT CALLBACK LaunchProgress::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  LaunchProgress* self = nullptr;
  if (message == WM_NCCREATE) {
    self = static_cast<LaunchProgress*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    self->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<LaunchProgress*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (self == nullptr) return DefWindowProcW(hwnd, message, wparam, lparam);
  const LRESULT result = self->OnMessage(message, wparam, lparam);
  if (message == WM_NCDESTROY) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    self->hwnd_ = nullptr;
    self->renderer_.DiscardTarget();
    self->state_ = UiState{};
    self->tracking_mouse_ = false;
  }
  return result;
}

LRESULT LaunchProgress::OnMessage(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_NCCALCSIZE:
      if (wparam != 0) return 0;
      break;
    case WM_NCHITTEST: {
      POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      ScreenToClient(hwnd_, &point);
      return OverCancel(point) ? HTCLIENT : HTCAPTION;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT:
      Paint();
      return 0;
    case WM_TIMER:
      if (wparam == kFrameTimer && !IsIconic(hwnd_)) InvalidateRect(hwnd_, nullptr, FALSE);
      return 0;
    case WM_SIZE:
      renderer_.Resize(LOWORD(lparam), HIWORD(lparam));
      return 0;
    case WM_DPICHANGED: {
      dpi_ = HIWORD(wparam) != 0 ? HIWORD(wparam) : 96;
      renderer_.SetDpi(dpi_);
      const auto* suggested = reinterpret_cast<const RECT*>(lparam);
      const int width = static_cast<int>(std::lround(kLaunchProgressWidth * static_cast<float>(dpi_) / 96.0f));
      const int height = static_cast<int>(std::lround(kLaunchProgressHeight * static_cast<float>(dpi_) / 96.0f));
      SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
      return 0;
    }
    case WM_MOUSEMOVE: {
      const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      const std::wstring hovered = OverCancel(point) ? std::wstring(kCancelId) : std::wstring();
      if (hovered != state_.hovered) {
        state_.hovered = hovered;
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      if (!tracking_mouse_) {
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd_, 0};
        tracking_mouse_ = TrackMouseEvent(&track) != FALSE;
      }
      return 0;
    }
    case WM_NCMOUSEMOVE:
    case WM_MOUSELEAVE:
      if (message == WM_MOUSELEAVE) tracking_mouse_ = false;
      if (!state_.hovered.empty() && GetCapture() != hwnd_) {
        state_.hovered.clear();
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      break;
    case WM_LBUTTONDOWN:
      if (view_.cancel_enabled && OverCancel(POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
        state_.pressed = kCancelId;
        state_.hovered = kCancelId;
        SetCapture(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      return 0;
    case WM_LBUTTONUP: {
      const bool was_pressed = !state_.pressed.empty();
      state_.pressed.clear();
      if (GetCapture() == hwnd_) ReleaseCapture();
      InvalidateRect(hwnd_, nullptr, FALSE);
      if (was_pressed && view_.cancel_enabled && OverCancel(POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
        Cancel();
      }
      return 0;
    }
    case WM_CAPTURECHANGED:
      state_.pressed.clear();
      return 0;
    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE && view_.cancel_enabled) {
        Cancel();
        return 0;
      }
      break;
    case WM_CLOSE:
      if (view_.cancel_enabled) Cancel();
      return 0;
    case WM_DESTROY:
      KillTimer(hwnd_, kFrameTimer);
      break;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void LaunchProgress::ApplyBackdrop(bool system_backdrop) {
  BOOL dark = TRUE;
  if (FAILED(DwmSetWindowAttribute(hwnd_, 20 , &dark, sizeof(dark)))) {
    DwmSetWindowAttribute(hwnd_, 19 , &dark, sizeof(dark));
  }
  HIGHCONTRASTW contrast{};
  contrast.cbSize = sizeof(contrast);
  SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
  backdrop_ = false;
  if (system_backdrop && (contrast.dwFlags & HCF_HIGHCONTRASTON) == 0) {
    const DWORD build = CollectWindowsInfo().build;
    if (build >= 22621) {
      const int mica = 2;
      backdrop_ = SUCCEEDED(DwmSetWindowAttribute(hwnd_, 38, &mica, sizeof(mica)));
    } else if (build >= 22000) {
      const BOOL mica = TRUE;
      backdrop_ = SUCCEEDED(DwmSetWindowAttribute(hwnd_, 1029, &mica, sizeof(mica)));
    }
  }
  if (backdrop_) {
    const MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);
  }
  const int corners = 2;
  DwmSetWindowAttribute(hwnd_, 33 , &corners, sizeof(corners));
}

void LaunchProgress::Paint() {
  PAINTSTRUCT paint{};
  BeginPaint(hwnd_, &paint);
  if (renderer_ok_ && renderer_.EnsureTarget(hwnd_, dpi_)) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const float scale = static_cast<float>(dpi_) / 96.0f;
    state_.animation = static_cast<float>(GetTickCount64() - opened_at_) / 1000.0f;
    animator_.BeginFrame(NowMs(), true);
    state_.animator = &animator_;
    ID2D1RenderTarget* target = renderer_.target();
    target->BeginDraw();
    target->SetTransform(D2D1::Matrix3x2F::Identity());
    target->Clear(backdrop_ ? D2D1::ColorF(0, 0, 0, 0) : theme::kWindow);
    records_.clear();
    Ui ui(&renderer_, &state_, &records_, nullptr);
    Build(ui, static_cast<float>(client.right) / scale, static_cast<float>(client.bottom) / scale);
    ui.Flush();
    if (target->EndDraw() == D2DERR_RECREATE_TARGET) {
      renderer_.DiscardTarget();
      InvalidateRect(hwnd_, nullptr, FALSE);
    }
    state_.animator = nullptr;
    animator_.EndFrame();
  }
  EndPaint(hwnd_, &paint);
}

void LaunchProgress::Build(Ui& ui, float width, float height) {
  if (backdrop_) ui.Fill(D2D1::RectF(0.0f, 0.0f, width, height), theme::kMicaTint);
  if (!view_.version.empty()) {
    const D2D1_RECT_F corner = D2D1::RectF(width / 2.0f, 5.0f, width - 5.0f, 19.0f);
    ui.TextLine(corner, view_.version, TextStyle::kMicro, theme::kTextSecondary, Align::kTrailing);
    ui.Label(corner, view_.version);
  }
  const float left = 32.0f;
  const float right = width - 32.0f;
  const float center = width / 2.0f;
  ui.AppIcon(D2D1::RectF(center - 40.0f, 46.0f, center + 40.0f, 126.0f));

  const float button_height = 32.0f;
  const float button_top = height - 16.0f - button_height;
  const float bar_top = button_top - 16.0f - 4.0f;
  const float message_height = ui.TextHeight(view_.message, TextStyle::kStatus, right - left);
  const float message_top = bar_top - 8.0f - message_height;
  ui.Text(left, message_top, right - left, view_.message, TextStyle::kStatus, theme::kText, Align::kCenter);
  ui.Label(D2D1::RectF(left, message_top, right, message_top + message_height), view_.message);
  ui.Progress(D2D1::RectF(left, bar_top, right, bar_top + 4.0f), view_.progress);
  cancel_ = D2D1::RectF(center - 60.0f, button_top, center + 60.0f, button_top + button_height);
  ui.Button(kCancelId, cancel_, T(L"Cancel"), ButtonStyle::kStandard, view_.cancel_enabled);
}

bool LaunchProgress::OverCancel(POINT client) const {
  const RECT button = cancel_rect();
  return PtInRect(&button, client) != FALSE;
}

void LaunchProgress::Cancel() {
  if (cancelled_) return;
  cancelled_ = true;
  const std::function<void()> on_cancel = std::move(on_cancel_);
  on_cancel_ = nullptr;
  if (on_cancel) on_cancel();
}

void LaunchProgress::ShowTaskbarProgress() {
  if (hwnd_ == nullptr) return;
  ITaskbarList3* taskbar = nullptr;
  if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar))) ||
      taskbar == nullptr) {
    return;
  }
  if (SUCCEEDED(taskbar->HrInit())) {
    if (view_.progress < 0.0f) {
      taskbar->SetProgressState(hwnd_, TBPF_INDETERMINATE);
    } else {
      taskbar->SetProgressState(hwnd_, TBPF_NORMAL);
      taskbar->SetProgressValue(hwnd_, static_cast<ULONGLONG>(std::clamp(view_.progress, 0.0f, 1.0f) * 1000.0f), 1000);
    }
  }
  taskbar->Release();
}

}
