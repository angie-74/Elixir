// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/guest_environment.h"

#include "runtime/mod_paths.h"

#include <windows.h>

#include <mcpelauncher/linker.h>

#include "bionic/export_table.h"
#include "bionic/files/files.h"
#include "bionic/guest_ext.h"
#include "bionic/vfs.h"
#include "elf/loader_log.h"
#include "elixir_runtime/cli.h"
#include "elixir_runtime/host_log.h"
#include "elixir_runtime/text.h"
#include "platform/windows/guest_tls.h"

#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
#include "graphics/vulkan/guest_vulkan.h"
#endif
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
#include "android/android.h"
#endif
#if defined(ELIXIR_RUNTIME_HAS_OPENSL)
#include "audio/opensl_exports_windows.h"
#endif

namespace linker::libdl {
std::unordered_map<std::string, void*> get_dl_symbols();
}

namespace elixir::runtime {
namespace {

GuestEnvironmentStatus g_status;
bool g_initialized = false;
bionic::ExportTable g_table;
std::unordered_map<std::string, std::unordered_set<std::string>> g_exports;

bool IsFile(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string SearchPath(const std::vector<std::filesystem::path>& directories, std::string* unusable) {
  std::string search;
  for (const std::filesystem::path& directory : directories) {
    if (directory.empty()) continue;
    const std::string text = GenericUtf8(directory);
    if (text.find(';') != std::string::npos) {
      if (unusable != nullptr && unusable->empty()) *unusable = NativeUtf8(directory);
      continue;
    }
    if (!search.empty()) search += ";";
    search += text;
  }
  return search;
}

GuestEnvironmentStatus Initialize(const GuestEnvironmentOptions& options) {
  GuestEnvironmentStatus status;
  const auto fail = [&status](int code, std::string message) {
    status.ok = false;
    status.exit_code = code;
    status.error = std::move(message);
    return status;
  };

  bionic::RuntimeOptions runtime;
  runtime.package_name = options.package_name;
  runtime.log_sink = &GuestLogSink;
  elixir_loader_set_log_sink(&LoaderLogSink);
  if (!bionic::InitializeGuestLibc(runtime, &g_table)) {
    return fail(kExitGuestRuntime,
                "guest TLS is unavailable on this machine: Android code needs user-mode FSGSBASE "
                "(RDFSBASE/WRFSBASE), which this CPU or Windows build does not provide");
  }
  if (!g_table.conflicts().empty()) {
    std::string message = "the bionic export table registers " + std::to_string(g_table.conflicts().size()) +
                          " symbol(s) twice with different addresses (a build defect):";
    for (const std::string& conflict : g_table.conflicts()) message += " " + conflict;
    return fail(kExitGuestRuntime, message);
  }
  status.export_count = g_table.size();

  const std::string root = GenericUtf8(options.data_root);
  std::vector<bionic::vfs::Mount> mounts = bionic::files::DefaultMounts(options.package_name, root);
  if (!bionic::files::EnsureMountDirectories(mounts)) {
    return fail(kExitConfiguration, "cannot create the Android data folders below " + NativeUtf8(options.data_root));
  }
  if (!options.app_dir.empty()) {
    mounts.push_back({"/data/app/" + options.package_name, GenericUtf8(options.app_dir), true});
    if (!options.mods_dir.empty()) {
      bionic::vfs::Mount assets;
      assets.guest_prefix = "/data/app/" + options.package_name + "/assets";
      assets.host_path = GenericUtf8(options.app_dir / L"assets");
      assets.read_only = true;
      assets.overlay_host_path = GenericUtf8(options.mods_dir);
      assets.overlay_filter = &mods::IsReplaceable;
      mounts.push_back(std::move(assets));
    }
  }
  bionic::vfs::SetMounts(mounts.data(), mounts.size());
  for (const bionic::vfs::Mount& mount : mounts) {
    status.mounts.push_back(mount.guest_prefix + " -> " + mount.host_path + (mount.read_only ? " [ro]" : "") +
                            (mount.overlay_host_path.empty() ? "" : ", mods from " + mount.overlay_host_path));
  }

  linker::init();
  status.synthetic_libraries = bionic::InstallSyntheticLibraries(g_table);
  for (const auto& [soname, symbols] : g_table.libraries()) {
    auto& names = g_exports[soname];
    for (const auto& [name, address] : symbols) names.insert(name);
  }
  {
    auto& names = g_exports[std::string(bionic::kLibdl)];
    for (const auto& [name, address] : linker::libdl::get_dl_symbols()) names.insert(name);
  }
#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
  {
    std::unordered_map<std::string, void*> vulkan;
    vulkan::RegisterVulkanExports(vulkan);
    linker::load_library("libvulkan.so", vulkan);
    status.vulkan_installed = true;
    status.vulkan_exports = vulkan.size();
    ++status.synthetic_libraries;
    auto& names = g_exports["libvulkan.so"];
    for (const auto& [name, address] : vulkan) names.insert(name);
  }
#endif
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
  status.synthetic_libraries += android::InstallAndroidLibraries();
  {
    bionic::ExportTable android_names;
    android::RegisterAndroidExports(android_names);
    for (const auto& [soname, symbols] : android_names.libraries()) {
      auto& names = g_exports[soname];
      for (const auto& [name, address] : symbols) names.insert(name);
    }
  }
#endif
#if defined(ELIXIR_RUNTIME_HAS_OPENSL)
  {
    std::unordered_map<std::string, void*> opensl;
    audio::RegisterOpenSlExports(opensl);
    linker::load_library(audio::kOpenSlSoname, opensl);
    ++status.synthetic_libraries;
    auto& names = g_exports[audio::kOpenSlSoname];
    for (const auto& [name, address] : opensl) names.insert(name);
    linker::load_library("libOpenMAXAL.so", opensl);
    ++status.synthetic_libraries;
    auto& openmax = g_exports["libOpenMAXAL.so"];
    for (const auto& [name, address] : opensl) openmax.insert(name);
  }
#endif

  platform::EnsureGuestThread();
  const std::filesystem::path extension =
      options.guest_runtime_dir.empty() ? std::filesystem::path()
                                        : options.guest_runtime_dir / bionic::kGuestLibcExtensionName;
  if (extension.empty() || !IsFile(extension)) {
    status.libc_extension_note = std::string(bionic::kGuestLibcExtensionName) + " not found in " +
                                 (options.guest_runtime_dir.empty() ? std::string("(no guest-runtime folder)")
                                                                    : NativeUtf8(options.guest_runtime_dir)) +
                                 "; libraries that import its functions (regex, getopt, _Unwind_*, ...) will "
                                 "not load";
  } else {
    std::string error;
    if (!bionic::LoadGuestLibcExtension(GenericUtf8(extension), &error)) {
      return fail(kExitGuestRuntime, "cannot install the guest libc extension " + NativeUtf8(extension) + ": " + error);
    }
    status.libc_extension_loaded = true;
    const std::vector<std::string> names = bionic::GuestLibcExtensionExports();
    status.libc_extension_exports = names.size();
    auto& libc = g_exports[std::string(bionic::kLibc)];
    libc.insert(names.begin(), names.end());
  }

  std::string unusable;
  const std::string search = SearchPath(options.library_search_path, &unusable);
  if (!unusable.empty()) {
    return fail(kExitConfiguration, "the library folder " + unusable +
                                        " has ';' in its path, which the Android loader's search path cannot "
                                        "hold; move Elixir or the app to a folder without ';'");
  }
  if (!search.empty()) linker::update_LD_LIBRARY_PATH(search.c_str());
  status.library_search_path = search;
  status.ok = true;
  return status;
}

}

bool EnsureAndroidDirectory(const std::string& guest_path) {
  std::string host;
  int guest_errno = 0;
  bool read_only = false;
  if (!bionic::vfs::ResolveToHost(guest_path, &host, &guest_errno, &read_only) || read_only) {
    Log("Roblox start-up: " + guest_path + " is not a writable Android folder (errno " +
        std::to_string(guest_errno) + ")");
    return false;
  }
  std::error_code error;
  std::filesystem::create_directories(PathFromUtf8(host), error);
  if (error) Log("Roblox start-up: cannot create " + guest_path + ": " + error.message());
  return !error;
}

const GuestEnvironmentStatus& EnsureGuestEnvironment(const GuestEnvironmentOptions& options) {
  if (!g_initialized) {
    g_status = Initialize(options);
    g_initialized = true;
  } else if (g_status.ok && !options.library_search_path.empty()) {
    std::string unusable;
    const std::string search = SearchPath(options.library_search_path, &unusable);
    if (!unusable.empty()) {
      static thread_local GuestEnvironmentStatus refused;
      refused = g_status;
      refused.ok = false;
      refused.exit_code = kExitConfiguration;
      refused.error = "the library folder " + unusable +
                      " has ';' in its path, which the Android loader's search path cannot hold; move Elixir or "
                      "the app to a folder without ';'";
      return refused;
    }
    linker::update_LD_LIBRARY_PATH(search.c_str());
    g_status.library_search_path = search;
  }
  return g_status;
}

bool GuestEnvironmentReady() { return g_initialized && g_status.ok; }

const std::unordered_map<std::string, std::unordered_set<std::string>>& SyntheticLibraryExports() {
  return g_exports;
}

bool IsSyntheticLibrary(std::string_view soname) { return g_exports.count(std::string(soname)) != 0; }

}
