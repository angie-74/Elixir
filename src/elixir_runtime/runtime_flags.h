// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_RUNTIME_FLAGS_H_
#define ELIXIR_RUNTIME_RUNTIME_FLAGS_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fflags/fflags.h"
#include "runtime/device_profile.h"
#include "runtime/frame_rate_policy.h"
#include "runtime/performance_policy.h"

namespace elixir::runtime {

struct RuntimeFlagInputs {
  mocktail::runtime::FrameRatePolicy frame_rate;
  mocktail::runtime::PerformancePolicy performance;
  bool microphone_enabled = true;
  std::uint64_t host_memory_bytes = 0;
  std::uint64_t gpu_memory_bytes = 0;
  std::string gpu_adapter_name;
  std::string gpu_memory_source;
  bool desktop_playability = true;
  std::optional<std::string> desktop_app_policy_json;
};

struct PendingRuntimeFlag {
  std::string name;
  std::string reason;
};

struct RuntimeFlagSets {
  elixir::fflags::FlagSet mandatory;
  elixir::fflags::FlagSet defaults;
  std::vector<PendingRuntimeFlag> pending;
  std::string error;
  bool ok() const { return error.empty(); }
};

RuntimeFlagSets BuildRuntimeFlagSets(const RuntimeFlagInputs& inputs);

struct UserFlags {
  std::filesystem::path path;
  bool loaded = false;
  elixir::fflags::FlagSet flags;
  std::string error;
  std::vector<std::string> warnings;
};
UserFlags LoadUserFlags(const std::filesystem::path& path);

std::string RuntimeFlagsJson(const RuntimeFlagInputs& inputs, const RuntimeFlagSets& sets,
                             const UserFlags& user, const elixir::fflags::Allowlist& allowlist,
                             const elixir::fflags::Resolution& resolution, const std::string& device_profile);

const char* FlagStatusId(elixir::fflags::FlagStatus status);

}

#endif
