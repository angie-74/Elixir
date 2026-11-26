// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_PAYLOAD_H_
#define ELIXIR_RUNTIME_PAYLOAD_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace elixir::runtime {

inline constexpr char kRobloxLibraryName[] = "libroblox.so";

struct PayloadRequest {
  std::filesystem::path app_dir;
  std::filesystem::path roblox_library;
  std::string expected_package;
  std::int64_t expected_version_code = -1;
};

struct PayloadLocation {
  std::filesystem::path app_dir;
  std::filesystem::path library_dir;
  std::filesystem::path roblox_library;
  bool development_override = false;
  bool install_record = false;
  std::string package;
  std::int64_t version_code = -1;
  std::string version_name;
  std::vector<std::string> split_apks;
  std::string build_id;
  std::vector<std::string> needed;
};

struct PayloadResolution {
  PayloadLocation location;
  std::string error;
  bool ok() const { return error.empty(); }
};

PayloadResolution ResolveInstalledPayload(const PayloadRequest& request);

}

#endif
