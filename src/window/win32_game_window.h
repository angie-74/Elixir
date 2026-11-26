// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_WINDOW_WIN32_GAME_WINDOW_H_
#define ELIXIR_WINDOW_WIN32_GAME_WINDOW_H_

#include <windows.h>

#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "mocktail/platform/platform_runtime.h"
#include "mocktail/status.h"
#include "window/input_pump_pacer.h"
#include "window/main_thread_command_gate.h"
#include "window/platform_event_observer.h"
#include "window/present_observer.h"
#include "window/roblox_fullscreen_menu_request_gate.h"
#include "window/vulkan_surface_recovery_gate.h"
#include "window/win32_text_input.h"
#include "window/window.h"
#include "window/window_fullscreen_request_gate.h"
#include "window/window_fullscreen_state_sync.h"
#include "window/window_pointer_capture_owner.h"
#include "window/window_resize_readiness_gate.h"
#include "window/window_state_store.h"
#include "window/window_surface_lifecycle.h"
#include "window/window_text_input_owner.h"

namespace elixir::fflags {
class FlagSet;
}

namespace elixir::window {

enum class WindowMode { kWindowed, kBorderless, kFullscreen };
bool ParseWindowMode(std::string_view text, WindowMode* mode);
const char* WindowModeName(WindowMode mode);

bool AltEnterFullscreenFromFlags(const elixir::fflags::FlagSet& effective);

struct CursorOps {
  std::function<bool(const RECT* screen_rect)> clip_cursor;
  std::function<bool(int screen_x, int screen_y)> set_cursor_pos;
};

struct RawAbsoluteMotion {
  bool has_last = false;
  int last_x = 0;
  int last_y = 0;
};

bool RawAbsoluteToRelative(RawAbsoluteMotion* state, LONG raw_x, LONG raw_y, int width,
                           int height, bool remote_session, float* delta_x, float* delta_y);

struct GameWindowOptions {
  int width = 1280;
  int height = 720;
  WindowMode mode = WindowMode::kBorderless;
  std::wstring title = L"Elixir";
  bool start_hidden = true;
  bool offscreen = false;
  bool raw_mouse = true;
  bool alt_enter_fullscreen = true;
  bool f11_fullscreen = true;
  bool confine_cursor = true;
  CursorOps cursor_ops;
  bool raw_input_background = false;
  bool pump_wake_on_input = false;
  std::uint64_t pump_interval_ns = mocktail::window::kProductionInputPumpIntervalNs;
  std::uint32_t raw_input_count_tag = 0;
  mocktail::window::WindowResizeReadinessConfig resize_readiness{};
  std::filesystem::path state_path;
};

enum class HostWindowEventType {
  kResized,
  kMinimized,
  kRestored,
  kFocusGained,
  kFocusLost,
  kDpiChanged,
  kFullscreenChanged,
  kShown,
  kCloseRequested,
};
const char* HostWindowEventTypeName(HostWindowEventType type);

struct HostWindowEvent {
  HostWindowEventType type = HostWindowEventType::kResized;
  std::uint64_t timestamp_ns = 0;
  std::uint32_t pixel_width = 0;
  std::uint32_t pixel_height = 0;
  float dpi_scale = 1.0F;
  bool fullscreen = false;
};

struct MicrophoneMenuItem {
  std::wstring label;
  std::wstring endpoint_id;
  bool enabled = false;
  bool selected = false;
};

struct GameWindowSnapshot {
  HWND hwnd = nullptr;
  std::uint32_t pixel_width = 0;
  std::uint32_t pixel_height = 0;
  std::uint32_t dpi = USER_DEFAULT_SCREEN_DPI;
  float dpi_scale = 1.0F;
  WindowMode mode = WindowMode::kWindowed;
  bool fullscreen = false;
  bool visible = false;
  bool minimized = false;
  bool focused = false;
  bool presented = false;
  bool pointer_captured = false;
  bool cursor_visible = true;
  bool text_input_active = false;
};

struct InputStats {
  std::uint64_t pumps = 0;
  std::uint64_t messages = 0;
  std::uint64_t raw_mouse_packets = 0;
  std::uint64_t raw_mouse_injected_packets = 0;
  std::uint64_t raw_mouse_tagged_packets = 0;
  std::uint64_t motion_events = 0;
  std::uint64_t key_events = 0;
  std::uint64_t text_events = 0;
  std::uint64_t pace_sleeps = 0;
  std::uint64_t pace_sleep_ns = 0;
};

class GameWindow;

class Win32PointerCaptureBackend final
    : public mocktail::window::PointerCaptureBackend {
 public:
  explicit Win32PointerCaptureBackend(GameWindow* window) : window_(window) {}
  bool Apply(const mocktail::window::PointerCaptureRequest& request) override;

  bool relative_mode() const { return relative_mode_; }
  const std::optional<mocktail::window::PointerPosition>& drag_start() const { return drag_start_; }
  bool cursor_hidden() const { return cursor_hidden_; }
  bool clip_applied() const { return clip_applied_; }
  void Reassert();
  void OnWindowFocusGained();

 private:
  bool Clip(const RECT* screen_rect);
  bool WarpCursor(int screen_x, int screen_y);

  GameWindow* window_ = nullptr;
  std::optional<mocktail::window::PointerPosition> drag_start_;
  bool relative_mode_ = false;
  bool cursor_hidden_ = false;
  bool clip_applied_ = false;
};

class GameWindow final {
 public:
  GameWindow();
  ~GameWindow();
  GameWindow(const GameWindow&) = delete;
  GameWindow& operator=(const GameWindow&) = delete;

