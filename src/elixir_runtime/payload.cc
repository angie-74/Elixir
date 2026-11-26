// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/payload.h"

#include <windows.h>

#define JSON_NOEXCEPTION 1
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "elixir_runtime/cli.h"
#include "elixir_runtime/elf_dynamic.h"
#include "elixir_runtime/text.h"

namespace elixir::runtime {
namespace {

bool IsDirectory(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsFile(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string Shown(const std::filesystem::path& path) { return NativeUtf8(path); }

std::vector<std::string> InstalledSplitApks(const std::filesystem::path& app_dir,
                                            const nlohmann::json& record) {
  std::vector<std::string> files;
  const auto add = [&](const std::string& name) {
    if (name.size() <= 10 || name.compare(0, 6, "split_") != 0 ||
        name.compare(name.size() - 4, 4, ".apk") != 0 ||
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") !=
            std::string::npos) return;
    const DWORD attr = GetFileAttributesW((app_dir / name).c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0) {
      files.push_back(name);
    }
  };
  const auto entries = record.find("split_apks");
  if (entries != record.end()) {
    if (entries->is_array()) {
      for (const auto& entry : *entries) {
        if (entry.is_string()) add(entry.get_ref<const std::string&>());
      }
    }
  } else {
    for (const char* name : {"split_config.arm64_v8a.apk", "split_config.armeabi_v7a.apk",
                             "split_config.x86_64.apk"}) add(name);
  }
  std::sort(files.begin(), files.end());
  files.erase(std::unique(files.begin(), files.end()), files.end());
  return files;
}

std::filesystem::path Absolute(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::path absolute = std::filesystem::absolute(path, error);
  return error ? path : absolute;
}

bool CheckPackageName(const std::string& package, const char* source, std::string* error) {
  if (package.empty() || IsAndroidPackageName(package)) return true;
  *error = std::string(source) + " names the package '" + package +
           "', which is not an Android package name; reinstall the APK with Elixir.exe";
  return false;
}

bool ValidateLibrary(PayloadLocation* location, std::string* error) {
  ElfDynamicInfo info;
  std::string problem;
  if (!ReadElfDynamicFile(location->roblox_library, &info, &problem)) {
    *error = Shown(location->roblox_library) + " is not a usable Android library: " + problem;
    return false;
  }
  if (info.machine != kElfMachineX86_64) {
    *error = Shown(location->roblox_library) + " is built for ELF machine " + std::to_string(info.machine) +
             ", not x86-64 (62); Elixir runs only the x86_64 build of the app";
    return false;
  }
  location->build_id = info.build_id;
  location->needed = info.needed;
  return true;
}

}

PayloadResolution ResolveInstalledPayload(const PayloadRequest& request) {
  PayloadResolution result;
  PayloadLocation& location = result.location;
  if (!CheckPackageName(request.expected_package, "the requested app (--package or config.json installed.package)",
                        &result.error)) {
    return result;
  }
  if (!request.roblox_library.empty()) {
    location.development_override = true;
    location.roblox_library = Absolute(request.roblox_library);
    location.library_dir = location.roblox_library.parent_path();
    location.app_dir = request.app_dir;
    if (!IsFile(location.roblox_library)) {
      result.error = "--roblox-lib " + Shown(location.roblox_library) + " does not exist";
      return result;
    }
    location.package = request.expected_package;
    location.version_code = request.expected_version_code;
    if (!location.app_dir.empty()) location.split_apks = InstalledSplitApks(location.app_dir, {});
    if (!ValidateLibrary(&location, &result.error)) return result;
    return result;
  }
  if (request.app_dir.empty()) {
    result.error =
        "no Android app is installed: install the Roblox APK with Elixir.exe (Install APK), or pass "
        "--app-dir <folder> for an existing install";
    return result;
  }
  location.app_dir = Absolute(request.app_dir);
  if (!IsDirectory(location.app_dir)) {
    result.error = "the app folder " + Shown(location.app_dir) +
                   " does not exist; reinstall the APK with Elixir.exe";
    return result;
  }

  const std::filesystem::path record = location.app_dir / L"elixir-install.json";
  if (IsFile(record)) {
    std::ifstream input(record, std::ios::binary);
    std::ostringstream text;
    text << input.rdbuf();
    const nlohmann::json json = nlohmann::json::parse(text.str(), nullptr, false, false);
    if (json.is_discarded() || !json.is_object()) {
      result.error = Shown(record) + " is not valid JSON; reinstall the APK with Elixir.exe";
      return result;
    }
    location.install_record = true;
    if (const auto it = json.find("package"); it != json.end() && it->is_string()) {
      location.package = it->get<std::string>();
    }
    if (const auto it = json.find("version_code"); it != json.end() && it->is_number_integer()) {
      location.version_code = it->get<std::int64_t>();
    }
    if (const auto it = json.find("version_name"); it != json.end() && it->is_string()) {
      location.version_name = it->get<std::string>();
    }
    if (!CheckPackageName(location.package, Shown(record).c_str(), &result.error)) return result;
    if (!request.expected_package.empty() && location.package != request.expected_package) {
      result.error = Shown(location.app_dir) + " holds " +
                     (location.package.empty() ? std::string("an unnamed package") : location.package) +
                     ", not " + request.expected_package + "; reinstall the APK with Elixir.exe";
      return result;
    }
    if (request.expected_version_code >= 0 && location.version_code != request.expected_version_code) {
      result.error = Shown(location.app_dir) + " holds versionCode " + std::to_string(location.version_code) +
                     ", not " + std::to_string(request.expected_version_code) +
                     "; reinstall the APK with Elixir.exe";
      return result;
    }
    location.split_apks = InstalledSplitApks(location.app_dir, json);
  } else {
    location.package = request.expected_package;
    location.version_code = request.expected_version_code;
    location.split_apks = InstalledSplitApks(location.app_dir, {});
  }

  for (const std::filesystem::path& candidate :
       {location.app_dir / L"lib" / L"x86_64", location.app_dir / L"lib"}) {
    if (IsFile(candidate / kRobloxLibraryName)) {
      location.library_dir = candidate;
      location.roblox_library = candidate / kRobloxLibraryName;
      break;
    }
  }
  if (location.roblox_library.empty()) {
    result.error = "no " + std::string(kRobloxLibraryName) + " in " + Shown(location.app_dir / L"lib") +
                   ": the installed APK has no x86_64 Roblox library; install the x86_64 Roblox APK "
                   "with Elixir.exe";
    return result;
  }
  ValidateLibrary(&location, &result.error);
  return result;
}

}
