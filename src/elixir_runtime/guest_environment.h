// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_GUEST_ENVIRONMENT_H_
#define ELIXIR_RUNTIME_GUEST_ENVIRONMENT_H_

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bionic/runtime.h"

namespace elixir::runtime {

struct GuestEnvironmentOptions {
  std::string package_name = "com.roblox.client";
  std::filesystem::path data_root;
  std::filesystem::path app_dir;
  std::filesystem::path mods_dir;
  std::filesystem::path guest_runtime_dir;
  std::vector<std::filesystem::path> library_search_path;
};

bool EnsureAndroidDirectory(const std::string& guest_path);

struct GuestEnvironmentStatus {
  bool ok = false;
  int exit_code = 0;
  std::string error;
  std::size_t export_count = 0;
  std::size_t synthetic_libraries = 0;
  bool vulkan_installed = false;
  std::size_t vulkan_exports = 0;
  bool libc_extension_loaded = false;
  std::size_t libc_extension_exports = 0;
  std::string libc_extension_note;
  std::vector<std::string> mounts;
  std::string library_search_path;
};

const GuestEnvironmentStatus& EnsureGuestEnvironment(const GuestEnvironmentOptions& options);
bool GuestEnvironmentReady();

const std::unordered_map<std::string, std::unordered_set<std::string>>& SyntheticLibraryExports();
bool IsSyntheticLibrary(std::string_view soname);

}

#endif