  mocktail::Status Create(const GameWindowOptions& options);
  void Destroy();
  bool created() const { return hwnd_ != nullptr; }

  bool Pump();
  std::uint64_t PaceInputPump();

  HWND hwnd() const { return hwnd_; }
  HWND hwnd_any_thread() const { return wake_hwnd_.load(std::memory_order_acquire); }
  GameWindowSnapshot Snapshot() const;
  mocktail::window::WindowViewportSnapshot Viewport() const;
  mocktail::platform::SurfaceCoordinateTransform CoordinateTransform() const;
  bool PollHostEvent(HostWindowEvent* event);
  bool PollSurfaceEvent(mocktail::window::WindowSurfaceEvent* event);
  mocktail::window::WindowSurfaceSnapshot SurfaceSnapshot() const;

  bool SetMicrophoneMenu(std::function<std::vector<MicrophoneMenuItem>()> refresh,
                         std::function<void(std::wstring_view)> select);

  void NoteVulkanPresent();
  void NoteVulkanSurfaceOutOfDate();
  bool HasPresentedFrame() const;
  std::uint64_t present_count() const { return present_count_.load(); }
  void ShowIfHidden();
  int PendingShowCommand() const;
  bool SetPresentObserver(mocktail::window::PresentObserver observer, void* context);
  void ClearPresentObserver();
  mocktail::Status RecordResizeReadinessSurfaceCommit(
      const mocktail::window::WindowSurfaceEvent& event);
  mocktail::Status StopResizeReadiness();
  mocktail::Status ResizeReadinessCompletionStatus() const;

  bool SetFullscreen(bool fullscreen, const char* reason);
  bool ToggleFullscreen(const char* reason);
  bool fullscreen() const { return fullscreen_; }
  bool RequestFullscreenFromAndroidWindowFlags(int flags, int mask);
  bool RequestFullscreenFromRobloxMenuLog(const char* tag,
                                          const char* message);
  bool SetFullscreenStateSyncCallback(mocktail::window::FullscreenStateSyncCallback callback,
                                      void* context);
  void ClearFullscreenStateSyncCallback();
  void set_alt_enter_fullscreen(bool enabled) { options_.alt_enter_fullscreen = enabled; }

