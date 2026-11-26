// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_GLOBAL_SETTINGS_H_
#define ELIXIR_LAUNCHER_CORE_GLOBAL_SETTINGS_H_

#include <optional>
#include <string>
#include <string_view>

#include "core/paths.h"

namespace elixir::launcher {

enum class GlobalSetting { kQualityLevel, kFramerateCap, kTextSize, kTransparency, kMouseSensitivity, kReducedMotion };

struct GlobalSettings {
  bool read_only = false;
  std::optional<int> quality_level, framerate_cap, text_size;
  std::optional<float> transparency, mouse_sensitivity;
  std::optional<bool> reduced_motion;
};

bool LoadGlobalSettings(const LauncherPaths& paths, GlobalSettings* settings, std::wstring* error);
bool SaveGlobalSetting(const LauncherPaths& paths, GlobalSetting field, std::string_view value, std::wstring* error);
bool SetGlobalSettingsReadOnly(const LauncherPaths& paths, bool read_only, std::wstring* error);

}

#endif
