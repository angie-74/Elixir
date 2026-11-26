// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_DEVICE_PROFILE_H_
#define MOCKTAIL_RUNTIME_DEVICE_PROFILE_H_

#include <cstddef>
#include <string>
#include <string_view>

namespace mocktail {
namespace runtime {

enum class DeviceClass {
  kPc,
  kMobile,
  kConsole,
};

struct DeviceProfile {
  std::string name;
  std::string cache_key;
  DeviceClass device_class = DeviceClass::kPc;
  std::string platform_name;
  std::string display_name;
  std::string manufacturer;
  std::string model;
  std::string brand;
  std::string device_code;
  std::string device_sku;
  std::string soc_model;
  std::string roblox_http_user_agent;
  bool touch_enabled = false;
  bool mouse_enabled = true;
  bool keyboard_enabled = true;
  bool pc_hardware = true;
};

inline constexpr std::string_view kDefaultDeviceProfileName = "pc-windows-11";
inline constexpr std::string_view kRobloxDesktopHttpUserAgent =
    "Roblox/WinInet";
inline constexpr std::string_view kRobloxConsoleAdmissionUserAgent =
    "Roblox/XboxOne";

const DeviceProfile* FindDeviceProfile(std::string_view name);
#if defined(_WIN32)
bool IsDeviceProfileAvailable(std::string_view name, std::string* reason);
#endif
bool IsValidDeviceProfileValue(std::string_view value, std::size_t maximum);
std::string BuildCustomDeviceProfileCacheKey(const DeviceProfile& profile);
std::string_view DeviceClassName(DeviceClass device_class);

}
}

#endif
