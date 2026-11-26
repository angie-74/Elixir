// Modified by Elixir.
#include "runtime/roblox_window_input_runtime.h"

#if defined(_WIN32)
#include "window/win32_game_window.h"
#include "window/win32_text_clipboard.h"
#else
#include <SDL3/SDL_video.h>
#endif

#include <cstdio>
#include <string>
#include <utility>
#include <variant>

#include "window/window.h"

#if defined(_WIN32)
#define MOCKTAIL_INPUT_BACKEND "Win32"
#else
#define MOCKTAIL_INPUT_BACKEND "SDL"
#endif

namespace mocktail {
namespace runtime {

#if defined(_WIN32)
RobloxWindowInputRuntime::RobloxWindowInputRuntime(
    JniEnvironmentProvider environment, RobloxInputSymbols symbols)
    : text_clipboard_(elixir::window::CreateWin32TextClipboard()),
      runtime_(environment, symbols, text_surface_overlay_.sink(),
               text_clipboard_.get()) {}

void RobloxWindowInputRuntime::ConfigureControllers(
    bool enabled, elixir::window::GamepadBackendPreference preference) {
  controllers_enabled_ = enabled;
  controller_preference_ = preference;
}
#else
RobloxWindowInputRuntime::RobloxWindowInputRuntime(
    JniEnvironmentProvider environment, RobloxInputSymbols symbols)
    : text_clipboard_(platform::CreateSdlTextClipboard()),
      runtime_(environment, symbols, text_surface_overlay_.sink(),
               text_clipboard_.get()) {}
#endif

RobloxWindowInputRuntime::~RobloxWindowInputRuntime() { Shutdown(); }

Status RobloxWindowInputRuntime::Initialize() {
  if (observer_registered_) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "window input runtime is already initialized");
  }
  const window::WindowViewportSnapshot window_viewport =
      window::GetWindowViewportSnapshot();
  if (!window_viewport.valid()) {
    return Status::Error(StatusCode::kUnavailable,
                         MOCKTAIL_INPUT_BACKEND " input viewport is unavailable");
  }
#if defined(_WIN32)
  elixir::window::GameWindow* game_window = elixir::window::CurrentGameWindow();
  if (game_window == nullptr || game_window->hwnd() == nullptr) {
    return Status::Error(StatusCode::kUnavailable,
                         "Win32 input window is unavailable");
  }
  const bool initially_focused = game_window->Snapshot().focused;
  Status status = text_surface_overlay_.Initialize(
      {window_viewport.pixel_width, window_viewport.pixel_height,
       window_viewport.dpi_scale});
  if (!status.ok()) {
    return status;
  }
#else
  auto* sdl_window = static_cast<SDL_Window*>(window::GetBackendWindow());
  if (sdl_window == nullptr) {
    return Status::Error(StatusCode::kUnavailable,
                         "SDL input window is unavailable");
  }
  Status status = text_surface_overlay_.Initialize(
      {window_viewport.pixel_width, window_viewport.pixel_height,
       window_viewport.dpi_scale});
  if (!status.ok()) {
    return status;
  }
  const bool initially_focused =
      (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_INPUT_FOCUS) != 0;
#endif
  status = runtime_.Initialize(
      {window_viewport.logical_width, window_viewport.logical_height,
       window_viewport.pixel_width, window_viewport.pixel_height,
       window_viewport.dpi_scale},
      initially_focused);
  if (!status.ok()) {
    (void)text_surface_overlay_.Shutdown();
    return status;
  }
  if (!window::SetPlatformEventObserver(
          &RobloxWindowInputRuntime::PlatformEventCallback, this)) {
    runtime_.Deactivate();
    (void)text_surface_overlay_.Shutdown();
    return Status::Error(StatusCode::kFailedPrecondition,
                         MOCKTAIL_INPUT_BACKEND " platform event observer is unavailable");
  }
  observer_registered_ = true;
  if (!window::SetMouseLockQueryCallback(
          &RobloxWindowInputRuntime::MouseLockQueryCallback, this)) {
    window::ClearPlatformEventObserver();
    observer_registered_ = false;
    runtime_.Deactivate();
    (void)text_surface_overlay_.Shutdown();
    return Status::Error(StatusCode::kFailedPrecondition,
                         MOCKTAIL_INPUT_BACKEND " mouse lock query owner is unavailable");
  }
  mouse_lock_query_registered_ = true;
  initialized_ = true;
#if defined(_WIN32)
  const Status overlay_window_status = text_overlay_window_.Start(
      game_window->hwnd(), &text_surface_overlay_, window_viewport.dpi_scale);
  if (!overlay_window_status.ok()) {
    std::fprintf(stderr, "  [input] TextBox overlay window unavailable: %s\n",
                 overlay_window_status.message().c_str());
  }
  if (overlay_window_status.ok()) {
    char value[4] = {};
    if (GetEnvironmentVariableA("ELIXIR_TEXT_OVERLAY_TEST", value, sizeof(value)) == 1 && value[0] == '1') {
      static const char kText[] = "Elixir text overlay test";
      RobloxTextDisplayUpdate update;
      update.event = RobloxTextDisplayEvent::kShow;
      update.generation = 0x7fffffffu;
      update.area_x = 120;
      update.area_y = 160;
      update.area_width = 420;
      update.area_height = 18;
      update.font_size = 20.0F;
      update.text_color = static_cast<int32_t>(0xFFFF4040u);
      update.utf8 = kText;
      update.utf8_size = sizeof(kText) - 1;
      update.cursor_utf16 = static_cast<int32_t>(sizeof(kText) - 1);
      update.selection_begin_utf16 = update.cursor_utf16;
      update.selection_end_utf16 = update.cursor_utf16;
      const RobloxTextDisplaySink sink = text_surface_overlay_.sink();
      sink.update(sink.context, update);
      std::fprintf(stderr, "  [input] TextBox overlay test text shown (ELIXIR_TEXT_OVERLAY_TEST=1)\n");
    }
  }
#endif
  if (runtime_.SupportsGamepads()) {
#if defined(_WIN32)
    elixir::window::GamepadManagerOptions options;
    options.enabled = controllers_enabled_;
    options.preference = controller_preference_;
    const Status gamepad_status = gamepads_.Initialize(
        this, &RobloxWindowInputRuntime::GamepadEventCallback,
        std::move(options));
    if (gamepad_status.ok()) {
      seen_device_changes_ = game_window->device_change_count();
      pump_hook_registered_ = game_window->SetPumpHook(
          &RobloxWindowInputRuntime::PumpHookCallback, this);
      pump_hook_window_ = pump_hook_registered_ ? game_window : nullptr;
      if (!pump_hook_registered_) {
        std::fprintf(stderr,
                     "  [gamepad] window pump hook is taken; controllers are not polled\n");
        gamepads_.Shutdown();
      }
    }
#else
    const Status gamepad_status = gamepads_.Initialize(
        this, &RobloxWindowInputRuntime::GamepadEventCallback);
#endif
    if (!gamepad_status.ok()) {
      std::fprintf(stderr, "  [gamepad] unavailable: %s\n",
                   gamepad_status.message().c_str());
    }
  }
  return Status::Ok();
}

