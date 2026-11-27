// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail's stubs/libandroid_stub.cc, libegl_stub.cc,
// libgles_stub.cc and libmediandk_stub.cc (Apache-2.0, Copyright 2026 Mocktail
// Project Authors). Modified by Elixir.
#ifndef ELIXIR_ANDROID_ANDROID_H_
#define ELIXIR_ANDROID_ANDROID_H_

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/render_scale.h"

struct ANativeWindow;
struct AAssetManager;
struct AInputQueue;
struct ALooper;

namespace elixir::bionic {
class ExportTable;
}

namespace elixir::android {

inline constexpr const char* kLibandroid = "libandroid.so";
inline constexpr const char* kLibEgl = "libEGL.so";
inline constexpr const char* kLibGlesV2 = "libGLESv2.so";
inline constexpr const char* kLibGlesV3 = "libGLESv3.so";
inline constexpr const char* kLibMediandk = "libmediandk.so";

void RegisterAndroidExports(bionic::ExportTable& table);

std::size_t InstallAndroidLibraries();

ANativeWindow* CreateNativeWindow(HWND hwnd, int width, int height);
void ResizeNativeWindow(ANativeWindow* window, int width, int height);
void DestroyNativeWindow(ANativeWindow* window);
void AssociateSurface(const void* java_surface, ANativeWindow* window);
void SetSurfaceRenderScale(const void* java_surface, const RenderScale& scale);
void SetNativeWindowRenderScale(ANativeWindow* window, const RenderScale& scale);
void ResetSurfaceRenderScales();

struct NativeWindowInfo {
  HWND hwnd = nullptr;
  int width = 0;
  int height = 0;
  int format = 0;
  int stride = 0;
  int references = 0;
  bool abandoned = false;
  bool locked = false;
  std::uint64_t posted_frames = 0;
  std::uint64_t presented_frames = 0;
};
bool GetNativeWindowInfo(ANativeWindow* window, NativeWindowInfo* info);
std::size_t LiveNativeWindowCount();

struct AssetSources {
  std::vector<std::string> directories;
  std::vector<std::string> archives;
  bool transcode_etc1_sky_for_vulkan = true;
  std::string overlay_directory;
  bool (*overlay_filter)(std::string_view name) = nullptr;
};

bool SetAssetSources(const AssetSources& sources, std::string* error = nullptr);

struct AssetReadFailures {
  std::uint64_t extracted = 0;
  std::uint64_t archive = 0;
};

AssetReadFailures GetAssetReadFailures() noexcept;

AAssetManager* GetAssetManager();

struct DeviceConfiguration {
  char language[2] = {'e', 'n'};
  char country[2] = {'U', 'S'};
  std::int32_t mcc = 0;
  std::int32_t mnc = 0;
  std::int32_t orientation = 0;
  std::int32_t touchscreen = 0;
  std::int32_t density = 0;
  std::int32_t keyboard = 0;
  std::int32_t navigation = 0;
  std::int32_t keys_hidden = 0;
  std::int32_t nav_hidden = 0;
  std::int32_t sdk_version = 0;
  std::int32_t screen_size = 0;
  std::int32_t screen_long = 0;
  std::int32_t screen_round = 0;
  std::int32_t ui_mode_type = 0;
  std::int32_t ui_mode_night = 0;
  std::int32_t screen_width_dp = 0;
  std::int32_t screen_height_dp = 0;
  std::int32_t smallest_screen_width_dp = 0;
  std::int32_t layout_direction = 0;
  std::int32_t grammatical_gender = 0;
};

DeviceConfiguration QueryWindowsConfiguration(HWND hwnd = nullptr, int width = 0, int height = 0);
void SetDeviceConfiguration(const DeviceConfiguration& configuration);
void ClearDeviceConfiguration();

struct VsyncInfo {
  std::int64_t period_ns = 0;
  const char* source = "";
  std::uint64_t vsync_count = 0;
};
VsyncInfo GetVsyncInfo();

struct KeyEventData {
  std::int32_t device_id = 0;
  std::int32_t source = 0x101;
  std::int32_t action = 0;
  std::int32_t flags = 0;
  std::int32_t key_code = 0;
  std::int32_t scan_code = 0;
  std::int32_t meta_state = 0;
  std::int32_t repeat_count = 0;
  std::int64_t down_time = 0;
  std::int64_t event_time = 0;
};
struct PointerData {
  std::int32_t id = 0;
  std::int32_t tool_type = 0;
  float x = 0, y = 0, raw_x = 0, raw_y = 0;
  float pressure = 0, size = 0, touch_major = 0, touch_minor = 0;
  float tool_major = 0, tool_minor = 0, orientation = 0;
  float axis_vscroll = 0, axis_hscroll = 0;
};
struct MotionEventData {
  std::int32_t device_id = 0;
  std::int32_t source = 0x1002;
  std::int32_t action = 0;
  std::int32_t action_button = 0;
  std::int32_t flags = 0;
  std::int32_t meta_state = 0;
  std::int32_t button_state = 0;
  std::int32_t edge_flags = 0;
  std::int32_t classification = 0;
  float x_offset = 0, y_offset = 0, x_precision = 1, y_precision = 1;
  std::int64_t down_time = 0;
  std::int64_t event_time = 0;
  std::vector<PointerData> pointers;
};
AInputQueue* CreateInputQueue();
void DestroyInputQueue(AInputQueue* queue);
bool PostKeyEvent(AInputQueue* queue, const KeyEventData& event);
bool PostMotionEvent(AInputQueue* queue, const MotionEventData& event);

struct GuestPollfd {
  std::int32_t fd;
  std::int16_t events;
  std::int16_t revents;
};
using FdWaitFn = int (*)(GuestPollfd* fds, std::size_t count, HANDLE wake_event, int timeout_ms,
                         int* guest_errno) noexcept;
void SetLooperFdWait(FdWaitFn wait);

}

#endif
