// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_ROBLOX_WINDOW_INPUT_RUNTIME_H_
#define MOCKTAIL_RUNTIME_ROBLOX_WINDOW_INPUT_RUNTIME_H_

#include <cstdint>
#include <memory>

#if defined(_WIN32)
#include "window/win32_gamepad.h"
#include "window/win32_text_overlay_window.h"
namespace elixir::window {
class GameWindow;
}
#else
#include "mocktail/platform/sdl_gamepad_manager.h"
#endif
#include "mocktail/platform/text_clipboard.h"
#include "mocktail/status.h"
#include "runtime/roblox_input_native_adapter.h"
#include "runtime/roblox_text_surface_overlay.h"

namespace mocktail {
namespace runtime {

class RobloxWindowInputRuntime final {
 public:
  RobloxWindowInputRuntime(JniEnvironmentProvider environment,
                           RobloxInputSymbols symbols);
  ~RobloxWindowInputRuntime();

  RobloxWindowInputRuntime(const RobloxWindowInputRuntime&) = delete;
  RobloxWindowInputRuntime& operator=(const RobloxWindowInputRuntime&) = delete;

#if defined(_WIN32)
  void ConfigureControllers(bool enabled,
                            elixir::window::GamepadBackendPreference preference =
                                elixir::window::GamepadBackendPreference::kAuto);
#endif

  Status Initialize();
  Status Shutdown();
  Status BeginTextFocusSession(RobloxTextFocusSession session);
  Status EndTextFocusSession(int64_t textbox_handle, uint64_t generation,
                             bool notify_native);
  Status ReplaceFocusedTextFromEngine(uint64_t generation,
                                      std::string authoritative_utf8);
  Status QueryCurrentTextBoxInfo(RobloxNativeTextBoxInfoQueryResult* result);
  Status UpdateTextFocusProperties(uint64_t generation,
                                   const RobloxTextFocusProperties& properties);
  RobloxInputSnapshot Snapshot() const;

 private:
  static void PlatformEventCallback(void* context,
                                    const platform::PlatformEvent& event);
  static bool MouseLockQueryCallback(void* context, bool* locked_center);
  static void GamepadEventCallback(void* context,
                                   const platform::PlatformEvent& event);

#if defined(_WIN32)
  static bool PumpHookCallback(void* context);

  RobloxTextSurfaceOverlay text_surface_overlay_;
  elixir::window::Win32TextOverlayWindow text_overlay_window_;
  std::unique_ptr<platform::TextClipboard> text_clipboard_;
  RobloxInputRuntime runtime_;
  elixir::window::Win32GamepadManager gamepads_;
  bool controllers_enabled_ = true;
  elixir::window::GamepadBackendPreference controller_preference_ =
      elixir::window::GamepadBackendPreference::kAuto;
  std::uint64_t seen_device_changes_ = 0;
  bool controller_focus_ = false;
  bool pump_hook_registered_ = false;
  elixir::window::GameWindow* pump_hook_window_ = nullptr;
#else
  RobloxTextSurfaceOverlay text_surface_overlay_;
  std::unique_ptr<platform::TextClipboard> text_clipboard_;
  RobloxInputRuntime runtime_;
  platform::SdlGamepadManager gamepads_;
#endif
  bool observer_registered_ = false;
  bool mouse_lock_query_registered_ = false;
  bool initialized_ = false;
};

}
}

#endif
