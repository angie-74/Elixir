// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_CLI_H_
#define ELIXIR_RUNTIME_CLI_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace elixir::runtime {

enum ExitCode : int {
  kExitOk = 0,
  kExitUsage = 2,
  kExitConfiguration = 3,
  kExitGuestRuntime = 4,
  kExitPayloadMissing = 5,
  kExitPayloadLoad = 6,
  kExitUnresolvedImports = 7,
  kExitNotImplemented = 10,
  kExitRestart = 20,
};

enum class RuntimeMode {
  kRun,
  kHelp,
  kVersion,
  kCheckImports,
  kPrintRuntimeFlags,
};

enum class PriorityRequest { kUnchanged, kNormal, kAboveNormal, kHigh };

struct LaunchOptions {
  RuntimeMode mode = RuntimeMode::kRun;
  std::string package;
  std::optional<std::int64_t> version_code;
  std::filesystem::path app_dir;
  std::filesystem::path data_dir;
  std::optional<int> instance;
  std::filesystem::path config_file;
  std::filesystem::path crash_dir;
  std::filesystem::path fflags_file;
  std::filesystem::path mods_dir;
  std::filesystem::path accounts_dir;
  std::optional<bool> user_fflags;
  std::string discord_app;
  std::optional<bool> discord_game;
  std::optional<bool> discord_join;
  std::optional<bool> discord_rpc;
  std::filesystem::path guest_runtime_dir;
  std::filesystem::path roblox_library;
  std::string renderer;
  std::string present_mode;
  int frame_rate_cap = 0;
  std::optional<double> resolution_scale;
  std::optional<double> stretch;
  std::optional<bool> controller;
  std::string audio_device;
  std::string audio_input_device;
  std::optional<bool> timer_resolution_1ms;
  std::optional<bool> mmcss;
  PriorityRequest priority = PriorityRequest::kUnchanged;
  std::optional<bool> power_throttling;
  bool session_log = true;
  std::optional<std::int64_t> stop_after_ms;
  std::vector<std::string> trace;
};

struct CommandLineResult {
  LaunchOptions options;
  std::string error;
  bool ok() const { return error.empty(); }
};

CommandLineResult ParseRuntimeCommandLine(const std::vector<std::string>& arguments);

bool IsAndroidPackageName(std::string_view name);

std::vector<std::string> ArgumentsFromWide(int argc, const wchar_t* const argv[]);

std::string RuntimeUsage();
std::string RuntimeVersionText();

std::vector<std::string> FormatRuntimeArguments(const LaunchOptions& options);

}

#endif
