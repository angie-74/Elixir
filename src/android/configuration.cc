// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail stubs/libandroid_stub.cc (Apache-2.0, Copyright 2026
// Mocktail Project Authors). Modified by Elixir.
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>

#include "android/android_internal.h"
#include "bionic/runtime.h"

struct AConfiguration {
  elixir::android::DeviceConfiguration values;
};

namespace elixir::android {
namespace {

constexpr std::int32_t kAny = 0;
constexpr std::int32_t kOrientationPort = 1, kOrientationLand = 2;
constexpr std::int32_t kTouchNone = 1, kTouchFinger = 3;
constexpr std::int32_t kDensityMedium = 160;
constexpr std::int32_t kDensityAny = 0xfffe, kDensityNone = 0xffff;
constexpr std::int32_t kKeyboardQwerty = 2;
constexpr std::int32_t kNavigationNone = 1;
constexpr std::int32_t kHiddenNo = 1;
constexpr std::int32_t kScreenSmall = 1, kScreenNormal = 2, kScreenLarge = 3, kScreenXlarge = 4;
constexpr std::int32_t kNo = 1, kYes = 2;
constexpr std::int32_t kUiModeNormal = 1;
constexpr std::int32_t kLayoutLtr = 1, kLayoutRtl = 2;

struct OverrideState {
  std::mutex mutex;
  std::optional<DeviceConfiguration> configuration;
};
OverrideState& GetOverride() {
  static OverrideState* state = new OverrideState();
  return *state;
}

void PackCode(const std::string& code, char base, char out[2]) {
  out[0] = out[1] = 0;
  if (code.size() == 2) {
    out[0] = code[0];
    out[1] = code[1];
  } else if (code.size() == 3) {
    const int c0 = code[0] - base, c1 = code[1] - base, c2 = code[2] - base;
    out[0] = static_cast<char>(0x80 | ((c2 & 0x1f) << 2) | ((c1 & 0x18) >> 3));
    out[1] = static_cast<char>(((c1 & 0x07) << 5) | (c0 & 0x1f));
  }
}

void PackGuestCode(const char* code, char base, char out[2]) {
  if (code == nullptr || code[0] == 0) {
    out[0] = out[1] = 0;
    return;
  }
  if (code[1] == 0 || code[2] == 0 || code[2] == '-') {
    out[0] = code[0];
    out[1] = code[1];
    return;
  }
  PackCode(std::string(code, 3), base, out);
}

void ParseLocale(DeviceConfiguration* config) {
  wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
  if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) <= 0) return;
  std::string tag;
  for (const wchar_t* p = name; *p != 0; ++p) tag.push_back(*p < 0x80 ? static_cast<char>(*p) : '?');
  std::string language;
  std::string region;
  std::size_t start = 0;
  for (int index = 0; start <= tag.size(); ++index) {
    const std::size_t end = tag.find('-', start);
    std::string part = tag.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (index == 0) {
      for (char& c : part) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
      language = part;
    } else if (region.empty() && part.size() == 2 && std::isalpha(static_cast<unsigned char>(part[0])) &&
               std::isalpha(static_cast<unsigned char>(part[1]))) {
      for (char& c : part) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
      region = part;
    } else if (region.empty() && part.size() == 3 && std::isdigit(static_cast<unsigned char>(part[0]))) {
      region = part;
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  if (language.size() == 2 || language.size() == 3) PackCode(language, 'a', config->language);
  if (!region.empty()) PackCode(region, '0', config->country);
  static const char* const kRtl[] = {"ar", "fa", "he", "iw", "ur", "ps", "sd", "ug", "yi", "dv", "ku"};
  config->layout_direction = kLayoutLtr;
  for (const char* rtl : kRtl) {
    if (language == rtl) config->layout_direction = kLayoutRtl;
  }
}

bool AppsUseDarkTheme() {
  DWORD value = 1;
  DWORD size = sizeof(value);
  if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
    return false;
  }
  return value == 0;
}

DeviceConfiguration CurrentConfiguration() {
  {
    OverrideState& state = GetOverride();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.configuration.has_value()) return *state.configuration;
  }
  HWND hwnd = nullptr;
  int width = 0, height = 0;
  PrimaryWindowGeometry(&hwnd, &width, &height);
  return QueryWindowsConfiguration(hwnd, width, height);
}

}

