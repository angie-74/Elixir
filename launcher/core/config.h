// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_CONFIG_H_
#define ELIXIR_LAUNCHER_CORE_CONFIG_H_

#include <cstdint>
#include <string>
#include <vector>

#include "core/json.h"

namespace elixir::launcher {

enum class PresentMode { kFifo, kMailbox, kImmediate };

const char* ToString(PresentMode mode);
bool ParsePresentMode(std::string_view text, PresentMode* mode);

inline constexpr double kResolutionScaleMin = 0.25;
inline constexpr double kResolutionScaleMax = 1.0;
inline constexpr double kStretchMax = 0.5;

double ResolutionScaleOrDefault(double scale);
double StretchOrDefault(double stretch);

struct GraphicsSettings {
  std::string renderer = "vulkan";
  PresentMode present_mode = PresentMode::kFifo;
  double resolution_scale = 1.0;
  double stretch = 0.0;
};

struct PerformanceSettings {
  bool high_priority = false;
  bool disable_power_throttling = true;
  bool timer_resolution_1ms = true;
  bool mmcss = true;
};

struct AudioSettings {
  std::string output_device_id;
  std::string output_device_name;
  std::string input_device_id;
  std::string input_device_name;
};

struct AdvancedSettings {
  std::string fflags_path;
  bool manage_fast_flags = true;
};

struct UpdateSettings {
  bool auto_update = true;
};

struct IntegrationSettings {
  bool discord_rich_presence = false;
  bool discord_show_game = true;
  bool discord_join_button = false;
  bool discord_allow_game_presence = true;
  bool server_location = false;
};

struct ModSettings {
  bool enabled = true;
};

struct InstalledApp {
  std::string package;
  std::int64_t version_code = 0;
  std::string version_name;
  std::string signer_sha256;
  std::string signer_status;
  std::string abi;
  std::string install_dir;
  std::string installed_at;
  std::string source_file;
  bool present() const { return !package.empty(); }
};

struct RobloxSourceSettings {
  bool auto_update = true;
  bool check_for_new_version = true;
  std::int64_t last_check = 0;
  InstalledApp previous;
  std::string previous_snapshot;
};

struct LauncherConfig {
  GraphicsSettings graphics;
  PerformanceSettings performance;
  AudioSettings audio;
  AdvancedSettings advanced;
  UpdateSettings updates;
  IntegrationSettings integrations;
  ModSettings mods;
  RobloxSourceSettings roblox;
  InstalledApp installed;
  std::string last_page = "home";
  std::string language = "auto";
};

inline constexpr int kConfigSchemaVersion = 2;

Json ConfigToJson(const LauncherConfig& config);
LauncherConfig ConfigFromJson(const Json& json, std::vector<std::string>* warnings);
int ConfigSchemaOf(const Json& json);
std::vector<std::string> MigrateConfigJson(Json* json);
std::string SerializeConfig(const LauncherConfig& config);
std::string LanguageOrAutomatic(std::string_view language);
std::string SavedLanguage(const std::wstring& config_file);

bool IsHttpsUrl(std::string_view url);
bool IsDiscordApplicationId(std::string_view id);
std::string_view DiscordApplicationId();

enum class LoadOutcome {
  kLoaded,
  kCreatedDefaults,
  kRecoveredFromBackup,
  kResetToDefaults,
};

struct LoadResult {
  LauncherConfig config;
  LoadOutcome outcome = LoadOutcome::kLoaded;
  std::wstring message;
  std::wstring quarantined_path;
  std::vector<std::string> warnings;
  bool saved = true;
  int migrated_from = 0;
  std::vector<std::string> migration;
};

class ConfigStore {
 public:
  explicit ConfigStore(std::wstring path) : path_(std::move(path)) {}

  LoadResult Load();
  bool Save(const LauncherConfig& config, std::wstring* error);

  const std::wstring& path() const { return path_; }

 private:
  bool SaveRepaired(const LauncherConfig& config, std::wstring* error);

  std::wstring path_;
};

}

#endif
