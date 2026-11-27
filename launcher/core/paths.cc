// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/paths.h"

#include "core/util.h"

namespace elixir::launcher {
namespace {

std::wstring& Program() {
  static std::wstring program;
  return program;
}

std::wstring& Bundle() {
  static std::wstring bundle;
  return bundle;
}

}

void UseBuiltInPrograms(const std::wstring& program) { Program() = program; }
const std::wstring& BuiltInProgram() { return Program(); }
void SetBundleDirectory(const std::wstring& directory) { Bundle() = directory; }
const std::wstring& BundleDirectory() { return Bundle(); }

LauncherPaths LauncherPathsForDataRoot(const std::wstring& exe_dir, const std::wstring& data_root, bool portable) {
  LauncherPaths paths;
  paths.portable = portable;
  paths.exe_dir = exe_dir;
  paths.data_root = data_root;
  paths.config_file = JoinPath(data_root, L"config.json");
  paths.fflags_file = JoinPath(data_root, L"fflags.json");
  paths.apps_dir = JoinPath(data_root, L"apps");
  paths.crashes_dir = JoinPath(data_root, L"crashes");
  paths.updates_dir = JoinPath(data_root, L"updates");
  paths.logs_dir = JoinPath(data_root, L"logs");
  paths.downloads_dir = JoinPath(data_root, L"downloads");
  paths.android_data_dir = JoinPath(data_root, L"data");
  paths.guest_tmp_dir = JoinPath(data_root, L"tmp");
  paths.bundle_dir = JoinPath(data_root, L"bundled");
  paths.accounts_dir = JoinPath(JoinPath(data_root, L"auth"), L"accounts");
  if (!BuiltInProgram().empty()) {
    paths.runtime_exe = BuiltInProgram();
    paths.runtime_flag = kRuntimeModeFlag;
    paths.guest_runtime_dir = JoinPath(paths.bundle_dir, L"guest-runtime");
  } else {
    paths.runtime_exe = JoinPath(exe_dir, kRuntimeExecutable);
    paths.guest_runtime_dir = JoinPath(exe_dir, L"guest-runtime");
  }
  return paths;
}

LauncherPaths ResolveLauncherPaths(const std::wstring& exe_dir, const std::wstring& local_app_data) {
  const bool portable = !exe_dir.empty() && PathIsFile(JoinPath(exe_dir, kPortableMarker));
  const std::wstring data_root = portable ? JoinPath(exe_dir, L"data") : JoinPath(local_app_data, L"Elixir");
  return LauncherPathsForDataRoot(exe_dir, data_root, portable);
}

LauncherPaths CurrentLauncherPaths() { return ResolveLauncherPaths(ExecutableDirectory(), LocalAppDataDirectory()); }

bool EnsureLauncherDirectories(const LauncherPaths& paths, std::wstring* error) {
  for (const std::wstring* directory : {&paths.data_root, &paths.apps_dir, &paths.crashes_dir, &paths.updates_dir}) {
    if (!EnsureDirectory(*directory, error)) return false;
  }
  return true;
}

std::wstring AndroidAppDataDirectory(const LauncherPaths& paths, std::string_view package) {
  std::wstring safe;
  for (const char c : package) {
    const bool allowed =
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
    safe.push_back(allowed ? static_cast<wchar_t>(c) : L'_');
  }
  while (!safe.empty() && safe.front() == L'.') safe.erase(safe.begin());
  if (safe.empty()) safe = L"unknown";
  return JoinPath(paths.android_data_dir, safe);
}

}