  bool SetPlatformEventObserver(mocktail::window::PlatformEventObserver observer,
                                void* context);
  void ClearPlatformEventObserver();
  bool SetMouseLockQueryCallback(mocktail::window::MouseLockQueryCallback callback,
                                 void* context);
  void ClearMouseLockQueryCallback();
  bool SetPreTextInputPumpCallback(mocktail::window::MainThreadCommandCallback callback,
                                   void* context);
  void ClearPreTextInputPumpCallback();
  bool SetPumpHook(mocktail::window::MainThreadCommandCallback callback, void* context);
  void ClearPumpHook();
  void SetTextInputOwnerEnabled(bool enabled);
  bool RequestShowTextInput(std::uint64_t generation,
                            const mocktail::window::TextInputArea& area,
                            const mocktail::window::TextInputOptions& options);
  bool RequestHideTextInput(std::uint64_t generation);
  std::uint64_t device_change_count() const { return device_changes_.load(); }
  InputStats input_stats() const;
  mocktail::window::WindowPointerCaptureOwner& pointer_capture() { return *pointer_owner_; }
  Win32PointerCaptureBackend& pointer_backend() { return *pointer_backend_; }
  Win32TextInputBackend& text_backend() { return text_backend_; }

  static std::uint64_t MonotonicNs();

 private:
  friend class Win32PointerCaptureBackend;

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                  LPARAM lparam) noexcept;
  LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

  void Emit(mocktail::platform::PlatformEventPayload payload);
  void EmitMotion(const mocktail::platform::MouseMotionEvent& motion);
  mocktail::window::PointerPosition RelativePin() const;
  void FlushMotion();
  void PushHostEvent(HostWindowEventType type);
  bool AttachMicrophoneSystemMenu();
  void RefreshMicrophoneMenu();
  bool InvokeMicrophoneMenuCommand(UINT command);

  LRESULT OnKey(UINT message, WPARAM wparam, LPARAM lparam);
  void OnRawInput(LPARAM lparam);
  void OnMouseMove(LPARAM lparam);
  void OnMouseButton(std::uint8_t sdl_button, bool pressed, LPARAM lparam);
  void OnMouseWheel(bool horizontal, WPARAM wparam, LPARAM lparam);
  void OnFocus(bool focused);
  void OnSize(WPARAM wparam);
  void OnDpiChanged(WPARAM wparam, const RECT* suggested);
  void ApplyIcons();
  void OnImeComposition(LPARAM lparam);
  void OnChar(UINT message, WPARAM wparam);

  bool SetRawMouseRegistered(bool registered);
  bool ClipCursorScreen(const RECT* screen_rect);
  bool WarpCursorScreen(int screen_x, int screen_y);
  void RefreshClientSize();
  void PublishGeometry();
  void ReleaseDeliveredKeys();
  void ObserveSurface();
  void MaybeEmitResized(bool force);
  void FitToMonitor();
  bool ApplyFullscreen(bool fullscreen);
  void MaybeApplyFullscreenRequests();
  void MaybeRequestResizeReadiness();
  void MaybeRecoverVulkanSurface();
  void CaptureWindowState();
  void PersistWindowState();
  bool TextInputActive() const;
  void ShowNow();
  std::uint32_t ButtonMask() const;
  POINT ClientCenter() const;

  GameWindowOptions options_;
  HWND hwnd_ = nullptr;
  HMENU microphone_menu_ = nullptr;
  bool microphone_menu_attached_ = false;
  std::function<std::vector<MicrophoneMenuItem>()> refresh_microphones_;
  std::function<void(std::wstring_view)> select_microphone_;
  std::vector<MicrophoneMenuItem> microphone_items_;
  DWORD owner_thread_id_ = 0;
  std::atomic<HWND> wake_hwnd_{nullptr};
  bool fullscreen_ = false;
  WINDOWPLACEMENT windowed_placement_{};
  bool has_windowed_placement_ = false;
  bool restore_maximized_pending_ = false;
  std::uint32_t pixel_width_ = 0;
  std::uint32_t pixel_height_ = 0;
  UINT dpi_ = USER_DEFAULT_SCREEN_DPI;
  HICON icon_big_ = nullptr;
  HICON icon_small_ = nullptr;
  std::atomic<std::uint64_t> geometry_{0};
  bool visible_ = false;
  bool minimized_ = false;
  bool focused_ = false;
  bool quit_requested_ = false;
  bool in_pump_ = false;

