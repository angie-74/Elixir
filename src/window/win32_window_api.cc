// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "window/win32_game_window.h"
#include "window/window.h"

namespace mocktail {
namespace window {

namespace {

using elixir::window::CurrentGameWindowRef;
using elixir::window::GameWindow;

std::filesystem::path g_state_path;
std::unique_ptr<GameWindow> g_owned_window;

std::wstring Utf8ToWide(const char* text) {
  if (text == nullptr || *text == '\0') {
    return L"Elixir";
  }
  const int units = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (units <= 1) {
    return L"Elixir";
  }
  std::wstring wide(static_cast<std::size_t>(units), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), units);
  wide.resize(static_cast<std::size_t>(units - 1));
  return wide;
}

// Mocktail's platform::NormalizeDisplayRefreshCapabilities
// (src/platform/sdl_display_refresh_capabilities.cc, Apache-2.0).
platform::DisplayRefreshCapabilities NormalizeRates(float current_hz,
                                                    std::vector<float> supported_hz) {
  constexpr float kDuplicateToleranceHz = 0.01F;
  const auto usable = [](float rate_hz) { return std::isfinite(rate_hz) && rate_hz > 0.0F; };
  platform::DisplayRefreshCapabilities result;
  if (!usable(current_hz)) {
    return result;
  }
  result.current_hz = current_hz;
  supported_hz.erase(std::remove_if(supported_hz.begin(), supported_hz.end(),
                                    [&](float rate_hz) { return !usable(rate_hz); }),
                     supported_hz.end());
  supported_hz.push_back(current_hz);
  std::sort(supported_hz.begin(), supported_hz.end());
  for (const float rate_hz : supported_hz) {
    if (result.supported_hz.empty() ||
        std::fabs(result.supported_hz.back() - rate_hz) > kDuplicateToleranceHz) {
      result.supported_hz.push_back(rate_hz);
    }
  }
  return result;
}

}

Status ConfigureWindowStatePersistence(const std::filesystem::path& path) {
  if (CurrentGameWindowRef window; window) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "window state persistence must be configured before Init");
  }
  if (!path.empty() && !path.is_absolute()) {
    return Status::Error(StatusCode::kInvalidArgument, "window state path must be absolute");
  }
  g_state_path = path.empty() ? path : path.lexically_normal();
  return Status::Ok();
}

bool Init(int width, int height, const char* title) {
  if (CurrentGameWindowRef window; window) {
    return true;
  }
  elixir::window::GameWindowOptions options;
  options.width = width;
  options.height = height;
  options.title = Utf8ToWide(title);
  options.state_path = g_state_path;
  auto window = std::make_unique<GameWindow>();
  const Status status = window->Create(options);
  if (!status.ok()) {
    std::fprintf(stderr, "  [window] creation failed: %s\n", status.message().c_str());
    return false;
  }
  elixir::window::SetCurrentGameWindow(window.get());
  g_owned_window = std::move(window);
  return true;
}

bool PumpEvents() {
  CurrentGameWindowRef window;
  return window && window->Pump();
}

uint64_t PaceInputPump() {
  CurrentGameWindowRef window;
  return window ? window->PaceInputPump() : 0;
}

bool RequestFullscreenFromAndroidWindowFlags(int flags, int mask) {
  CurrentGameWindowRef window;
  return window && window->RequestFullscreenFromAndroidWindowFlags(flags, mask);
}

bool RequestFullscreenFromRobloxMenuLog(const char* tag, const char* message) {
  CurrentGameWindowRef window;
  return window && window->RequestFullscreenFromRobloxMenuLog(tag, message);
}

bool SetFullscreenStateSyncCallback(FullscreenStateSyncCallback callback, void* context) {
  CurrentGameWindowRef window;
  return window && window->SetFullscreenStateSyncCallback(callback, context);
}

void ClearFullscreenStateSyncCallback() {
  if (CurrentGameWindowRef window; window) {
    window->ClearFullscreenStateSyncCallback();
  }
}

bool PollWindowSurfaceEvent(WindowSurfaceEvent* event) {
  CurrentGameWindowRef window;
  return window && window->PollSurfaceEvent(event);
}

WindowSurfaceSnapshot GetWindowSurfaceSnapshot() {
  CurrentGameWindowRef window;
  return window ? window->SurfaceSnapshot() : WindowSurfaceSnapshot{};
}

Status RecordResizeReadinessSurfaceCommit(const WindowSurfaceEvent& event) {
  CurrentGameWindowRef window;
  return window ? window->RecordResizeReadinessSurfaceCommit(event) : Status::Ok();
}

Status StopResizeReadiness() {
  CurrentGameWindowRef window;
  return window ? window->StopResizeReadiness() : Status::Ok();
}

Status ResizeReadinessCompletionStatus() {
  CurrentGameWindowRef window;
  return window ? window->ResizeReadinessCompletionStatus() : Status::Ok();
}

WindowViewportSnapshot GetWindowViewportSnapshot() {
  CurrentGameWindowRef window;
  return window ? window->Viewport() : WindowViewportSnapshot{};
}