DeviceConfiguration QueryWindowsConfiguration(HWND hwnd, int width, int height) {
  DeviceConfiguration config;
  config.language[0] = 'e';
  config.language[1] = 'n';
  config.country[0] = 'U';
  config.country[1] = 'S';
  ParseLocale(&config);

  UINT dpi = hwnd != nullptr ? GetDpiForWindow(hwnd) : 0;
  if (dpi == 0) dpi = GetDpiForSystem();
  if (dpi == 0) dpi = USER_DEFAULT_SCREEN_DPI;
  config.density = static_cast<std::int32_t>(std::lround(dpi * 160.0 / USER_DEFAULT_SCREEN_DPI));

  if (width <= 0 || height <= 0) {
    HMONITOR monitor = hwnd != nullptr ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY)
                                       : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &info)) {
      width = info.rcWork.right - info.rcWork.left;
      height = info.rcWork.bottom - info.rcWork.top;
    }
  }
  if (width <= 0 || height <= 0) {
    width = 1280;
    height = 720;
  }
  config.screen_width_dp = width * kDensityMedium / config.density;
  config.screen_height_dp = height * kDensityMedium / config.density;
  const std::int32_t long_dp = std::max(config.screen_width_dp, config.screen_height_dp);
  const std::int32_t short_dp = std::min(config.screen_width_dp, config.screen_height_dp);
  config.smallest_screen_width_dp = short_dp;
  config.orientation = width >= height ? kOrientationLand : kOrientationPort;
  if (long_dp >= 960 && short_dp >= 720) {
    config.screen_size = kScreenXlarge;
  } else if (long_dp >= 640 && short_dp >= 480) {
    config.screen_size = kScreenLarge;
  } else if (long_dp >= 470 && short_dp >= 320) {
    config.screen_size = kScreenNormal;
  } else {
    config.screen_size = kScreenSmall;
  }
  config.screen_long = long_dp * 3 >= short_dp * 5 ? kYes : kNo;
  config.screen_round = kNo;

  const int digitizer = GetSystemMetrics(SM_DIGITIZER);
  config.touchscreen = (digitizer & NID_READY) != 0 && (digitizer & (NID_INTEGRATED_TOUCH | NID_EXTERNAL_TOUCH)) != 0
                           ? kTouchFinger
                           : kTouchNone;
  config.keyboard = kKeyboardQwerty;
  config.keys_hidden = kHiddenNo;
  config.navigation = kNavigationNone;
  config.nav_hidden = kHiddenNo;
  config.ui_mode_type = kUiModeNormal;
  config.ui_mode_night = AppsUseDarkTheme() ? kYes : kNo;
  config.sdk_version = bionic::Options().sdk_version;
  config.mcc = 0;
  config.mnc = 0;
  config.grammatical_gender = kAny;
  return config;
}

void SetDeviceConfiguration(const DeviceConfiguration& configuration) {
  OverrideState& state = GetOverride();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.configuration = configuration;
}

void ClearDeviceConfiguration() {
  OverrideState& state = GetOverride();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.configuration.reset();
}