  std::uint32_t emitted_width_ = 0;
  std::uint32_t emitted_height_ = 0;
  float emitted_scale_ = 0.0F;

  bool has_cursor_position_ = false;
  float cursor_x_ = 0.0F;
  float cursor_y_ = 0.0F;
  std::uint32_t buttons_down_ = 0;
  RawAbsoluteMotion raw_absolute_{};
  std::vector<BYTE> raw_buffer_;
  mocktail::platform::PlatformEvent pending_motion_{};
  bool has_pending_motion_ = false;

  std::bitset<512> keys_down_;
  std::bitset<512> keys_delivered_;
  std::array<std::uint32_t, 512> delivered_keycodes_{};
  bool alt_enter_consumed_ = false;
  bool f11_consumed_ = false;

  std::atomic<std::uint64_t> present_count_{0};
  std::atomic<bool> show_requested_{false};

  std::atomic<std::uint64_t> device_changes_{0};
  InputStats stats_{};
  HANDLE pace_timer_ = nullptr;

  mutable std::mutex host_events_mutex_;
  std::deque<HostWindowEvent> host_events_;

  mocktail::window::WindowSurfaceLifecycle surface_;
  mocktail::window::VulkanSurfaceRecoveryGate surface_recovery_;
  mocktail::window::PresentLifecycleGate present_lifecycle_;
  mocktail::window::PlatformEventObserverGate observer_;
  mocktail::window::MainThreadCommandGate pre_text_input_gate_;
  mocktail::window::MainThreadCommandGate pump_hook_gate_;
  mocktail::window::WindowFullscreenRequestGate fullscreen_request_;
  mocktail::window::RobloxFullscreenMenuRequestGate fullscreen_menu_request_;
  mocktail::window::WindowFullscreenStateSync fullscreen_sync_;
  std::unique_ptr<mocktail::window::WindowResizeReadinessGate> resize_readiness_;
  std::optional<mocktail::window::InputPumpPacer> pacer_;
  bool initial_fullscreen_sync_pending_ = false;
  bool raw_input_registered_ = false;
  std::unique_ptr<Win32PointerCaptureBackend> pointer_backend_;
  std::unique_ptr<mocktail::window::WindowPointerCaptureOwner> pointer_owner_;
  Win32TextInputBackend text_backend_;
  std::unique_ptr<mocktail::window::WindowTextInputOwner> text_owner_;

  mocktail::window::PersistedWindowState persisted_{};
  bool persistence_active_ = false;
  bool persistence_dirty_ = false;
  std::uint64_t persistence_change_ns_ = 0;
};

GameWindow* CurrentGameWindow();
void SetCurrentGameWindow(GameWindow* window);
void RetireCurrentGameWindow(GameWindow* window);

class CurrentGameWindowRef final {
 public:
  CurrentGameWindowRef();
  ~CurrentGameWindowRef();
  CurrentGameWindowRef(const CurrentGameWindowRef&) = delete;
  CurrentGameWindowRef& operator=(const CurrentGameWindowRef&) = delete;

  GameWindow* get() const { return window_; }
  GameWindow* operator->() const { return window_; }
  explicit operator bool() const { return window_ != nullptr; }

 private:
  GameWindow* window_ = nullptr;
};

std::size_t CurrentGameWindowReferenceCount(const GameWindow* window);

}

#endif
