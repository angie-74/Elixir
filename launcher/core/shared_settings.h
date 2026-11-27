// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_SHARED_SETTINGS_H_
#define ELIXIR_LAUNCHER_CORE_SHARED_SETTINGS_H_

#include <string>

#include "core/paths.h"

namespace elixir::launcher {

std::wstring SharedSettingsFile(const LauncherPaths& first);
std::wstring SharedSettingsBaseFile(const LauncherPaths& window);
std::wstring SharedSettingsBackupFile(const LauncherPaths& window);

bool CollectSharedSettings(const LauncherPaths& first);

bool ShareSettingsWith(const LauncherPaths& first, const LauncherPaths& window);

}

#endif