#if defined(_WIN32)
bool RobloxWindowInputRuntime::PumpHookCallback(void* context) {
  auto* self = static_cast<RobloxWindowInputRuntime*>(context);
  elixir::window::GameWindow* game_window = elixir::window::CurrentGameWindow();
  if (game_window != nullptr) {
    const std::uint64_t changes = game_window->device_change_count();
    if (changes != self->seen_device_changes_) {
      self->seen_device_changes_ = changes;
      self->gamepads_.NotifyDeviceChange();
    }
    const bool focused = game_window->has_input_focus();
    if (self->runtime_.Snapshot().focused != focused) {
      (void)self->runtime_.HandleEvent(
          {elixir::window::GameWindow::MonotonicNs(),
           platform::WindowFocusEvent{focused}});
    }
    if (focused && !self->controller_focus_) {
      self->gamepads_.ResendState();
    }
    self->controller_focus_ = focused;
  }
  self->gamepads_.Poll(elixir::window::GameWindow::MonotonicNs());
  return true;
}
#endif

void RobloxWindowInputRuntime::GamepadEventCallback(
    void* context, const platform::PlatformEvent& event) {
  auto* self = static_cast<RobloxWindowInputRuntime*>(context);
  (void)self->runtime_.HandleEvent(event);
}

void RobloxWindowInputRuntime::PlatformEventCallback(
    void* context, const platform::PlatformEvent& event) {
  if (context == nullptr) {
    return;
  }
  auto* self = static_cast<RobloxWindowInputRuntime*>(context);
  if (self->gamepads_.HandleEvent(event)) {
    return;
  }
  if (const auto* resized =
          std::get_if<platform::WindowResizedEvent>(&event.payload)) {
    (void)self->text_surface_overlay_.UpdateViewport(
        {resized->pixel_width, resized->pixel_height, resized->density_scale});
#if defined(_WIN32)
    self->text_overlay_window_.UpdateDensity(resized->density_scale);
#endif
  }
  (void)self->runtime_.HandleEvent(event);
#if !defined(_WIN32)
  if (const auto* focus =
          std::get_if<platform::WindowFocusEvent>(&event.payload)) {
    if (focus->focused) {
      self->gamepads_.ResendState();
    }
  }
#endif
}

