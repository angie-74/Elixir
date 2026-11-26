// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Pump() follows Mocktail's window::PumpEvents (src/window/window.cc, Apache-2.0).
#include "window/win32_game_window.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <utility>

#include "fflags/fflags.h"
#include "window/win32_keyboard.h"

namespace elixir::window {

namespace mwindow = mocktail::window;
namespace platform = mocktail::platform;
using mocktail::Status;
using mocktail::StatusCode;

namespace {

constexpr wchar_t kWindowClassName[] = L"ElixirGameWindow";
constexpr COLORREF kBackgroundColor = RGB(18, 18, 18);
constexpr int kOffscreenOrigin = -20000;
constexpr std::uint64_t kPersistDelayNs = 1'000'000'000ULL;
constexpr int kMinimumVisibleRestoreExtent = 64;
constexpr int kMaximumAbsoluteJump = 32;
constexpr UINT kMicrophoneCommandFirst = 0x2000;
constexpr UINT kMicrophoneCommandStride = 16;
constexpr std::size_t kMaximumMicrophoneItems = 128;
constexpr UINT kShowFullscreenMicrophoneMenu = WM_APP + 1;

struct WindowReferences {
  const GameWindow* window = nullptr;
  std::size_t count = 0;
};

constexpr std::size_t kReferenceSlots = 8;

struct CurrentWindowState {
  std::mutex mutex;
  std::condition_variable released;
  GameWindow* window = nullptr;
  std::array<WindowReferences, kReferenceSlots> references{};
};

CurrentWindowState& State() {
  static CurrentWindowState* const state = new CurrentWindowState();
  return *state;
}

thread_local std::array<WindowReferences, kReferenceSlots> t_references{};

std::size_t* ReferenceCount(std::array<WindowReferences, kReferenceSlots>& table,
                            const GameWindow* window, bool create) {
  WindowReferences* free_slot = nullptr;
  for (WindowReferences& entry : table) {
    if (entry.window == window && entry.count != 0) {
      return &entry.count;
    }
    if (entry.count == 0 && free_slot == nullptr) {
      free_slot = &entry;
    }
  }
  if (!create || free_slot == nullptr) {
    return nullptr;
  }
  free_slot->window = window;
  return &free_slot->count;
}

std::size_t ReferencesHeld(std::array<WindowReferences, kReferenceSlots>& table,
                           const GameWindow* window) {
  const std::size_t* count = ReferenceCount(table, window, false);
  return count != nullptr ? *count : 0;
}

void WaitForOtherReferences(std::unique_lock<std::mutex>& lock, const GameWindow* window) {
  CurrentWindowState& state = State();
  state.released.wait(lock, [&state, window] {
    return ReferencesHeld(state.references, window) == ReferencesHeld(t_references, window);
  });
}

std::uint64_t PackGeometry(std::uint32_t width, std::uint32_t height, UINT dpi) {
  return (static_cast<std::uint64_t>(width) & 0xFFFFFFu) |
         ((static_cast<std::uint64_t>(height) & 0xFFFFFFu) << 24) |
         ((static_cast<std::uint64_t>(dpi) & 0xFFFFu) << 48);
}

struct Geometry {
  std::uint32_t width;
  std::uint32_t height;
  UINT dpi;
};

Geometry UnpackGeometry(std::uint64_t packed) {
  const auto dpi = static_cast<UINT>((packed >> 48) & 0xFFFFu);
  return {static_cast<std::uint32_t>(packed & 0xFFFFFFu),
          static_cast<std::uint32_t>((packed >> 24) & 0xFFFFFFu),
          dpi != 0 ? dpi : USER_DEFAULT_SCREEN_DPI};
}

bool RectIsVisibleOnAMonitor(const RECT& rect) {
  struct Search {
    RECT rect;
    bool visible;
  } search{rect, false};
  EnumDisplayMonitors(
      nullptr, nullptr,
      [](HMONITOR, HDC, LPRECT monitor, LPARAM data) -> BOOL {
        auto* state = reinterpret_cast<Search*>(data);
        RECT overlap{};
        if (IntersectRect(&overlap, &state->rect, monitor) &&
            overlap.right - overlap.left >= kMinimumVisibleRestoreExtent &&
            overlap.bottom - overlap.top >= kMinimumVisibleRestoreExtent) {
          state->visible = true;
          return FALSE;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&search));
  return search.visible;
}

HBRUSH BackgroundBrush() {
  static HBRUSH brush = CreateSolidBrush(kBackgroundColor);
  return brush;
}

float ScaleForDpi(UINT dpi) {
  return static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
}

int Scale(int dips, UINT dpi) { return MulDiv(dips, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI); }

constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW;
constexpr DWORD kFullscreenStyle = WS_POPUP;
constexpr DWORD kExStyle = WS_EX_APPWINDOW;

RECT WindowRectForClient(int client_width, int client_height, DWORD style, UINT dpi) {
  RECT rect{0, 0, client_width, client_height};
  AdjustWindowRectExForDpi(&rect, style, FALSE, kExStyle, dpi);
  return rect;
}

}

bool ParseWindowMode(std::string_view text, WindowMode* mode) {
  if (mode == nullptr) {
    return false;
  }
  if (text == "windowed") {
    *mode = WindowMode::kWindowed;
  } else if (text == "borderless") {
    *mode = WindowMode::kBorderless;
  } else if (text == "fullscreen") {
    *mode = WindowMode::kFullscreen;
  } else {
    return false;
  }
  return true;
}

const char* WindowModeName(WindowMode mode) {
  switch (mode) {
    case WindowMode::kWindowed:
      return "windowed";
    case WindowMode::kBorderless:
      return "borderless";
    case WindowMode::kFullscreen:
      return "fullscreen";
  }
  return "unknown";
}

bool AltEnterFullscreenFromFlags(const elixir::fflags::FlagSet& effective) {
  const std::string* value = effective.Find("FFlagHandleAltEnterFullscreenManually");
  if (value == nullptr) {
    return true;
  }
  const fflags::ValueCheck check = fflags::NormalizeValue(fflags::FlagType::kBool, *value);
  return !(check.valid && check.normalized == "False");
}

const char* HostWindowEventTypeName(HostWindowEventType type) {
  switch (type) {
    case HostWindowEventType::kResized:
      return "Resized";
    case HostWindowEventType::kMinimized:
      return "Minimized";
    case HostWindowEventType::kRestored:
      return "Restored";
    case HostWindowEventType::kFocusGained:
      return "FocusGained";
    case HostWindowEventType::kFocusLost:
      return "FocusLost";
    case HostWindowEventType::kDpiChanged:
      return "DpiChanged";
    case HostWindowEventType::kFullscreenChanged:
      return "FullscreenChanged";
    case HostWindowEventType::kShown:
      return "Shown";
    case HostWindowEventType::kCloseRequested:
      return "CloseRequested";
  }
  return "Unknown";
}

bool RawAbsoluteToRelative(RawAbsoluteMotion* state, LONG raw_x, LONG raw_y, int width,
                           int height, bool remote_session, float* delta_x, float* delta_y) {
  if (state == nullptr || delta_x == nullptr || delta_y == nullptr || width <= 0 ||
      height <= 0) {
    return false;
  }
  *delta_x = 0.0F;
  *delta_y = 0.0F;
  const int x = static_cast<int>(static_cast<float>(raw_x) / 65535.0F * static_cast<float>(width));
  const int y =
      static_cast<int>(static_cast<float>(raw_y) / 65535.0F * static_cast<float>(height));
  if (!state->has_last) {
    state->has_last = true;
    state->last_x = x;
    state->last_y = y;
    return false;
  }
  const int relative_x = x - state->last_x;
  const int relative_y = y - state->last_y;
  state->last_x = x;
  state->last_y = y;
  const int limit = remote_session ? height / 6 : kMaximumAbsoluteJump;
  const bool jump = remote_session
                        ? (std::abs(relative_x) >= limit || std::abs(relative_y) >= limit)
                        : (std::abs(relative_x) > limit || std::abs(relative_y) > limit);
  if (jump || (relative_x == 0 && relative_y == 0)) {
    return false;
  }
  *delta_x = static_cast<float>(relative_x);
  *delta_y = static_cast<float>(relative_y);
  return true;
}

GameWindow* CurrentGameWindow() {
  CurrentWindowState& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.window;
}

void SetCurrentGameWindow(GameWindow* window) {
  CurrentWindowState& state = State();
  std::unique_lock<std::mutex> lock(state.mutex);
  GameWindow* previous = state.window;
  if (previous == window) {
    return;
  }
  state.window = window;
  if (previous != nullptr) {
    WaitForOtherReferences(lock, previous);
  }
}

void RetireCurrentGameWindow(GameWindow* window) {
  CurrentWindowState& state = State();
  std::unique_lock<std::mutex> lock(state.mutex);
  if (window == nullptr || state.window != window) {
    return;
  }
  state.window = nullptr;
  WaitForOtherReferences(lock, window);
}

CurrentGameWindowRef::CurrentGameWindowRef() {
  CurrentWindowState& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  GameWindow* window = state.window;
  if (window == nullptr) {
    return;
  }
  std::size_t* all = ReferenceCount(state.references, window, true);
  std::size_t* mine = ReferenceCount(t_references, window, true);
  if (all == nullptr || mine == nullptr) {
    return;
  }
  ++*all;
  ++*mine;
  window_ = window;
}

CurrentGameWindowRef::~CurrentGameWindowRef() {
  if (window_ == nullptr) {
    return;
  }
  CurrentWindowState& state = State();
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    if (std::size_t* all = ReferenceCount(state.references, window_, false)) {
      --*all;
    }
    if (std::size_t* mine = ReferenceCount(t_references, window_, false)) {
      --*mine;
    }
  }
  state.released.notify_all();
}

std::size_t CurrentGameWindowReferenceCount(const GameWindow* window) {
  CurrentWindowState& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  return ReferencesHeld(state.references, window) - ReferencesHeld(t_references, window);
}

std::uint64_t GameWindow::MonotonicNs() {
  static const std::uint64_t frequency = [] {
    LARGE_INTEGER value{};
    QueryPerformanceFrequency(&value);
    return static_cast<std::uint64_t>(value.QuadPart);
  }();
  LARGE_INTEGER counter{};
  QueryPerformanceCounter(&counter);
  const auto ticks = static_cast<std::uint64_t>(counter.QuadPart);
  return (ticks / frequency) * 1'000'000'000ULL +
         (ticks % frequency) * 1'000'000'000ULL / frequency;
}

bool GameWindow::ClipCursorScreen(const RECT* screen_rect) {
  const CursorOps& ops = options_.cursor_ops;
  if (ops.clip_cursor) {
    return ops.clip_cursor(screen_rect);
  }
  return ClipCursor(screen_rect) != FALSE;
}

bool GameWindow::WarpCursorScreen(int screen_x, int screen_y) {
  const CursorOps& ops = options_.cursor_ops;
  if (ops.set_cursor_pos) {
    return ops.set_cursor_pos(screen_x, screen_y);
  }
  return SetCursorPos(screen_x, screen_y) != FALSE;
}

bool Win32PointerCaptureBackend::Clip(const RECT* screen_rect) {
  return window_->ClipCursorScreen(screen_rect);
}

bool Win32PointerCaptureBackend::WarpCursor(int screen_x, int screen_y) {
  return window_->WarpCursorScreen(screen_x, screen_y);
}

bool Win32PointerCaptureBackend::Apply(const mwindow::PointerCaptureRequest& request) {
  HWND hwnd = window_ != nullptr ? window_->hwnd_ : nullptr;
  if (hwnd == nullptr) {
    relative_mode_ = false;
    cursor_hidden_ = false;
    if (clip_applied_ && window_ != nullptr) {
      Clip(nullptr);
    }
    clip_applied_ = false;
    return !request.relative_mode;
  }
  std::optional<mwindow::PointerPosition> released_at;
  if (request.relative_mode && !relative_mode_) {
    if (request.release_position == mwindow::PointerReleasePosition::kDragStart) {
      drag_start_ = request.anchor;
      if (!drag_start_) {
        POINT cursor{};
        GetCursorPos(&cursor);
        ScreenToClient(hwnd, &cursor);
        drag_start_ = mwindow::PointerPosition{static_cast<float>(cursor.x),
                                               static_cast<float>(cursor.y)};
      }
    }
    relative_mode_ = true;
    window_->raw_absolute_ = {};
    const bool raw = window_->options_.raw_mouse && window_->SetRawMouseRegistered(true);
    if (!raw && window_->focused_) {
      POINT center = window_->ClientCenter();
      ClientToScreen(hwnd, &center);
      WarpCursor(center.x, center.y);
    }
  }
  if (!request.relative_mode && relative_mode_) {
    if (request.release_position != mwindow::PointerReleasePosition::kUnchanged &&
        window_->focused_ && window_->pixel_width_ > 0 && window_->pixel_height_ > 0) {
      POINT target = window_->ClientCenter();
      if (request.release_position == mwindow::PointerReleasePosition::kDragStart &&
          drag_start_ && std::isfinite(drag_start_->x) && std::isfinite(drag_start_->y)) {
        target.x = static_cast<LONG>(std::lround(std::clamp(
            drag_start_->x, 0.0F, static_cast<float>(window_->pixel_width_) - 1.0F)));
        target.y = static_cast<LONG>(std::lround(std::clamp(
            drag_start_->y, 0.0F, static_cast<float>(window_->pixel_height_) - 1.0F)));
      }
      window_->cursor_x_ = static_cast<float>(target.x);
      window_->cursor_y_ = static_cast<float>(target.y);
      window_->has_cursor_position_ = true;
      ClientToScreen(hwnd, &target);
      WarpCursor(target.x, target.y);
      released_at = mwindow::PointerPosition{window_->cursor_x_, window_->cursor_y_};
    }
    relative_mode_ = false;
    drag_start_.reset();
    window_->SetRawMouseRegistered(false);
  }
  cursor_hidden_ = !request.cursor_visible;
  Reassert();
  if (released_at) {
    window_->EmitMotion({released_at->x, released_at->y, 0.0F, 0.0F, window_->ButtonMask(), false});
  }
  return true;
}

void Win32PointerCaptureBackend::OnWindowFocusGained() {
  HWND hwnd = window_ != nullptr ? window_->hwnd_ : nullptr;
  if (hwnd == nullptr || !relative_mode_) {
    return;
  }
  if (!window_->raw_input_registered_ && window_->pixel_width_ > 0 &&
      window_->pixel_height_ > 0) {
    POINT center = window_->ClientCenter();
    ClientToScreen(hwnd, &center);
    WarpCursor(center.x, center.y);
  }
  Reassert();
}

void Win32PointerCaptureBackend::Reassert() {
  HWND hwnd = window_ != nullptr ? window_->hwnd_ : nullptr;
  if (hwnd == nullptr) {
    return;
  }
  if (relative_mode_ && window_->options_.confine_cursor && window_->focused_ &&
      window_->pixel_width_ > 0 && window_->pixel_height_ > 0) {
    RECT clip{};
    if (window_->raw_input_registered_) {
      const POINT center = window_->ClientCenter();
      clip = {center.x, center.y, center.x + 2, center.y + 2};
    } else {
      GetClientRect(hwnd, &clip);
    }
    POINT top_left{clip.left, clip.top};
    POINT bottom_right{clip.right, clip.bottom};
    ClientToScreen(hwnd, &top_left);
    ClientToScreen(hwnd, &bottom_right);
    const RECT screen_clip{top_left.x, top_left.y, bottom_right.x, bottom_right.y};
    clip_applied_ = Clip(&screen_clip);
  } else if (clip_applied_) {
    Clip(nullptr);
    clip_applied_ = false;
  }
  POINT cursor{};
  if (GetCursorPos(&cursor) && WindowFromPoint(cursor) == hwnd) {
    ScreenToClient(hwnd, &cursor);
    RECT client{};
    GetClientRect(hwnd, &client);
    if (PtInRect(&client, cursor)) {
      SetCursor(cursor_hidden_ ? nullptr : LoadCursorW(nullptr, IDC_ARROW));
    }
  }
}

GameWindow::GameWindow()
    : pointer_backend_(std::make_unique<Win32PointerCaptureBackend>(this)),
      pointer_owner_(
          std::make_unique<mwindow::WindowPointerCaptureOwner>(pointer_backend_.get())),
      text_owner_(std::make_unique<mwindow::WindowTextInputOwner>(&text_backend_)) {}

GameWindow::~GameWindow() {
  Destroy();
  text_owner_.reset();
  pointer_owner_.reset();
  pointer_backend_.reset();
}

Status GameWindow::Create(const GameWindowOptions& options) {
  if (hwnd_ != nullptr) {
    return Status::Error(StatusCode::kFailedPrecondition, "game window already exists");
  }
  options_ = options;
  if (options_.offscreen) {
    options_.mode = WindowMode::kWindowed;
  }
  if (options_.width < mwindow::kMinimumWindowWidth ||
      options_.height < mwindow::kMinimumWindowHeight) {
    return Status::Error(StatusCode::kInvalidArgument, "window size is below the minimum");
  }

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  HINSTANCE instance = GetModuleHandleW(nullptr);
  static std::once_flag class_once;
  static ATOM class_atom = 0;
  std::call_once(class_once, [&] {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &GameWindow::WndProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = BackgroundBrush();
    window_class.lpszClassName = kWindowClassName;
    HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.hIcon = icon != nullptr ? icon : LoadIconW(nullptr, IDI_APPLICATION);
    HICON small_icon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                     GetSystemMetrics(SM_CXSMICON),
                                                     GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    window_class.hIconSm = small_icon != nullptr ? small_icon : window_class.hIcon;
    class_atom = RegisterClassExW(&window_class);
  });
  if (class_atom == 0) {
    return Status::Error(StatusCode::kPlatformError, "cannot register the game window class");
  }

  std::optional<POINT> restored_position;
  bool restore_maximized = false;
  int client_dips_width = options_.width;
  int client_dips_height = options_.height;
  persistence_active_ = false;
  persistence_dirty_ = false;
  bool enable_persistence = false;
  if (!options_.state_path.empty()) {
    const mwindow::WindowStateLoadResult loaded = mwindow::LoadWindowState(options_.state_path);
    if (!loaded.status.ok()) {
      std::fprintf(stderr, "  [window-state] ignored: %s\n", loaded.status.message().c_str());
    } else if (loaded.found &&
               mwindow::RestoredSizeApplies(loaded.state, options_.width, options_.height)) {
      client_dips_width = loaded.state.width;
      client_dips_height = loaded.state.height;
      restore_maximized = loaded.state.maximized;
      if (loaded.state.has_position) {
        restored_position = POINT{loaded.state.x, loaded.state.y};
      }
    }
    persisted_ = {};
    persisted_.width = client_dips_width;
    persisted_.height = client_dips_height;
    persisted_.configured_width = options_.width;
    persisted_.configured_height = options_.height;
    persisted_.has_configured_size = true;
    enable_persistence = true;
  }

  const int initial_x = options_.offscreen ? kOffscreenOrigin : CW_USEDEFAULT;
  const int initial_y = options_.offscreen ? kOffscreenOrigin : CW_USEDEFAULT;
  HWND created = CreateWindowExW(kExStyle, kWindowClassName, options_.title.c_str(),
                                 kWindowedStyle, initial_x, initial_y, 320, 240, nullptr,
                                 nullptr, instance, this);
  if (created == nullptr || hwnd_ != created) {
    hwnd_ = nullptr;
    return Status::Error(StatusCode::kPlatformError, "CreateWindowExW failed for the game window");
  }
  owner_thread_id_ = GetCurrentThreadId();
  wake_hwnd_.store(hwnd_, std::memory_order_release);

  const BOOL dark = TRUE;
  DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

  if (!options_.offscreen) {
    HMONITOR monitor = nullptr;
    if (restored_position) {
      const RECT saved{restored_position->x, restored_position->y,
                       restored_position->x + client_dips_width,
                       restored_position->y + client_dips_height};
      if (RectIsVisibleOnAMonitor(saved)) {
        monitor = MonitorFromRect(&saved, MONITOR_DEFAULTTONEAREST);
      } else {
        std::fprintf(stderr,
                     "  [window-state] saved position is outside active displays; "
                     "centring the window\n");
        restored_position.reset();
      }
    }
    if (monitor == nullptr) {
      monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    }
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    const POINT anchor = restored_position.value_or(
        POINT{(info.rcWork.left + info.rcWork.right) / 2,
              (info.rcWork.top + info.rcWork.bottom) / 2});
    SetWindowPos(hwnd_, nullptr, anchor.x, anchor.y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    dpi_ = GetDpiForWindow(hwnd_);
    const RECT frame = WindowRectForClient(Scale(client_dips_width, dpi_),
                                           Scale(client_dips_height, dpi_), kWindowedStyle, dpi_);
    const int frame_width = frame.right - frame.left;
    const int frame_height = frame.bottom - frame.top;
    int x = restored_position ? restored_position->x
                              : (info.rcWork.left + info.rcWork.right - frame_width) / 2;
    int y = restored_position ? restored_position->y
                              : (info.rcWork.top + info.rcWork.bottom - frame_height) / 2;
    if (!restored_position) {
      x = std::max<int>(x, info.rcWork.left);
      y = std::max<int>(y, info.rcWork.top);
    }
    SetWindowPos(hwnd_, nullptr, x, y, frame_width, frame_height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  } else {
    dpi_ = GetDpiForWindow(hwnd_);
    const RECT frame = WindowRectForClient(Scale(client_dips_width, dpi_),
                                           Scale(client_dips_height, dpi_), kWindowedStyle, dpi_);
    SetWindowPos(hwnd_, nullptr, kOffscreenOrigin, kOffscreenOrigin, frame.right - frame.left,
                 frame.bottom - frame.top, SWP_NOZORDER | SWP_NOACTIVATE);
  }
  dpi_ = GetDpiForWindow(hwnd_);
  ApplyIcons();
  restore_maximized_pending_ = restore_maximized && !options_.offscreen;
  persistence_active_ = enable_persistence;
  persisted_.maximized = restore_maximized_pending_;

  raw_input_registered_ = false;

  text_backend_.Attach(hwnd_, [this] { return CoordinateTransform(); });
  RefreshClientSize();
  emitted_width_ = pixel_width_;
  emitted_height_ = pixel_height_;
  emitted_scale_ = ScaleForDpi(dpi_);

  const Status surface_status = surface_.Activate(reinterpret_cast<std::uintptr_t>(hwnd_),
                                                  pixel_width_, pixel_height_,
                                                  ScaleForDpi(dpi_));
  if (!surface_status.ok()) {
    Destroy();
    return surface_status;
  }
  if (options_.resize_readiness.enabled) {
    resize_readiness_ = std::make_unique<mwindow::WindowResizeReadinessGate>();
    const Status resize_status =
        resize_readiness_->Activate(options_.resize_readiness, surface_.Snapshot());
    if (!resize_status.ok()) {
      Destroy();
      return resize_status;
    }
  }
  present_count_.store(0, std::memory_order_relaxed);
  show_requested_.store(false, std::memory_order_relaxed);
  present_lifecycle_.Activate();
  observer_.Activate();
  pre_text_input_gate_.Activate();
  pump_hook_gate_.Activate();
  surface_recovery_.Activate();
  fullscreen_request_.Reset();
  fullscreen_menu_request_.Reset();
  pacer_.emplace(options_.pump_interval_ns);
  quit_requested_ = false;
  initial_fullscreen_sync_pending_ = true;
  keys_down_.reset();
  keys_delivered_.reset();

  pace_timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                       TIMER_ALL_ACCESS);
  if (pace_timer_ == nullptr) {
    pace_timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
  }

  if (options_.mode != WindowMode::kWindowed) {
    ApplyFullscreen(true);
  }
  emitted_width_ = pixel_width_;
  emitted_height_ = pixel_height_;
  emitted_scale_ = ScaleForDpi(dpi_);
  {
    mwindow::WindowSurfaceEvent discarded;
    while (surface_.Poll(&discarded)) {
    }
    std::lock_guard<std::mutex> lock(host_events_mutex_);
    host_events_.clear();
  }

  focused_ = GetFocus() == hwnd_;
  if (!options_.start_hidden) {
    ShowNow();
  }
  if (CurrentGameWindow() == nullptr) {
    SetCurrentGameWindow(this);
  }
  std::fprintf(stderr,
               "  [window] created %s %ux%u px dpi=%u raw_mouse=%d alt_enter=%d\n",
               WindowModeName(options_.mode), pixel_width_, pixel_height_, dpi_,
               options_.raw_mouse ? 1 : 0, options_.alt_enter_fullscreen ? 1 : 0);
  return Status::Ok();
}

void GameWindow::Destroy() {
  if (hwnd_ == nullptr) {
    return;
  }
  if (owner_thread_id_ != 0 && GetCurrentThreadId() != owner_thread_id_) {
    std::fprintf(stderr,
                 "  [window] Destroy called on thread %lu, but the window belongs to thread "
                 "%lu\n",
                 GetCurrentThreadId(), owner_thread_id_);
  }
  wake_hwnd_.store(nullptr, std::memory_order_release);
  RetireCurrentGameWindow(this);
  PersistWindowState();
  if (pacer_) {
    pacer_->Reset();
  }
  pre_text_input_gate_.Deactivate();
  pump_hook_gate_.Deactivate();
  observer_.Deactivate();
  surface_recovery_.Deactivate();
  surface_.Deactivate();
  present_lifecycle_.Deactivate();
  text_owner_->Shutdown();
  pointer_owner_->Shutdown();
  text_backend_.Detach();
  fullscreen_request_.Reset();
  fullscreen_menu_request_.Reset();
  SetRawMouseRegistered(false);
  if (GetCapture() == hwnd_) {
    ReleaseCapture();
  }
  HWND hwnd = hwnd_;
  SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
  hwnd_ = nullptr;
  if (microphone_menu_ != nullptr && !microphone_menu_attached_) DestroyMenu(microphone_menu_);
  DestroyWindow(hwnd);
  microphone_menu_ = nullptr;
  microphone_menu_attached_ = false;
  refresh_microphones_ = {};
  select_microphone_ = {};
  microphone_items_.clear();
  for (HICON* icon : {&icon_big_, &icon_small_}) {
    if (*icon != nullptr) {
      DestroyIcon(*icon);
      *icon = nullptr;
    }
  }
  if (pace_timer_ != nullptr) {
    CloseHandle(pace_timer_);
    pace_timer_ = nullptr;
  }
  resize_readiness_.reset();
  fullscreen_ = false;
  has_windowed_placement_ = false;
  restore_maximized_pending_ = false;
  visible_ = false;
  minimized_ = false;
  focused_ = false;
  has_pending_motion_ = false;
  buttons_down_ = 0;
  has_cursor_position_ = false;
  keys_down_.reset();
  keys_delivered_.reset();
  alt_enter_consumed_ = false;
  f11_consumed_ = false;
  persistence_active_ = false;
  pixel_width_ = 0;
  pixel_height_ = 0;
  dpi_ = USER_DEFAULT_SCREEN_DPI;
  geometry_.store(0, std::memory_order_release);
  owner_thread_id_ = 0;
  {
    std::lock_guard<std::mutex> lock(host_events_mutex_);
    host_events_.clear();
  }
}

LRESULT CALLBACK GameWindow::WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                     LPARAM lparam) noexcept {
  GameWindow* self = nullptr;
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
    self = static_cast<GameWindow*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    if (self != nullptr) {
      self->hwnd_ = hwnd;
    }
  } else {
    self = reinterpret_cast<GameWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (self == nullptr || self->hwnd_ != hwnd) {
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }
  return self->HandleMessage(message, wparam, lparam);
}

bool GameWindow::SetMicrophoneMenu(std::function<std::vector<MicrophoneMenuItem>()> refresh,
                                    std::function<void(std::wstring_view)> select) {
  if (hwnd_ == nullptr || microphone_menu_ != nullptr || !refresh || !select) return false;
  const HMENU submenu = CreatePopupMenu();
  if (submenu == nullptr) return false;
  if (AppendMenuW(submenu, MF_STRING | MF_GRAYED, kMicrophoneCommandFirst, L"Microphone unavailable") == FALSE) {
    DestroyMenu(submenu);
    return false;
  }
  microphone_menu_ = submenu;
  if (!fullscreen_ && !AttachMicrophoneSystemMenu()) {
    DestroyMenu(submenu);
    microphone_menu_ = nullptr;
    return false;
  }
  refresh_microphones_ = std::move(refresh);
  select_microphone_ = std::move(select);
  return true;
}

bool GameWindow::AttachMicrophoneSystemMenu() {
  if (microphone_menu_attached_) return true;
  const HMENU system_menu = GetSystemMenu(hwnd_, FALSE);
  microphone_menu_attached_ = system_menu != nullptr &&
      AppendMenuW(system_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(microphone_menu_), L"&Microphone") != FALSE;
  return microphone_menu_attached_;
}

void GameWindow::RefreshMicrophoneMenu() {
  while (GetMenuItemCount(microphone_menu_) > 0 && DeleteMenu(microphone_menu_, 0, MF_BYPOSITION)) {
  }
  microphone_items_ = refresh_microphones_();
  if (microphone_items_.size() > kMaximumMicrophoneItems) microphone_items_.resize(kMaximumMicrophoneItems);
  for (std::size_t index = 0; index < microphone_items_.size(); ++index) {
    const MicrophoneMenuItem& item = microphone_items_[index];
    const UINT flags = MF_STRING | (item.enabled ? MF_ENABLED : MF_GRAYED) | (item.selected ? MF_CHECKED : 0);
    const UINT command = kMicrophoneCommandFirst + static_cast<UINT>(index) * kMicrophoneCommandStride;
    std::wstring label = item.label;
    for (std::size_t at = label.find(L'&'); at != std::wstring::npos; at = label.find(L'&', at + 2)) {
      label.insert(at, 1, L'&');
    }
    AppendMenuW(microphone_menu_, flags, command, label.c_str());
  }
}

bool GameWindow::InvokeMicrophoneMenuCommand(UINT command) {
  if (microphone_menu_ == nullptr || command < kMicrophoneCommandFirst ||
      command >= kMicrophoneCommandFirst + kMaximumMicrophoneItems * kMicrophoneCommandStride) {
    return false;
  }
  const std::size_t index = (command - kMicrophoneCommandFirst) / kMicrophoneCommandStride;
  if (index < microphone_items_.size() && microphone_items_[index].enabled) {
    select_microphone_(microphone_items_[index].endpoint_id);
  }
  return true;
}

LRESULT GameWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case kShowFullscreenMicrophoneMenu: {
      if (fullscreen_ && microphone_menu_ != nullptr && GetForegroundWindow() == hwnd_) {
        RefreshMicrophoneMenu();
        POINT position{16, 16};
        ClientToScreen(hwnd_, &position);
        ReleaseDeliveredKeys();
        const UINT command = TrackPopupMenu(microphone_menu_, TPM_RETURNCMD | TPM_NONOTIFY,
                                            position.x, position.y, 0, hwnd_, nullptr);
        InvokeMicrophoneMenuCommand(command);
      }
      return 0;
    }
    case WM_CLOSE:
      quit_requested_ = true;
      PushHostEvent(HostWindowEventType::kCloseRequested);
      Emit(platform::QuitEvent{});
      return 0;
    case WM_ERASEBKGND: {
      if (!HasPresentedFrame()) {
        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(reinterpret_cast<HDC>(wparam), &client, BackgroundBrush());
      }
      return 1;
    }
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(hwnd_, &paint);
      if (dc != nullptr && !HasPresentedFrame()) {
        FillRect(dc, &paint.rcPaint, BackgroundBrush());
      }
      EndPaint(hwnd_, &paint);
      return 0;
    }
    case WM_SETFOCUS:
      OnFocus(true);
      return 0;
    case WM_KILLFOCUS:
      OnFocus(false);
      return 0;
    case WM_SIZE:
      OnSize(wparam);
      return 0;
    case WM_MOVE:
      pointer_backend_->Reassert();
      CaptureWindowState();
      return 0;
    case WM_DPICHANGED:
      OnDpiChanged(wparam, reinterpret_cast<const RECT*>(lparam));
      return 0;
    case WM_DISPLAYCHANGE:
      if (fullscreen_) {
        FitToMonitor();
      }
      return 0;
    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
      const UINT dpi = hwnd_ != nullptr ? GetDpiForWindow(hwnd_) : dpi_;
      const RECT frame = WindowRectForClient(Scale(mwindow::kMinimumWindowWidth, dpi),
                                             Scale(mwindow::kMinimumWindowHeight, dpi),
                                             kWindowedStyle, dpi);
      info->ptMinTrackSize.x = frame.right - frame.left;
      info->ptMinTrackSize.y = frame.bottom - frame.top;
      return 0;
    }
    case WM_EXITSIZEMOVE:
      CaptureWindowState();
      return 0;
    case WM_SETCURSOR:
      if (LOWORD(lparam) == HTCLIENT) {
        SetCursor(pointer_backend_->cursor_hidden() ? nullptr : LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
      }
      break;
    case WM_INPUT:
      OnRawInput(lparam);
      break;
    case WM_DEVICECHANGE:
      device_changes_.fetch_add(1, std::memory_order_relaxed);
      return TRUE;
    case WM_MOUSEMOVE:
      OnMouseMove(lparam);
      return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
      OnMouseButton(SDL_BUTTON_LEFT, message == WM_LBUTTONDOWN, lparam);
      return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
      OnMouseButton(SDL_BUTTON_RIGHT, message == WM_RBUTTONDOWN, lparam);
      return 0;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
      OnMouseButton(SDL_BUTTON_MIDDLE, message == WM_MBUTTONDOWN, lparam);
      return 0;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
      OnMouseButton(GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? SDL_BUTTON_X1 : SDL_BUTTON_X2,
                    message == WM_XBUTTONDOWN, lparam);
      return TRUE;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      OnMouseWheel(message == WM_MOUSEHWHEEL, wparam, lparam);
      return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
      return OnKey(message, wparam, lparam);
    case WM_CHAR:
    case WM_UNICHAR:
      OnChar(message, wparam);
      return message == WM_UNICHAR && wparam == UNICODE_NOCHAR ? TRUE : 0;
    case WM_SYSCHAR:
      if (wparam == VK_SPACE && (!fullscreen_ || microphone_menu_ == nullptr)) {
        break;
      }
      return 0;
    case WM_MENUCHAR:
      return MAKELRESULT(0, MNC_CLOSE);
    case WM_INITMENUPOPUP:
      if (microphone_menu_ != nullptr && reinterpret_cast<HMENU>(wparam) == microphone_menu_) {
        RefreshMicrophoneMenu();
      }
      break;
    case WM_SYSCOMMAND: {
      const WPARAM command = wparam & 0xFFF0;
      if (InvokeMicrophoneMenuCommand(static_cast<UINT>(command))) return 0;
      if (command == SC_KEYMENU && lparam != VK_SPACE) {
        return 0;
      }
      if ((command == SC_SCREENSAVE || command == SC_MONITORPOWER) && focused_ && visible_) {
        return 0;
      }
      break;
    }
    case WM_IME_SETCONTEXT:
      if (TextInputActive()) {
        lparam &= ~static_cast<LPARAM>(ISC_SHOWUICOMPOSITIONWINDOW);
      }
      return DefWindowProcW(hwnd_, message, wparam, lparam);
    case WM_IME_STARTCOMPOSITION:
      if (TextInputActive()) {
        text_backend_.set_composing(true);
        return 0;
      }
      break;
    case WM_IME_COMPOSITION:
      if (TextInputActive()) {
        OnImeComposition(lparam);
        return 0;
      }
      break;
    case WM_IME_ENDCOMPOSITION:
      if (TextInputActive()) {
        std::optional<platform::TextEditingEvent> editing;
        if (text_backend_.HandleImeEndComposition(&editing) && editing) {
          Emit(*editing);
        }
        return 0;
      }
      break;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void GameWindow::Emit(platform::PlatformEventPayload payload) {
  FlushMotion();
  observer_.Notify({MonotonicNs(), std::move(payload)});
}

void GameWindow::EmitMotion(const platform::MouseMotionEvent& motion) {
  if (has_pending_motion_) {
    auto* pending = std::get_if<platform::MouseMotionEvent>(&pending_motion_.payload);
    if (pending != nullptr && pending->relative_mode == motion.relative_mode) {
      pending->delta_x += motion.delta_x;
      pending->delta_y += motion.delta_y;
      pending->x = motion.x;
      pending->y = motion.y;
      pending->buttons = motion.buttons;
      pending_motion_.timestamp_ns = MonotonicNs();
      return;
    }
    FlushMotion();
  }
  pending_motion_ = {MonotonicNs(), motion};
  has_pending_motion_ = true;
  if (!in_pump_) {
    FlushMotion();
  }
}

void GameWindow::FlushMotion() {
  if (!has_pending_motion_) {
    return;
  }
  has_pending_motion_ = false;
  ++stats_.motion_events;
  const platform::PlatformEvent event = pending_motion_;
  pending_motion_ = {};
  observer_.Notify(event);
}

void GameWindow::PushHostEvent(HostWindowEventType type) {
  HostWindowEvent event;
  event.type = type;
  event.timestamp_ns = MonotonicNs();
  event.pixel_width = pixel_width_;
  event.pixel_height = pixel_height_;
  event.dpi_scale = ScaleForDpi(dpi_);
  event.fullscreen = fullscreen_;
  std::lock_guard<std::mutex> lock(host_events_mutex_);
  if (host_events_.size() >= 256) {
    host_events_.pop_front();
  }
  host_events_.push_back(event);
}

bool GameWindow::PollHostEvent(HostWindowEvent* event) {
  if (event == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(host_events_mutex_);
  if (host_events_.empty()) {
    return false;
  }
  *event = host_events_.front();
  host_events_.pop_front();
  return true;
}

bool GameWindow::PollSurfaceEvent(mwindow::WindowSurfaceEvent* event) {
  return surface_.Poll(event);
}

mwindow::WindowSurfaceSnapshot GameWindow::SurfaceSnapshot() const {
  return surface_.Snapshot();
}

bool GameWindow::Pump() {
  if (hwnd_ == nullptr) {
    return false;
  }
  in_pump_ = true;
  ++stats_.pumps;
  if (show_requested_.exchange(false, std::memory_order_acq_rel)) {
    ShowIfHidden();
  }
  if (initial_fullscreen_sync_pending_ && HasPresentedFrame()) {
    if (fullscreen_sync_.Notify(fullscreen_)) {
      initial_fullscreen_sync_pending_ = false;
    }
  }
  MaybeApplyFullscreenRequests();
  MaybeRequestResizeReadiness();
  if (!pre_text_input_gate_.Invoke()) {
    std::fprintf(stderr, "  [input] main-thread text command drain failed\n");
  }
  if (!text_owner_->Pump()) {
    std::fprintf(stderr, "  [input] text-input state update failed\n");
  }
  if (!pointer_owner_->Pump(TextInputActive())) {
    std::fprintf(stderr, "  [input] pointer capture state update failed\n");
  }

  MSG message;
  while (hwnd_ != nullptr && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
    ++stats_.messages;
    if (message.message == WM_QUIT) {
      quit_requested_ = true;
      continue;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  FlushMotion();
  in_pump_ = false;
  if (hwnd_ == nullptr) {
    return false;
  }
  if (!pump_hook_gate_.Invoke()) {
    std::fprintf(stderr, "  [input] main-thread pump hook failed\n");
  }
  MaybeRecoverVulkanSurface();
  if (persistence_dirty_ && MonotonicNs() - persistence_change_ns_ >= kPersistDelayNs) {
    PersistWindowState();
  }
  return !quit_requested_;
}

std::uint64_t GameWindow::PaceInputPump() {
  if (hwnd_ == nullptr || !pacer_) {
    return 0;
  }
  constexpr UINT kWakeMask = QS_INPUT | QS_POSTMESSAGE | QS_SENDMESSAGE;
  if (HIWORD(GetQueueStatus(kWakeMask)) != 0) {
    return 0;
  }
  const std::uint64_t delay_ns = pacer_->DelayBeforeNextPump(MonotonicNs());
  if (delay_ns == 0) {
    return 0;
  }
  const std::uint64_t start = MonotonicNs();
  if (pace_timer_ != nullptr) {
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(std::max<std::uint64_t>(delay_ns / 100, 1));
    if (SetWaitableTimerEx(pace_timer_, &due, 0, nullptr, nullptr, nullptr, 0)) {
      if (options_.pump_wake_on_input) {
        const DWORD result = MsgWaitForMultipleObjectsEx(1, &pace_timer_, INFINITE, kWakeMask,
                                                         MWMO_INPUTAVAILABLE);
        if (result != WAIT_OBJECT_0) {
          CancelWaitableTimer(pace_timer_);
        }
      } else {
        WaitForSingleObject(pace_timer_, INFINITE);
      }
    } else {
      Sleep(static_cast<DWORD>(delay_ns / 1'000'000ULL));
    }
  } else {
    Sleep(static_cast<DWORD>(delay_ns / 1'000'000ULL));
  }
  const std::uint64_t slept = MonotonicNs() - start;
  ++stats_.pace_sleeps;
  stats_.pace_sleep_ns += slept;
  return slept;
}

GameWindowSnapshot GameWindow::Snapshot() const {
  GameWindowSnapshot snapshot;
  const Geometry geometry = UnpackGeometry(geometry_.load(std::memory_order_acquire));
  snapshot.hwnd = hwnd_;
  snapshot.pixel_width = geometry.width;
  snapshot.pixel_height = geometry.height;
  snapshot.dpi = geometry.dpi;
  snapshot.dpi_scale = ScaleForDpi(geometry.dpi);
  snapshot.mode = options_.mode;
  snapshot.fullscreen = fullscreen_;
  snapshot.visible = visible_;
  snapshot.minimized = minimized_;
  snapshot.focused = focused_;
  snapshot.presented = HasPresentedFrame();
  snapshot.pointer_captured = pointer_owner_->captured();
  snapshot.cursor_visible = !pointer_backend_->cursor_hidden();
  snapshot.text_input_active = text_backend_.active();
  return snapshot;
}

mwindow::WindowViewportSnapshot GameWindow::Viewport() const {
  const Geometry geometry = UnpackGeometry(geometry_.load(std::memory_order_acquire));
  mwindow::WindowViewportSnapshot viewport;
  viewport.logical_width = static_cast<int>(geometry.width);
  viewport.logical_height = static_cast<int>(geometry.height);
  viewport.pixel_width = static_cast<int>(geometry.width);
  viewport.pixel_height = static_cast<int>(geometry.height);
  viewport.dpi_scale = ScaleForDpi(geometry.dpi);
  return viewport;
}

platform::SurfaceCoordinateTransform GameWindow::CoordinateTransform() const {
  const mwindow::WindowViewportSnapshot viewport = Viewport();
  return {viewport.logical_width, viewport.logical_height, viewport.pixel_width,
          viewport.pixel_height, viewport.dpi_scale};
}

InputStats GameWindow::input_stats() const { return stats_; }

void GameWindow::NoteVulkanPresent() {
  auto scope = present_lifecycle_.EnterPresent();
  if (!scope) {
    return;
  }
  surface_recovery_.NotifyPresent();
  const std::uint64_t count = present_count_.fetch_add(1, std::memory_order_acq_rel) + 1;
  if (resize_readiness_ != nullptr) {
    const Status status = resize_readiness_->RecordPresent(count);
    if (!status.ok()) {
      std::fprintf(stderr, "  [resize] present evidence rejected: %s\n", status.message().c_str());
    }
  }
  scope.NotifyObserver(count);
  if (count == 1) {
    show_requested_.store(true, std::memory_order_release);
    if (HWND wake = wake_hwnd_.load(std::memory_order_acquire); wake != nullptr) {
      PostMessageW(wake, WM_NULL, 0, 0);
    }
    std::fprintf(stderr, "  [window] first Vulkan frame presented\n");
  }
}

void GameWindow::NoteVulkanSurfaceOutOfDate() { surface_recovery_.NotifyOutOfDate(); }

bool GameWindow::HasPresentedFrame() const {
  return present_count_.load(std::memory_order_acquire) > 0;
}

void GameWindow::ShowIfHidden() {
  if (hwnd_ == nullptr || visible_) {
    return;
  }
  ShowNow();
  std::fprintf(stderr, "  [window] shown on first Roblox frame\n");
}

int GameWindow::PendingShowCommand() const {
  return restore_maximized_pending_ && !fullscreen_ ? SW_SHOWMAXIMIZED : SW_SHOW;
}

void GameWindow::ShowNow() {
  if (options_.offscreen) {
    SetWindowPos(hwnd_, nullptr, kOffscreenOrigin, kOffscreenOrigin, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  } else {
    const int command = PendingShowCommand();
    ShowWindow(hwnd_, command);
    if (command == SW_SHOWMAXIMIZED) {
      restore_maximized_pending_ = false;
    }
    SetForegroundWindow(hwnd_);
  }
  visible_ = IsWindowVisible(hwnd_) != FALSE;
  PushHostEvent(HostWindowEventType::kShown);
}

bool GameWindow::SetPresentObserver(mwindow::PresentObserver observer, void* context) {
  return present_lifecycle_.Register(observer, context);
}

void GameWindow::ClearPresentObserver() { present_lifecycle_.Clear(); }

Status GameWindow::RecordResizeReadinessSurfaceCommit(const mwindow::WindowSurfaceEvent& event) {
  return resize_readiness_ == nullptr ? Status::Ok()
                                      : resize_readiness_->RecordCommittedSurfaceEvent(event);
}

Status GameWindow::StopResizeReadiness() {
  return resize_readiness_ == nullptr ? Status::Ok() : resize_readiness_->RecordStopped();
}

Status GameWindow::ResizeReadinessCompletionStatus() const {
  return resize_readiness_ == nullptr ? Status::Ok() : resize_readiness_->CompletionStatus();
}

void GameWindow::MaybeRequestResizeReadiness() {
  if (resize_readiness_ == nullptr) {
    return;
  }
  mwindow::WindowResizeRequest request;
  if (!resize_readiness_->TakeResizeRequest(&request)) {
    return;
  }
  if (fullscreen_) {
    std::fprintf(stderr, "  [resize] readiness resize requested while fullscreen; ignored\n");
    return;
  }
  const RECT frame = WindowRectForClient(static_cast<int>(request.width),
                                         static_cast<int>(request.height), kWindowedStyle, dpi_);
  SetWindowPos(hwnd_, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  std::fprintf(stderr, "  [resize] client resize requested to %ux%u px\n", request.width,
               request.height);
}

void GameWindow::MaybeRecoverVulkanSurface() {
  if (!surface_.active()) {
    return;
  }
  mwindow::VulkanSurfaceRecoveryRequest request;
  if (!surface_recovery_.TakeRebindRequest(MonotonicNs(), &request)) {
    return;
  }
  const Status status = surface_.Recreate();
  if (!status.ok()) {
    std::fprintf(stderr, "  [vulkan] surface recovery rejected: %s\n", status.message().c_str());
    quit_requested_ = true;
    return;
  }
  const mwindow::WindowSurfaceSnapshot snapshot = surface_.Snapshot();
  std::fprintf(stderr,
               "  [vulkan] swapchain out of date count=%llu coalesced=%llu; queued surface %s "
               "recreation generation=%llu pixels=%ux%u\n",
               static_cast<unsigned long long>(request.observed_error_count),
               static_cast<unsigned long long>(request.coalesced_error_count),
               request.retry ? "retry" : "recovery",
               static_cast<unsigned long long>(snapshot.generation), snapshot.width,
               snapshot.height);
}

void GameWindow::RefreshClientSize() {
  RECT client{};
  if (hwnd_ != nullptr && GetClientRect(hwnd_, &client)) {
    pixel_width_ = static_cast<std::uint32_t>(std::max<LONG>(0, client.right - client.left));
    pixel_height_ = static_cast<std::uint32_t>(std::max<LONG>(0, client.bottom - client.top));
  }
  PublishGeometry();
}

void GameWindow::PublishGeometry() {
  geometry_.store(PackGeometry(pixel_width_, pixel_height_, dpi_), std::memory_order_release);
}

void GameWindow::ObserveSurface() {
  if (!surface_.active()) {
    return;
  }
  const Status status = surface_.Observe(reinterpret_cast<std::uintptr_t>(hwnd_),
                                         minimized_ ? 0 : pixel_width_,
                                         minimized_ ? 0 : pixel_height_, ScaleForDpi(dpi_));
  if (!status.ok()) {
    std::fprintf(stderr, "  [window] surface event rejected: %s\n", status.message().c_str());
    quit_requested_ = true;
  }
}

void GameWindow::MaybeEmitResized(bool force) {
  if (minimized_ || pixel_width_ == 0 || pixel_height_ == 0) {
    return;
  }
  const float scale = ScaleForDpi(dpi_);
  if (!force && pixel_width_ == emitted_width_ && pixel_height_ == emitted_height_ &&
      scale == emitted_scale_) {
    return;
  }
  emitted_width_ = pixel_width_;
  emitted_height_ = pixel_height_;
  emitted_scale_ = scale;
  platform::WindowResizedEvent resized;
  resized.logical_width = static_cast<int>(pixel_width_);
  resized.logical_height = static_cast<int>(pixel_height_);
  resized.pixel_width = static_cast<int>(pixel_width_);
  resized.pixel_height = static_cast<int>(pixel_height_);
  resized.density_scale = scale;
  Emit(resized);
  PushHostEvent(HostWindowEventType::kResized);
}

void GameWindow::OnSize(WPARAM wparam) {
  if (wparam == SIZE_MINIMIZED) {
    if (!minimized_) {
      minimized_ = true;
      PushHostEvent(HostWindowEventType::kMinimized);
    }
    ObserveSurface();
    return;
  }
  const bool restored = minimized_;
  minimized_ = false;
  RefreshClientSize();
  if (restored) {
    PushHostEvent(HostWindowEventType::kRestored);
  }
  ObserveSurface();
  MaybeEmitResized(false);
  pointer_backend_->Reassert();
  if (!text_owner_->OnViewportChanged()) {
    std::fprintf(stderr, "  [input] text-input area reapply failed after resize\n");
  }
  CaptureWindowState();
}

void GameWindow::ApplyIcons() {
  if (hwnd_ == nullptr) {
    return;
  }
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  const struct {
    WPARAM which;
    int metric_x;
    int metric_y;
    HICON* slot;
  } sizes[] = {{ICON_BIG, SM_CXICON, SM_CYICON, &icon_big_},
               {ICON_SMALL, SM_CXSMICON, SM_CYSMICON, &icon_small_}};
  for (const auto& size : sizes) {
    HICON icon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                               GetSystemMetricsForDpi(size.metric_x, dpi_),
                                               GetSystemMetricsForDpi(size.metric_y, dpi_),
                                               LR_DEFAULTCOLOR));
    if (icon == nullptr) {
      continue;
    }
    SendMessageW(hwnd_, WM_SETICON, size.which, reinterpret_cast<LPARAM>(icon));
    if (*size.slot != nullptr) {
      DestroyIcon(*size.slot);
    }
    *size.slot = icon;
  }
}

void GameWindow::OnDpiChanged(WPARAM wparam, const RECT* suggested) {
  const UINT new_dpi = LOWORD(wparam);
  const bool changed = new_dpi != dpi_;
  dpi_ = new_dpi != 0 ? new_dpi : dpi_;
  if (changed) {
    ApplyIcons();
  }
  PublishGeometry();
  if (fullscreen_) {
    FitToMonitor();
  } else if (suggested != nullptr) {
    SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  }
  RefreshClientSize();
  ObserveSurface();
  MaybeEmitResized(false);
  if (changed) {
    PushHostEvent(HostWindowEventType::kDpiChanged);
  }
  if (!text_owner_->OnViewportChanged()) {
    std::fprintf(stderr, "  [input] text-input area reapply failed after DPI change\n");
  }
}

void GameWindow::FitToMonitor() {
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  if (!GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &info)) {
    return;
  }
  SetWindowPos(hwnd_, HWND_TOP, info.rcMonitor.left, info.rcMonitor.top,
               info.rcMonitor.right - info.rcMonitor.left,
               info.rcMonitor.bottom - info.rcMonitor.top,
               SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

POINT GameWindow::ClientCenter() const {
  return POINT{static_cast<LONG>(pixel_width_ / 2), static_cast<LONG>(pixel_height_ / 2)};
}

bool GameWindow::ApplyFullscreen(bool fullscreen) {
  if (hwnd_ == nullptr) {
    return false;
  }
  if (fullscreen == fullscreen_) {
    return true;
  }
  const LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
  const bool was_visible = (style & WS_VISIBLE) != 0;
  if (fullscreen) {
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(hwnd_, &placement)) {
      if (restore_maximized_pending_) {
        placement.showCmd = SW_SHOWMAXIMIZED;
      }
      windowed_placement_ = placement;
      has_windowed_placement_ = true;
    }
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &info)) {
      return false;
    }
    fullscreen_ = true;
    SetWindowLongPtrW(hwnd_, GWL_STYLE,
                      (style & ~static_cast<LONG_PTR>(kWindowedStyle)) | kFullscreenStyle);
    SetWindowPos(hwnd_, HWND_TOP, info.rcMonitor.left, info.rcMonitor.top,
                 info.rcMonitor.right - info.rcMonitor.left,
                 info.rcMonitor.bottom - info.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
  } else {
    fullscreen_ = false;
    SetWindowLongPtrW(hwnd_, GWL_STYLE,
                      (style & ~static_cast<LONG_PTR>(kFullscreenStyle)) | kWindowedStyle);
    if (microphone_menu_ != nullptr && !AttachMicrophoneSystemMenu()) {
      std::fprintf(stderr, "  [window] could not attach the microphone system menu\n");
    }
    if (has_windowed_placement_) {
      WINDOWPLACEMENT placement = windowed_placement_;
      if (!was_visible) {
        placement.showCmd = SW_HIDE;
      } else if (placement.showCmd != SW_SHOWMAXIMIZED) {
        placement.showCmd = SW_SHOWNOACTIVATE;
      } else {
        restore_maximized_pending_ = false;
      }
      SetWindowPlacement(hwnd_, &placement);
    }
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);
  }
  RefreshClientSize();
  ObserveSurface();
  if (surface_.active() && !minimized_ && pixel_width_ >= mwindow::kMinimumWindowWidth &&
      pixel_height_ >= mwindow::kMinimumWindowHeight) {
    const Status refresh = surface_.Refresh();
    if (!refresh.ok()) {
      std::fprintf(stderr, "  [fullscreen] surface refresh rejected: %s\n",
                   refresh.message().c_str());
    }
  }
  MaybeEmitResized(true);
  PushHostEvent(HostWindowEventType::kFullscreenChanged);
  if (!text_owner_->OnViewportChanged()) {
    std::fprintf(stderr, "  [input] text-input area reapply failed after fullscreen change\n");
  }
  pointer_backend_->Reassert();
  std::fprintf(stderr, "  [fullscreen] transition committed state=%s pixels=%ux%u\n",
               fullscreen_ ? "fullscreen" : "windowed", pixel_width_, pixel_height_);
  return true;
}

bool GameWindow::SetFullscreen(bool fullscreen, const char* reason) {
  if (hwnd_ == nullptr) {
    return false;
  }
  if (fullscreen != fullscreen_) {
    CaptureWindowState();
    if (!ApplyFullscreen(fullscreen)) {
      std::fprintf(stderr, "  [fullscreen] request failed\n");
      return false;
    }
    std::fprintf(stderr, "  [fullscreen] %s requested state=%s\n",
                 reason != nullptr ? reason : "toggle", fullscreen ? "fullscreen" : "windowed");
  }
  if (persistence_active_) {
    persisted_.fullscreen = fullscreen;
    persistence_dirty_ = true;
    persistence_change_ns_ = MonotonicNs();
    PersistWindowState();
  }
  if (!fullscreen_sync_.Notify(fullscreen)) {
    std::fprintf(stderr, "  [fullscreen] Roblox settings state synchronization failed\n");
    return false;
  }
  return true;
}

bool GameWindow::ToggleFullscreen(const char* reason) {
  return SetFullscreen(!fullscreen_, reason);
}

bool GameWindow::RequestFullscreenFromAndroidWindowFlags(int flags, int mask) {
  const bool accepted = fullscreen_request_.RequestFromAndroidFlags(flags, mask);
  if (HWND wake = wake_hwnd_.load(std::memory_order_acquire); accepted && wake != nullptr) {
    PostMessageW(wake, WM_NULL, 0, 0);
  }
  return accepted;
}

bool GameWindow::RequestFullscreenFromRobloxMenuLog(const char* tag, const char* message) {
  return fullscreen_menu_request_.RequestFromAndroidLog(tag, message, MonotonicNs());
}

bool GameWindow::SetFullscreenStateSyncCallback(mwindow::FullscreenStateSyncCallback callback,
                                                void* context) {
  return fullscreen_sync_.Register(callback, context);
}

void GameWindow::ClearFullscreenStateSyncCallback() { fullscreen_sync_.Clear(); }

void GameWindow::MaybeApplyFullscreenRequests() {
  if (fullscreen_menu_request_.Take(MonotonicNs()) && !ToggleFullscreen("Roblox menu")) {
    std::fprintf(stderr, "  [fullscreen] rejected Roblox menu toggle request\n");
  }
  bool fullscreen = false;
  if (fullscreen_request_.Take(&fullscreen) && !SetFullscreen(fullscreen, "Roblox settings")) {
    std::fprintf(stderr, "  [fullscreen] rejected Android setWindowFlags request\n");
  }
}

void GameWindow::CaptureWindowState() {
  if (!persistence_active_ || hwnd_ == nullptr || fullscreen_ || minimized_) {
    return;
  }
  WINDOWPLACEMENT placement{};
  placement.length = sizeof(placement);
  if (!GetWindowPlacement(hwnd_, &placement)) {
    return;
  }
  persisted_.maximized =
      restore_maximized_pending_ || placement.showCmd == SW_SHOWMAXIMIZED || IsZoomed(hwnd_);
  if (!persisted_.maximized) {
    RECT window{};
    if (GetWindowRect(hwnd_, &window)) {
      persisted_.x = window.left;
      persisted_.y = window.top;
      persisted_.has_position = true;
    }
    persisted_.width = std::max(mwindow::kMinimumWindowWidth,
                                MulDiv(static_cast<int>(pixel_width_), USER_DEFAULT_SCREEN_DPI,
                                       static_cast<int>(dpi_)));
    persisted_.height = std::max(mwindow::kMinimumWindowHeight,
                                 MulDiv(static_cast<int>(pixel_height_), USER_DEFAULT_SCREEN_DPI,
                                        static_cast<int>(dpi_)));
  }
  persisted_.fullscreen = fullscreen_;
  persistence_dirty_ = true;
  persistence_change_ns_ = MonotonicNs();
}

void GameWindow::PersistWindowState() {
  if (!persistence_active_ || !persistence_dirty_) {
    return;
  }
  const Status status = mwindow::StoreWindowState(options_.state_path, persisted_);
  if (!status.ok()) {
    std::fprintf(stderr, "  [window-state] save failed: %s\n", status.message().c_str());
  }
  persistence_dirty_ = false;
}

bool GameWindow::SetRawMouseRegistered(bool registered) {
  if (registered == raw_input_registered_) {
    return true;
  }
  RAWINPUTDEVICE device{};
  device.usUsagePage = 0x01;
  device.usUsage = 0x02;
  if (registered) {
    device.dwFlags = options_.raw_input_background ? RIDEV_INPUTSINK : 0;
    device.hwndTarget = hwnd_;
  } else {
    device.dwFlags = RIDEV_REMOVE;
    device.hwndTarget = nullptr;
  }
  if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
    if (registered) {
      std::fprintf(stderr,
                   "  [input] Raw Input registration failed (%lu); using cursor "
                   "re-centering for relative motion\n",
                   GetLastError());
    }
    return false;
  }
  raw_input_registered_ = registered;
  return true;
}

bool GameWindow::TextInputActive() const { return text_backend_.active(); }

std::uint32_t GameWindow::ButtonMask() const { return buttons_down_; }

void GameWindow::OnFocus(bool focused) {
  if (focused == focused_) {
    return;
  }
  focused_ = focused;
  if (!focused) {
    text_owner_->OnFocusLost();
    if (!pointer_owner_->OnFocusLost()) {
      std::fprintf(stderr, "  [input] failed to release pointer after focus loss\n");
    }
    if (GetCapture() == hwnd_) {
      ReleaseCapture();
    }
    buttons_down_ = 0;
    ReleaseDeliveredKeys();
    alt_enter_consumed_ = false;
    f11_consumed_ = false;
  } else {
    text_owner_->OnFocusGained();
    if (!pointer_owner_->OnFocusGained(TextInputActive())) {
      std::fprintf(stderr, "  [input] failed to restore pointer after focus gain\n");
    }
    pointer_backend_->OnWindowFocusGained();
  }
  Emit(platform::WindowFocusEvent{focused});
  PushHostEvent(focused ? HostWindowEventType::kFocusGained : HostWindowEventType::kFocusLost);
}

void GameWindow::OnRawInput(LPARAM lparam) {
  UINT size = 0;
  if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, nullptr, &size,
                      sizeof(RAWINPUTHEADER)) != 0 ||
      size == 0) {
    return;
  }
  if (raw_buffer_.size() < size) {
    raw_buffer_.resize(size);
  }
  if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, raw_buffer_.data(), &size,
                      sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) {
    return;
  }
  const auto* raw = reinterpret_cast<const RAWINPUT*>(raw_buffer_.data());
  if (raw->header.dwType != RIM_TYPEMOUSE) {
    return;
  }
  ++stats_.raw_mouse_packets;
  if (raw->header.hDevice == nullptr) {
    ++stats_.raw_mouse_injected_packets;
  }
  if (options_.raw_input_count_tag != 0 &&
      raw->data.mouse.ulExtraInformation == options_.raw_input_count_tag) {
    ++stats_.raw_mouse_tagged_packets;
  }
  if (!pointer_backend_->relative_mode() || !raw_input_registered_) {
    return;
  }
  if (!focused_ && !options_.raw_input_background) {
    return;
  }
  const RAWMOUSE& mouse = raw->data.mouse;
  float delta_x = 0.0F;
  float delta_y = 0.0F;
  if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
    if (mouse.lLastX == 0 && mouse.lLastY == 0) {
      return;
    }
    const bool virtual_desktop = (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
    const int width = GetSystemMetrics(virtual_desktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
    const int height = GetSystemMetrics(virtual_desktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
    if (!RawAbsoluteToRelative(&raw_absolute_, mouse.lLastX, mouse.lLastY, width, height,
                               GetSystemMetrics(SM_REMOTESESSION) != 0, &delta_x, &delta_y)) {
      return;
    }
  } else {
    delta_x = static_cast<float>(mouse.lLastX);
    delta_y = static_cast<float>(mouse.lLastY);
  }
  if (delta_x == 0.0F && delta_y == 0.0F) {
    return;
  }
  const mwindow::PointerPosition pin = RelativePin();
  EmitMotion({pin.x, pin.y, delta_x, delta_y, ButtonMask(), true});
}

mwindow::PointerPosition GameWindow::RelativePin() const {
  const auto& start = pointer_backend_->drag_start();
  if (start && std::isfinite(start->x) && std::isfinite(start->y) &&
      pointer_owner_->release_position() == mwindow::PointerReleasePosition::kDragStart) {
    return {std::clamp(start->x, 0.0F, std::max(0.0F, static_cast<float>(pixel_width_) - 1.0F)),
            std::clamp(start->y, 0.0F, std::max(0.0F, static_cast<float>(pixel_height_) - 1.0F))};
  }
  const POINT center = ClientCenter();
  return {static_cast<float>(center.x), static_cast<float>(center.y)};
}

void GameWindow::OnMouseMove(LPARAM lparam) {
  const float x = static_cast<float>(GET_X_LPARAM(lparam));
  const float y = static_cast<float>(GET_Y_LPARAM(lparam));
  if (pointer_backend_->relative_mode()) {
    if (raw_input_registered_) {
      return;
    }
    POINT center = ClientCenter();
    const float delta_x = x - static_cast<float>(center.x);
    const float delta_y = y - static_cast<float>(center.y);
    if (delta_x == 0.0F && delta_y == 0.0F) {
      return;
    }
    if (focused_) {
      ClientToScreen(hwnd_, &center);
      WarpCursorScreen(center.x, center.y);
    }
    const mwindow::PointerPosition pin = RelativePin();
    EmitMotion({pin.x, pin.y, delta_x, delta_y, ButtonMask(), true});
    return;
  }
  const float delta_x = has_cursor_position_ ? x - cursor_x_ : 0.0F;
  const float delta_y = has_cursor_position_ ? y - cursor_y_ : 0.0F;
  const bool had_position = has_cursor_position_;
  cursor_x_ = x;
  cursor_y_ = y;
  has_cursor_position_ = true;
  if (had_position && delta_x == 0.0F && delta_y == 0.0F) {
    return;
  }
  EmitMotion({x, y, delta_x, delta_y, ButtonMask(), false});
}

void GameWindow::OnMouseButton(std::uint8_t sdl_button, bool pressed, LPARAM lparam) {
  const bool relative = pointer_backend_->relative_mode();
  const float x = static_cast<float>(GET_X_LPARAM(lparam));
  const float y = static_cast<float>(GET_Y_LPARAM(lparam));
  if (!relative) {
    cursor_x_ = x;
    cursor_y_ = y;
    has_cursor_position_ = true;
  }
  const std::uint32_t mask = SDL_BUTTON_MASK(sdl_button);
  if (pressed) {
    if (buttons_down_ == 0 && !relative) {
      SetCapture(hwnd_);
    }
    buttons_down_ |= mask;
  } else {
    buttons_down_ &= ~mask;
    if (buttons_down_ == 0 && GetCapture() == hwnd_) {
      ReleaseCapture();
    }
  }
  platform::MouseButtonEvent event;
  event.pressed = pressed;
  event.button = sdl_button;
  event.clicks = 1;
  event.x = cursor_x_;
  event.y = cursor_y_;
  if (relative) {
    const mwindow::PointerPosition pin = RelativePin();
    event.x = pin.x;
    event.y = pin.y;
  }
  event.relative_mode = relative;
  Emit(event);
  if (sdl_button == SDL_BUTTON_RIGHT &&
      !pointer_owner_->OnRightButton(pressed, TextInputActive(), mwindow::PointerPosition{x, y})) {
    std::fprintf(stderr, "  [input] right-button pointer capture update failed\n");
  }
}

void GameWindow::OnMouseWheel(bool horizontal, WPARAM wparam, LPARAM lparam) {
  const float delta =
      static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / static_cast<float>(WHEEL_DELTA);
  POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
  ScreenToClient(hwnd_, &point);
  platform::MouseWheelEvent event;
  event.delta_x = horizontal ? delta : 0.0F;
  event.delta_y = horizontal ? 0.0F : delta;
  event.mouse_x = static_cast<float>(point.x);
  event.mouse_y = static_cast<float>(point.y);
  event.relative_mode = pointer_backend_->relative_mode();
  Emit(event);
}

LRESULT GameWindow::OnKey(UINT message, WPARAM wparam, LPARAM lparam) {
  DecodedKeyMessage decoded = DecodeKeyMessage(message, wparam, lparam);
  if (!decoded.valid) {
    return DefWindowProcW(hwnd_, message, wparam, lparam);
  }
  const bool sys = message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
  if (decoded.virtual_key == VK_PROCESSKEY) {
    return 0;
  }
  if (fullscreen_ && microphone_menu_ != nullptr && decoded.virtual_key == VK_SPACE &&
      decoded.alt_context && message == WM_SYSKEYDOWN) {
    if (!decoded.repeat) PostMessageW(hwnd_, kShowFullscreenMicrophoneMenu, 0, 0);
    return 0;
  }
  if (decoded.virtual_key == VK_CONTROL && !decoded.extended) {
    const auto current_time = static_cast<DWORD>(GetMessageTime());
    MSG next{};
    if (PeekMessageW(&next, hwnd_, 0, 0, PM_NOREMOVE | PM_NOYIELD) &&
        (decoded.pressed ? (next.message == WM_KEYDOWN || next.message == WM_SYSKEYDOWN)
                         : (next.message == WM_KEYUP || next.message == WM_SYSKEYUP)) &&
        next.wParam == VK_MENU && (next.lParam & (1 << 24)) != 0 && next.time == current_time) {
      return 0;
    }
  }
  const std::uint32_t scancode = decoded.sdl_scancode;
  const bool tracked = scancode != 0 && scancode < keys_down_.size();
  const bool was_down = tracked && keys_down_.test(scancode);
  const bool was_delivered = tracked && keys_delivered_.test(scancode);
  if (tracked) {
    decoded.repeat = decoded.pressed && was_down;
    keys_down_.set(scancode, decoded.pressed);
  }
  if (decoded.virtual_key == VK_RETURN && options_.alt_enter_fullscreen) {
    if (decoded.pressed && decoded.alt_context && message == WM_SYSKEYDOWN) {
      if (!decoded.repeat) {
        (void)ToggleFullscreen("Alt+Enter");
      }
      alt_enter_consumed_ = true;
      return 0;
    }
    if (!decoded.pressed && alt_enter_consumed_) {
      alt_enter_consumed_ = false;
      if (!was_delivered) {
        return 0;
      }
    }
  }
  if (decoded.virtual_key == VK_F11 && options_.f11_fullscreen) {
    if (decoded.pressed) {
      if (!decoded.repeat) {
        (void)ToggleFullscreen("F11");
      }
      f11_consumed_ = true;
      return 0;
    }
    if (f11_consumed_) {
      f11_consumed_ = false;
      if (!was_delivered) {
        return 0;
      }
    }
  }
  const std::uint32_t keycode =
      SdlKeycodeForKey(scancode, decoded.virtual_key, GetKeyboardLayout(0));
  const std::uint32_t modifiers = CurrentSdlModifiers();
  if (!decoded.pressed && !was_down && tracked) {
    if (scancode != SDL_SCANCODE_PRINTSCREEN) {
      return 0;
    }
    DecodedKeyMessage down = decoded;
    down.pressed = true;
    down.repeat = false;
    Emit(MakeKeyEvent(down, keycode, modifiers));
    ++stats_.key_events;
  }
  Emit(MakeKeyEvent(decoded, keycode, modifiers));
  ++stats_.key_events;
  if (tracked) {
    keys_delivered_.set(scancode, decoded.pressed);
    delivered_keycodes_[scancode] = keycode;
  }
  if (decoded.pressed && !decoded.repeat &&
      (scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) &&
      !pointer_owner_->OnShiftKeyPressed(TextInputActive())) {
    std::fprintf(stderr, "  [input] shift-lock pointer capture update failed\n");
  }
  if (sys) {
    if (decoded.pressed && (decoded.virtual_key == VK_F4 || decoded.virtual_key == VK_SPACE)) {
      return DefWindowProcW(hwnd_, message, wparam, lparam);
    }
    return 0;
  }
  return 0;
}

void GameWindow::ReleaseDeliveredKeys() {
  if (keys_delivered_.any()) {
    const std::uint32_t modifiers = CurrentSdlModifiers();
    for (std::size_t scancode = 1; scancode < keys_delivered_.size(); ++scancode) {
      if (!keys_delivered_.test(scancode)) {
        continue;
      }
      platform::KeyEvent release;
      release.pressed = false;
      release.repeat = false;
      release.scancode = static_cast<std::uint32_t>(scancode);
      release.keycode = delivered_keycodes_[scancode];
      release.modifiers = modifiers;
      Emit(release);
      ++stats_.key_events;
    }
  }
  keys_down_.reset();
  keys_delivered_.reset();
}

void GameWindow::OnChar(UINT message, WPARAM wparam) {
  std::optional<platform::TextInputEvent> text;
  text_backend_.HandleChar(message, wparam, &text);
  if (text) {
    ++stats_.text_events;
    Emit(std::move(*text));
  }
}

void GameWindow::OnImeComposition(LPARAM lparam) {
  std::optional<platform::TextEditingEvent> editing;
  std::optional<platform::TextInputEvent> result;
  if (!text_backend_.HandleImeComposition(lparam, &editing, &result)) {
    return;
  }
  if (result) {
    ++stats_.text_events;
    Emit(std::move(*result));
  }
  if (editing) {
    Emit(std::move(*editing));
  }
}

bool GameWindow::SetPlatformEventObserver(mwindow::PlatformEventObserver observer,
                                          void* context) {
  return observer_.Register(observer, context);
}

void GameWindow::ClearPlatformEventObserver() { observer_.Clear(); }

bool GameWindow::SetMouseLockQueryCallback(mwindow::MouseLockQueryCallback callback,
                                           void* context) {
  return pointer_owner_->RegisterQuery(callback, context);
}

void GameWindow::ClearMouseLockQueryCallback() { pointer_owner_->ClearQuery(); }

bool GameWindow::SetPreTextInputPumpCallback(mwindow::MainThreadCommandCallback callback,
                                             void* context) {
  return pre_text_input_gate_.Register(callback, context);
}

void GameWindow::ClearPreTextInputPumpCallback() { pre_text_input_gate_.Clear(); }

bool GameWindow::SetPumpHook(mwindow::MainThreadCommandCallback callback, void* context) {
  return pump_hook_gate_.Register(callback, context);
}

void GameWindow::ClearPumpHook() { pump_hook_gate_.Clear(); }

void GameWindow::SetTextInputOwnerEnabled(bool enabled) { text_owner_->SetEnabled(enabled); }

bool GameWindow::RequestShowTextInput(std::uint64_t generation,
                                      const mwindow::TextInputArea& area,
                                      const mwindow::TextInputOptions& options) {
  return text_owner_->RequestShowTextInput(generation, area, options);
}

bool GameWindow::RequestHideTextInput(std::uint64_t generation) {
  return text_owner_->RequestHideTextInput(generation);
}

}
