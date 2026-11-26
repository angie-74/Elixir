// Modified by Elixir.
#ifndef MOCKTAIL_WINDOW_WINDOW_POINTER_CAPTURE_OWNER_H_
#define MOCKTAIL_WINDOW_WINDOW_POINTER_CAPTURE_OWNER_H_

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>

namespace mocktail {
namespace window {

using MouseLockQueryCallback = bool (*)(void* context, bool* locked_center);

struct PointerPosition {
  float x = 0.0f;
  float y = 0.0f;
};

enum class PointerReleasePosition { kDragStart, kWindowCenter, kUnchanged };

struct PointerCaptureRequest {
  bool relative_mode = false;
  bool cursor_visible = true;
  std::optional<PointerPosition> anchor;
  PointerReleasePosition release_position = PointerReleasePosition::kUnchanged;
};

class PointerCaptureBackend {
 public:
  virtual ~PointerCaptureBackend() = default;
  virtual bool Apply(const PointerCaptureRequest& request) = 0;
};

class WindowPointerCaptureOwner final {
 public:
  explicit WindowPointerCaptureOwner(PointerCaptureBackend* backend);
  ~WindowPointerCaptureOwner();

  WindowPointerCaptureOwner(const WindowPointerCaptureOwner&) = delete;
  WindowPointerCaptureOwner& operator=(const WindowPointerCaptureOwner&) =
      delete;

  bool RegisterQuery(MouseLockQueryCallback callback, void* context);
  void ClearQuery();
  bool Pump(bool text_input_active);
  bool OnRightButton(bool pressed, bool text_input_active,
                     std::optional<PointerPosition> position = std::nullopt);
  bool OnShiftKeyPressed(bool text_input_active);
  bool NeedsRightButtonReleaseRecovery(bool observed_pressed) const;
  bool ShouldDispatchMouseMotion();
  bool OnFocusGained(bool text_input_active);
  bool OnFocusLost();
  bool Shutdown();

  bool captured() const { return captured_; }
  bool cursor_visible() const { return cursor_visible_; }
  PointerReleasePosition release_position() const { return release_position_; }

 private:
  bool Apply(bool capture, bool cursor_visible,
             PointerReleasePosition release_position);

  PointerCaptureBackend* backend_ = nullptr;
  std::mutex mutex_;
  std::condition_variable condition_;
  MouseLockQueryCallback callback_ = nullptr;
  void* context_ = nullptr;
  std::size_t in_flight_ = 0;
  bool clearing_ = false;
  bool focused_ = true;
  bool right_button_held_ = false;
  std::optional<PointerPosition> right_drag_anchor_;
  PointerReleasePosition release_position_ = PointerReleasePosition::kUnchanged;
  bool shift_key_pressed_during_right_drag_ = false;
  bool captured_ = false;
  bool cursor_visible_ = true;
  bool state_valid_ = true;
};

#if !defined(_WIN32)
class SdlPointerCaptureBackend final : public PointerCaptureBackend {
 public:
  explicit SdlPointerCaptureBackend(void* window) : window_(window) {}

  bool Apply(const PointerCaptureRequest& request) override;

 private:
  void* window_ = nullptr;
  std::optional<PointerPosition> drag_start_;
  bool relative_mode_ = false;
};
#endif

}
}

#endif
