// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_PATHS_H_
#define ELIXIR_LAUNCHER_CORE_PATHS_H_

#include <string>
#include <string_view>

namespace elixir::launcher {

inline constexpr wchar_t kPortableMarker[] = L"portable.txt";
inline constexpr wchar_t kRuntimeExecutable[] = L"elixir-runtime.exe";
inline constexpr wchar_t kRuntimeModeFlag[] = L"--elixir-runtime";
inline constexpr wchar_t kApkToolModeFlag[] = L"--elixir-apk-tool";
inline constexpr wchar_t kGuestExtensionLibrary[] = L"libelixir-extension.so";

struct LauncherPaths {
  bool portable = false;
  std::wstring exe_dir;
  std::wstring data_root;
  std::wstring config_file;
  std::wstring fflags_file;
  std::wstring apps_dir;
  std::wstring crashes_dir;
  std::wstring updates_dir;
  std::wstring logs_dir;
  std::wstring downloads_dir;
  std::wstring android_data_dir;
  std::wstring guest_tmp_dir;
  std::wstring runtime_exe;
  std::wstring runtime_flag;
  std::wstring guest_runtime_dir;
  std::wstring bundle_dir;
};

void UseBuiltInPrograms(const std::wstring& program);
const std::wstring& BuiltInProgram();
void SetBundleDirectory(const std::wstring& directory);
const std::wstring& BundleDirectory();

LauncherPaths ResolveLauncherPaths(const std::wstring& exe_dir, const std::wstring& local_app_data);

LauncherPaths LauncherPathsForDataRoot(const std::wstring& exe_dir, const std::wstring& data_root, bool portable);

LauncherPaths CurrentLauncherPaths();

bool EnsureLauncherDirectories(const LauncherPaths& paths, std::wstring* error);

inline constexpr char kRobloxPackage[] = "com.roblox.client";

std::wstring AndroidAppDataDirectory(const LauncherPaths& paths, std::string_view package);

}

#endif
