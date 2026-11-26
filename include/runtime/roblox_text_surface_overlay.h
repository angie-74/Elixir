// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_ROBLOX_TEXT_SURFACE_OVERLAY_H_
#define MOCKTAIL_RUNTIME_ROBLOX_TEXT_SURFACE_OVERLAY_H_

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "mocktail/graphics/text_overlay_frame.h"
#include "mocktail/status.h"
#include "runtime/roblox_text_display_state.h"

namespace mocktail {
namespace runtime {

class RobloxTextSurfaceOverlay final {
 public:
  RobloxTextSurfaceOverlay() = default;
  ~RobloxTextSurfaceOverlay();

  RobloxTextSurfaceOverlay(const RobloxTextSurfaceOverlay&) = delete;
  RobloxTextSurfaceOverlay& operator=(const RobloxTextSurfaceOverlay&) = delete;

  Status Initialize(RobloxTextOverlayViewport viewport);
  Status UpdateViewport(RobloxTextOverlayViewport viewport);
  Status Shutdown();
  RobloxTextDisplaySink sink();

  bool QueryFrame(MocktailTextOverlayFrameInfo* frame);
  bool CopyFrame(std::uint64_t revision, void* rgba, std::size_t rgba_capacity);

#if defined(_WIN32)
  Status SetRasterScale(float scale);
  using ChangeCallback = void (*)(void* context);
  void SetChangeCallback(ChangeCallback callback, void* context);
#endif

 private:
  static void UpdateCallback(void* context,
                             const RobloxTextDisplayUpdate& update);
  void ApplyUpdate(const RobloxTextDisplayUpdate& update);
  Status RasterizeLocked();
  void ClearFrameLocked();
  void RecordFailureLocked(Status status);

  std::mutex mutex_;
  RobloxTextDisplayState state_;
  RobloxTextOverlayViewport viewport_;
  std::vector<std::uint8_t> rgba_;
  std::uint64_t state_revision_ = 0;
  std::uint64_t raster_revision_ = 0;
  std::uint32_t raster_height_ = 0;
  std::uint32_t raster_rows_above_ = 0;
  Status failure_;
  bool initialized_ = false;
  bool failure_logged_ = false;
  bool ready_logged_ = false;
#if defined(_WIN32)
  float raster_scale_ = 1.0F;
  ChangeCallback change_callback_ = nullptr;
  void* change_context_ = nullptr;
#endif
};

}
}

#endif