namespace {

bool Conflicts(std::int32_t base, std::int32_t requested) { return base != kAny && requested != kAny && base != requested; }
bool Exceeds(std::int32_t base, std::int32_t requested) { return base != kAny && requested != kAny && base > requested; }

bool LocaleConflicts(const DeviceConfiguration& base, const DeviceConfiguration& requested) {
  if (base.language[0] != 0 && requested.language[0] != 0 &&
      std::memcmp(base.language, requested.language, 2) != 0) {
    return true;
  }
  return base.country[0] != 0 && requested.country[0] != 0 && std::memcmp(base.country, requested.country, 2) != 0;
}

int Match(const DeviceConfiguration& base, const DeviceConfiguration& requested) {
  if (Conflicts(base.mcc, requested.mcc) || Conflicts(base.mnc, requested.mnc) || LocaleConflicts(base, requested) ||
      Conflicts(base.layout_direction, requested.layout_direction) ||
      Exceeds(base.smallest_screen_width_dp, requested.smallest_screen_width_dp) ||
      Exceeds(base.screen_width_dp, requested.screen_width_dp) ||
      Exceeds(base.screen_height_dp, requested.screen_height_dp) || Exceeds(base.screen_size, requested.screen_size) ||
      Conflicts(base.screen_long, requested.screen_long) || Conflicts(base.screen_round, requested.screen_round) ||
      Conflicts(base.orientation, requested.orientation) || Conflicts(base.ui_mode_type, requested.ui_mode_type) ||
      Conflicts(base.ui_mode_night, requested.ui_mode_night) || Conflicts(base.touchscreen, requested.touchscreen) ||
      Conflicts(base.keys_hidden, requested.keys_hidden) || Conflicts(base.keyboard, requested.keyboard) ||
      Conflicts(base.nav_hidden, requested.nav_hidden) || Conflicts(base.navigation, requested.navigation) ||
      Exceeds(base.sdk_version, requested.sdk_version) ||
      Conflicts(base.grammatical_gender, requested.grammatical_gender)) {
    return 0;
  }
  return 1;
}

int Specific(std::int32_t base, std::int32_t test, std::int32_t requested) {
  if (requested == kAny || (base != kAny) == (test != kAny)) return 0;
  return base != kAny ? 1 : -1;
}
int Larger(std::int32_t base, std::int32_t test, std::int32_t requested) {
  if (requested == kAny || base == test) return 0;
  return base > test ? 1 : -1;
}

int IsBetterThan(const DeviceConfiguration& base, const DeviceConfiguration& test,
                 const DeviceConfiguration& requested) {
  const int decisions[] = {
      Specific(base.mcc, test.mcc, requested.mcc),
      Specific(base.mnc, test.mnc, requested.mnc),
      Specific(base.language[0], test.language[0], requested.language[0]),
      Specific(base.country[0], test.country[0], requested.country[0]),
      Specific(base.layout_direction, test.layout_direction, requested.layout_direction),
      Larger(base.smallest_screen_width_dp, test.smallest_screen_width_dp, requested.smallest_screen_width_dp),
      Larger(base.screen_width_dp, test.screen_width_dp, requested.screen_width_dp),
      Larger(base.screen_height_dp, test.screen_height_dp, requested.screen_height_dp),
      Larger(base.screen_size, test.screen_size, requested.screen_size),
      Specific(base.screen_long, test.screen_long, requested.screen_long),
      Specific(base.screen_round, test.screen_round, requested.screen_round),
      Specific(base.orientation, test.orientation, requested.orientation),
      Specific(base.ui_mode_type, test.ui_mode_type, requested.ui_mode_type),
      Specific(base.ui_mode_night, test.ui_mode_night, requested.ui_mode_night),
  };
  for (const int decision : decisions) {
    if (decision != 0) return decision > 0 ? 1 : 0;
  }
  if (base.density != test.density) {
    const std::int32_t wanted = requested.density == kAny || requested.density == kDensityAny ||
                                        requested.density == kDensityNone
                                    ? kDensityMedium
                                    : requested.density;
    const auto distance = [wanted](std::int32_t density) {
      const std::int32_t d = density == kAny ? kDensityMedium : density;
      return d >= wanted ? (d - wanted) * 2 : (wanted - d) * 3;
    };
    return distance(base.density) < distance(test.density) ? 1 : 0;
  }
  const int late[] = {
      Specific(base.touchscreen, test.touchscreen, requested.touchscreen),
      Specific(base.keys_hidden, test.keys_hidden, requested.keys_hidden),
      Specific(base.keyboard, test.keyboard, requested.keyboard),
      Specific(base.nav_hidden, test.nav_hidden, requested.nav_hidden),
      Specific(base.navigation, test.navigation, requested.navigation),
      Larger(base.sdk_version, test.sdk_version, requested.sdk_version),
      Specific(base.grammatical_gender, test.grammatical_gender, requested.grammatical_gender),
  };
  for (const int decision : late) {
    if (decision != 0) return decision > 0 ? 1 : 0;
  }
  return 0;
}

}
}

using elixir::android::DeviceConfiguration;

ELIXIR_ANDROID_FN(AConfiguration*, AConfiguration_new)() noexcept {
  auto* config = new (std::nothrow) AConfiguration();
  if (config == nullptr) return nullptr;
  config->values = DeviceConfiguration{};
  std::memset(config->values.language, 0, 2);
  std::memset(config->values.country, 0, 2);
  return config;
}