platform::DisplayRefreshCapabilities GetDisplayRefreshCapabilities() {
  CurrentGameWindowRef window;
  HWND hwnd = window ? window->hwnd_any_thread() : nullptr;
  if (hwnd == nullptr) {
    return {};
  }
  MONITORINFOEXW info{};
  info.cbSize = sizeof(info);
  if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &info)) {
    return {};
  }
  DEVMODEW current{};
  current.dmSize = sizeof(current);
  if (!EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &current) ||
      current.dmDisplayFrequency <= 1) {
    return {};
  }
  std::vector<float> rates;
  DEVMODEW mode{};
  mode.dmSize = sizeof(mode);
  for (DWORD index = 0; EnumDisplaySettingsW(info.szDevice, index, &mode); ++index) {
    if (mode.dmPelsWidth == current.dmPelsWidth && mode.dmPelsHeight == current.dmPelsHeight &&
        mode.dmDisplayFrequency > 1) {
      rates.push_back(static_cast<float>(mode.dmDisplayFrequency));
    }
  }
  return NormalizeRates(static_cast<float>(current.dmDisplayFrequency), std::move(rates));
}

bool SetPlatformEventObserver(PlatformEventObserver observer, void* context) {
  CurrentGameWindowRef window;
  return window && window->SetPlatformEventObserver(observer, context);
}

void ClearPlatformEventObserver() {
  if (CurrentGameWindowRef window; window) {
    window->ClearPlatformEventObserver();
  }
}

bool SetMouseLockQueryCallback(MouseLockQueryCallback callback, void* context) {
  CurrentGameWindowRef window;
  return window && window->SetMouseLockQueryCallback(callback, context);
}

void ClearMouseLockQueryCallback() {
  if (CurrentGameWindowRef window; window) {
    window->ClearMouseLockQueryCallback();
  }
}

bool SetPreTextInputPumpCallback(PreTextInputPumpCallback callback, void* context) {
  CurrentGameWindowRef window;
  return window && window->SetPreTextInputPumpCallback(callback, context);
}

void ClearPreTextInputPumpCallback() {
  if (CurrentGameWindowRef window; window) {
    window->ClearPreTextInputPumpCallback();
  }
}

void SetWindowTextInputOwnerEnabled(bool enabled) {
  if (CurrentGameWindowRef window; window) {
    window->SetTextInputOwnerEnabled(enabled);
  }
}

bool RequestShowTextInput(uint64_t generation, const TextInputArea& area,
                          const TextInputOptions& options) {
  CurrentGameWindowRef window;
  return window && window->RequestShowTextInput(generation, area, options);
}

bool RequestHideTextInput(uint64_t generation) {
  CurrentGameWindowRef window;
  return window && window->RequestHideTextInput(generation);
}

void* GetBackendWindow() {
  CurrentGameWindowRef window;
  return window ? static_cast<void*>(window->hwnd_any_thread()) : nullptr;
}

bool UsesDirectVulkan() {
  CurrentGameWindowRef window;
  return static_cast<bool>(window);
}

bool HasPresentedFrame() {
  CurrentGameWindowRef window;
  return window && window->HasPresentedFrame();
}

void NoteVulkanPresent() {
  if (CurrentGameWindowRef window; window) {
    window->NoteVulkanPresent();
  }
}

void NoteVulkanSurfaceOutOfDate() {
  if (CurrentGameWindowRef window; window) {
    window->NoteVulkanSurfaceOutOfDate();
  }
}

bool SetPresentObserver(PresentObserver observer, void* context) {
  CurrentGameWindowRef window;
  return window && window->SetPresentObserver(observer, context);
}

void ClearPresentObserver() {
  if (CurrentGameWindowRef window; window) {
    window->ClearPresentObserver();
  }
}

ScopedPresentObserver::~ScopedPresentObserver() { Reset(); }

bool ScopedPresentObserver::Register(PresentObserver observer, void* context) {
  if (registered_ || !SetPresentObserver(observer, context)) {
    return false;
  }
  registered_ = true;
  return true;
}

void ScopedPresentObserver::Reset() {
  if (!registered_) {
    return;
  }
  ClearPresentObserver();
  registered_ = false;
}

void ShowIfHidden() {
  if (CurrentGameWindowRef window; window) {
    window->ShowIfHidden();
  }
}

void Shutdown() {
  if (g_owned_window != nullptr) {
    g_owned_window->Destroy();
    g_owned_window.reset();
  }
}

bool IsInitialised() {
  CurrentGameWindowRef window;
  return static_cast<bool>(window);
}

int GetWidth() {
  CurrentGameWindowRef window;
  return window ? window->Viewport().pixel_width : 0;
}

int GetHeight() {
  CurrentGameWindowRef window;
  return window ? window->Viewport().pixel_height : 0;
}

}
}

extern "C" void mocktail_window_note_vulkan_present() {
  mocktail::window::NoteVulkanPresent();
}

extern "C" void mocktail_window_note_vulkan_surface_out_of_date() {
  mocktail::window::NoteVulkanSurfaceOutOfDate();
}

extern "C" bool mocktail_window_has_presented_frame() {
  return mocktail::window::HasPresentedFrame();
}

extern "C" void* mocktail_window_backend_window() {
  return mocktail::window::GetBackendWindow();
}

extern "C" bool mocktail_window_uses_direct_vulkan() {
  return mocktail::window::UsesDirectVulkan();
}

extern "C" void mocktail_window_show_if_hidden() { mocktail::window::ShowIfHidden(); }

extern "C" int mocktail_window_width() {
  const auto surface = mocktail::window::GetWindowSurfaceSnapshot();
  return surface.generation != 0 ? static_cast<int>(surface.width)
                                 : mocktail::window::GetWidth();
}

extern "C" int mocktail_window_height() {
  const auto surface = mocktail::window::GetWindowSurfaceSnapshot();
  return surface.generation != 0 ? static_cast<int>(surface.height)
                                 : mocktail::window::GetHeight();
}
