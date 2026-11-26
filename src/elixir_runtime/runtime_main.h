// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_RUNTIME_MAIN_H_
#define ELIXIR_RUNTIME_RUNTIME_MAIN_H_

#include <filesystem>
#include <string>
#include <vector>

#include "elixir_runtime/cli.h"
#include "elixir_runtime/runtime_flags.h"
#include "fflags/fflags.h"
#include "runtime/runtime_config_file.h"
#include "runtime/runtime_paths.h"

namespace elixir::runtime {

struct RuntimeSettings {
  int exit_code = kExitOk;
  std::string error;
  mocktail::runtime::RuntimePaths paths;
  mocktail::runtime::RuntimeConfigLoadResult config;
  std::filesystem::path fflags_file;
  bool user_fflags = true;
};
RuntimeSettings ResolveRuntimeSettings(const LaunchOptions& options);

int BuildRuntimeFlagsDocument(const RuntimeSettings& settings, std::string* json, std::string* error,
                              UserFlags* user = nullptr, elixir::fflags::Resolution* resolution = nullptr,
                              std::string* gpu_report = nullptr);

int RunElixirRuntime(const std::vector<std::string>& arguments);

void RequestStop() noexcept;

}

#endif