ELIXIR_ANDROID_FN(void, AConfiguration_delete)(AConfiguration* config) noexcept { delete config; }

ELIXIR_ANDROID_FN(void, AConfiguration_fromAssetManager)(AConfiguration* out, AAssetManager* manager) noexcept {
  if (out == nullptr) return;
  elixir::android::GuardedVoid([&] { out->values = elixir::android::CurrentConfiguration(); });
}

ELIXIR_ANDROID_FN(void, AConfiguration_copy)(AConfiguration* dest, AConfiguration* src) noexcept {
  if (dest != nullptr && src != nullptr) dest->values = src->values;
}

ELIXIR_ANDROID_FN(void, AConfiguration_getLanguage)(AConfiguration* config, char* out) noexcept {
  if (config != nullptr && out != nullptr) std::memcpy(out, config->values.language, 2);
}

ELIXIR_ANDROID_FN(void, AConfiguration_setLanguage)(AConfiguration* config, const char* language) noexcept {
  if (config == nullptr) return;
  elixir::android::PackGuestCode(language, 'a', config->values.language);
}

ELIXIR_ANDROID_FN(void, AConfiguration_getCountry)(AConfiguration* config, char* out) noexcept {
  if (config != nullptr && out != nullptr) std::memcpy(out, config->values.country, 2);
}

ELIXIR_ANDROID_FN(void, AConfiguration_setCountry)(AConfiguration* config, const char* country) noexcept {
  if (config == nullptr) return;
  elixir::android::PackGuestCode(country, '0', config->values.country);
}

#define ELIXIR_CONFIG_FIELD(Name, field)                                                                    \
  ELIXIR_ANDROID_FN(std::int32_t, AConfiguration_get##Name)(AConfiguration * config) noexcept {            \
    return config != nullptr ? config->values.field : 0;                                                    \
  }                                                                                                         \
  ELIXIR_ANDROID_FN(void, AConfiguration_set##Name)(AConfiguration * config, std::int32_t value) noexcept { \
    if (config != nullptr) config->values.field = value;                                                    \
  }
ELIXIR_CONFIG_FIELD(Mcc, mcc)
ELIXIR_CONFIG_FIELD(Mnc, mnc)
ELIXIR_CONFIG_FIELD(Orientation, orientation)
ELIXIR_CONFIG_FIELD(Touchscreen, touchscreen)
ELIXIR_CONFIG_FIELD(Density, density)
ELIXIR_CONFIG_FIELD(Keyboard, keyboard)
ELIXIR_CONFIG_FIELD(Navigation, navigation)
ELIXIR_CONFIG_FIELD(KeysHidden, keys_hidden)
ELIXIR_CONFIG_FIELD(NavHidden, nav_hidden)
ELIXIR_CONFIG_FIELD(SdkVersion, sdk_version)
ELIXIR_CONFIG_FIELD(ScreenSize, screen_size)
ELIXIR_CONFIG_FIELD(ScreenLong, screen_long)
ELIXIR_CONFIG_FIELD(ScreenRound, screen_round)
ELIXIR_CONFIG_FIELD(UiModeType, ui_mode_type)
ELIXIR_CONFIG_FIELD(UiModeNight, ui_mode_night)
ELIXIR_CONFIG_FIELD(ScreenWidthDp, screen_width_dp)
ELIXIR_CONFIG_FIELD(ScreenHeightDp, screen_height_dp)
ELIXIR_CONFIG_FIELD(SmallestScreenWidthDp, smallest_screen_width_dp)
ELIXIR_CONFIG_FIELD(LayoutDirection, layout_direction)
ELIXIR_CONFIG_FIELD(GrammaticalGender, grammatical_gender)
#undef ELIXIR_CONFIG_FIELD