Status RobloxWindowInputRuntime::Shutdown() {
  if (!initialized_) {
    return Status::Ok();
  }
  if (observer_registered_) {
    window::ClearPlatformEventObserver();
    observer_registered_ = false;
  }
  if (mouse_lock_query_registered_) {
    window::ClearMouseLockQueryCallback();
    mouse_lock_query_registered_ = false;
  }
#if defined(_WIN32)
  if (pump_hook_registered_) {
    if (elixir::window::GameWindow* game_window =
            elixir::window::CurrentGameWindow();
        game_window != nullptr && game_window == pump_hook_window_) {
      game_window->ClearPumpHook();
    }
    pump_hook_registered_ = false;
    pump_hook_window_ = nullptr;
  }
#endif
  gamepads_.Shutdown();
  const RobloxInputSnapshot snapshot = runtime_.Snapshot();
  Status status = runtime_.Deactivate();
#if defined(_WIN32)
  text_overlay_window_.Stop();
#endif
  Status overlay_status = text_surface_overlay_.Shutdown();
  initialized_ = false;
  if (!status.ok()) {
    return status;
  }
  if (!overlay_status.ok()) {
    return overlay_status;
  }
  if (snapshot.native_errors != 0) {
    return Status::Error(StatusCode::kPlatformError,
                         "Roblox native input reported " +
                             std::to_string(snapshot.native_errors) +
                             " dispatch error(s)");
  }
  std::fprintf(stderr,
               "  [input] typed production input stopped after observer "
               "clear\n");
  return Status::Ok();
}

Status RobloxWindowInputRuntime::BeginTextFocusSession(
    RobloxTextFocusSession session) {
  if (!initialized_) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "window input runtime is not initialized");
  }
  return runtime_.BeginTextFocusSession(std::move(session));
}

Status RobloxWindowInputRuntime::EndTextFocusSession(int64_t textbox_handle,
                                                     uint64_t generation,
                                                     bool notify_native) {
  if (!initialized_) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "window input runtime is not initialized");
  }
  return runtime_.EndTextFocusSession(textbox_handle, generation,
                                      notify_native);
}

Status RobloxWindowInputRuntime::ReplaceFocusedTextFromEngine(
    uint64_t generation, std::string authoritative_utf8) {
  if (!initialized_) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "window input runtime is not initialized");
  }
  return runtime_.ReplaceFocusedTextFromEngine(generation,
                                               std::move(authoritative_utf8));
}

Status RobloxWindowInputRuntime::QueryCurrentTextBoxInfo(
    RobloxNativeTextBoxInfoQueryResult* result) {
  if (!initialized_) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "window input runtime is not initialized");
  }
  return runtime_.QueryCurrentTextBoxInfo(result);
}

Status RobloxWindowInputRuntime::UpdateTextFocusProperties(
    uint64_t generation, const RobloxTextFocusProperties& properties) {
  if (!initialized_) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "window input runtime is not initialized");
  }
  return runtime_.UpdateTextFocusProperties(generation, properties);
}

RobloxInputSnapshot RobloxWindowInputRuntime::Snapshot() const {
  return runtime_.Snapshot();
}

bool RobloxWindowInputRuntime::MouseLockQueryCallback(void* context,
                                                      bool* locked_center) {
  if (context == nullptr || locked_center == nullptr) {
    return false;
  }
  return static_cast<RobloxWindowInputRuntime*>(context)
      ->runtime_.GetMainWindowIsMouseLockedCenter(locked_center)
      .ok();
}

}
}
