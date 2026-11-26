// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_INSTANCES_H_
#define ELIXIR_LAUNCHER_CORE_INSTANCES_H_

#include <windows.h>

#include <string>

#include "core/config.h"
#include "core/paths.h"

namespace elixir::launcher {

inline constexpr int kMaxRobloxWindows = 8;

LauncherPaths InstancePaths(const LauncherPaths& paths, int instance);

bool PrepareInstance(const LauncherConfig& config, const LauncherPaths& paths, int instance, std::wstring* error);

bool IsInstanceCopySkipped(const std::wstring& name);

bool DataFolderInUse(const std::wstring& data_root);

HANDLE TryReserveDataFolder(const std::wstring& data_root);

}

#endif