ELIXIR_ANDROID_FN(std::int32_t, AConfiguration_diff)(AConfiguration* a, AConfiguration* b) noexcept {
  if (a == nullptr || b == nullptr) return 0;
  const DeviceConfiguration& x = a->values;
  const DeviceConfiguration& y = b->values;
  std::int32_t diff = 0;
  if (x.mcc != y.mcc) diff |= 0x0001;
  if (x.mnc != y.mnc) diff |= 0x0002;
  if (std::memcmp(x.language, y.language, 2) != 0 || std::memcmp(x.country, y.country, 2) != 0) diff |= 0x0004;
  if (x.touchscreen != y.touchscreen) diff |= 0x0008;
  if (x.keyboard != y.keyboard) diff |= 0x0010;
  if (x.keys_hidden != y.keys_hidden || x.nav_hidden != y.nav_hidden) diff |= 0x0020;
  if (x.navigation != y.navigation) diff |= 0x0040;
  if (x.orientation != y.orientation) diff |= 0x0080;
  if (x.density != y.density) diff |= 0x0100;
  if (x.screen_width_dp != y.screen_width_dp || x.screen_height_dp != y.screen_height_dp) diff |= 0x0200;
  if (x.sdk_version != y.sdk_version) diff |= 0x0400;
  if (x.screen_size != y.screen_size || x.screen_long != y.screen_long) diff |= 0x0800;
  if (x.ui_mode_type != y.ui_mode_type || x.ui_mode_night != y.ui_mode_night) diff |= 0x1000;
  if (x.smallest_screen_width_dp != y.smallest_screen_width_dp) diff |= 0x2000;
  if (x.layout_direction != y.layout_direction) diff |= 0x4000;
  if (x.screen_round != y.screen_round) diff |= 0x8000;
  if (x.grammatical_gender != y.grammatical_gender) diff |= 0x20000;
  return diff;
}

ELIXIR_ANDROID_FN(std::int32_t, AConfiguration_match)(AConfiguration* base, AConfiguration* requested) noexcept {
  if (base == nullptr || requested == nullptr) return 0;
  return elixir::android::Match(base->values, requested->values);
}

ELIXIR_ANDROID_FN(std::int32_t, AConfiguration_isBetterThan)(AConfiguration* base, AConfiguration* test,
                                                             AConfiguration* requested) noexcept {
  if (base == nullptr || test == nullptr || requested == nullptr) return 0;
  return elixir::android::IsBetterThan(base->values, test->values, requested->values);
}

namespace elixir::android {

void RegisterConfigurationExports(bionic::ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibandroid, #name, reinterpret_cast<void*>(&elixir_android_##name))
#define ELIXIR_ADD_FIELD(Name) \
  ELIXIR_ADD(AConfiguration_get##Name); \
  ELIXIR_ADD(AConfiguration_set##Name)
  ELIXIR_ADD(AConfiguration_new);
  ELIXIR_ADD(AConfiguration_delete);
  ELIXIR_ADD(AConfiguration_fromAssetManager);
  ELIXIR_ADD(AConfiguration_copy);
  ELIXIR_ADD(AConfiguration_getLanguage);
  ELIXIR_ADD(AConfiguration_setLanguage);
  ELIXIR_ADD(AConfiguration_getCountry);
  ELIXIR_ADD(AConfiguration_setCountry);
  ELIXIR_ADD_FIELD(Mcc);
  ELIXIR_ADD_FIELD(Mnc);
  ELIXIR_ADD_FIELD(Orientation);
  ELIXIR_ADD_FIELD(Touchscreen);
  ELIXIR_ADD_FIELD(Density);
  ELIXIR_ADD_FIELD(Keyboard);
  ELIXIR_ADD_FIELD(Navigation);
  ELIXIR_ADD_FIELD(KeysHidden);
  ELIXIR_ADD_FIELD(NavHidden);
  ELIXIR_ADD_FIELD(SdkVersion);
  ELIXIR_ADD_FIELD(ScreenSize);
  ELIXIR_ADD_FIELD(ScreenLong);
  ELIXIR_ADD_FIELD(ScreenRound);
  ELIXIR_ADD_FIELD(UiModeType);
  ELIXIR_ADD_FIELD(UiModeNight);
  ELIXIR_ADD_FIELD(ScreenWidthDp);
  ELIXIR_ADD_FIELD(ScreenHeightDp);
  ELIXIR_ADD_FIELD(SmallestScreenWidthDp);
  ELIXIR_ADD_FIELD(LayoutDirection);
  ELIXIR_ADD_FIELD(GrammaticalGender);
  ELIXIR_ADD(AConfiguration_diff);
  ELIXIR_ADD(AConfiguration_match);
  ELIXIR_ADD(AConfiguration_isBetterThan);
#undef ELIXIR_ADD_FIELD
#undef ELIXIR_ADD
}

}
