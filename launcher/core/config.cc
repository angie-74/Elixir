// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/config.h"

#include <cmath>
#include <optional>

#include "core/i18n.h"
#include "core/util.h"

namespace elixir::launcher {

namespace {

constexpr const char* kPresentModeNames[] = {"fifo", "mailbox", "immediate"};

bool IsValidResolutionScale(double scale) {
  return scale >= kResolutionScaleMin && scale <= kResolutionScaleMax;
}

template <typename Enum, std::size_t N>
bool ParseEnum(std::string_view text, const char* const (&names)[N], Enum* out) {
  for (std::size_t i = 0; i < N; ++i) {
    if (text == names[i]) {
      *out = static_cast<Enum>(i);
      return true;
    }
  }
  return false;
}

class Reader {
 public:
  Reader(const Json& root, std::vector<std::string>* warnings) : root_(root), warnings_(warnings) {}

  const Json* Section(const char* name) {
    const Json* section = root_.Find(name);
    if (section == nullptr) return nullptr;
    if (!section->is_object()) {
      Warn(std::string(name) + " is not an object; using defaults");
      return nullptr;
    }
    return section;
  }

  void Bool(const Json* section, const char* section_name, const char* key, bool* value) {
    if (section == nullptr) return;
    const Json* member = section->Find(key);
    if (member == nullptr) return;
    if (!member->is_bool()) {
      Warn(std::string(section_name) + "." + key + " is not a boolean; using default");
      return;
    }
    *value = member->AsBool(*value);
  }

  void String(const Json* section, const char* section_name, const char* key, std::string* value) {
    if (section == nullptr) return;
    const Json* member = section->Find(key);
    if (member == nullptr) return;
    if (!member->is_string()) {
      Warn(std::string(section_name) + "." + key + " is not a string; using default");
      return;
    }
    *value = member->string_ref();
  }

  template <typename Enum, std::size_t N>
  void EnumValue(const Json* section, const char* section_name, const char* key, const char* const (&names)[N],
                 Enum* value) {
    std::string text;
    String(section, section_name, key, &text);
    if (text.empty()) return;
    if (!ParseEnum(text, names, value)) {
      Warn(std::string(section_name) + "." + key + " has unknown value \"" + text + "\"; using default");
    }
  }

  void Warn(std::string message) {
    if (warnings_ != nullptr) warnings_->push_back(std::move(message));
  }

 private:
  const Json& root_;
  std::vector<std::string>* warnings_;
};

}

const char* ToString(PresentMode mode) { return kPresentModeNames[static_cast<int>(mode)]; }
bool ParsePresentMode(std::string_view text, PresentMode* mode) { return ParseEnum(text, kPresentModeNames, mode); }

double ResolutionScaleOrDefault(double scale) { return IsValidResolutionScale(scale) ? scale : 1.0; }
double StretchOrDefault(double stretch) { return stretch >= 0.0 && stretch <= kStretchMax ? stretch : 0.0; }


bool IsDiscordApplicationId(std::string_view id) {
  return id.size() >= 17 && id.size() <= 20 && id.find_first_not_of("0123456789") == std::string_view::npos;
}

std::string_view DiscordApplicationId() {
#if defined(ELIXIR_DISCORD_APPLICATION_ID)
  return ELIXIR_DISCORD_APPLICATION_ID;
#else
  return {};
#endif
}

bool IsHttpsUrl(std::string_view url) {
  if (url.size() <= 8) return false;
  const std::string scheme = ToLowerAscii(url.substr(0, 8));
  if (scheme != "https://") return false;
  for (const char c : url) {
    if (static_cast<unsigned char>(c) <= 0x20 || c == 0x7F) return false;
  }
  return url[8] != '/';
}

Json ConfigToJson(const LauncherConfig& config) {
  Json root = Json::MakeObject();
  root.Set("schema", kConfigSchemaVersion);

  Json& graphics = root.Set("graphics", Json::MakeObject());
  graphics.Set("renderer", config.graphics.renderer);
  graphics.Set("present_mode", ToString(config.graphics.present_mode));
  graphics.Set("resolution_scale", ResolutionScaleOrDefault(config.graphics.resolution_scale));
  graphics.Set("stretch", StretchOrDefault(config.graphics.stretch));

  Json& performance = root.Set("performance", Json::MakeObject());
  performance.Set("high_priority", config.performance.high_priority);
  performance.Set("disable_power_throttling", config.performance.disable_power_throttling);
  performance.Set("timer_resolution_1ms", config.performance.timer_resolution_1ms);
  performance.Set("mmcss", config.performance.mmcss);

  Json& audio = root.Set("audio", Json::MakeObject());
  audio.Set("output_device_id", config.audio.output_device_id);
  audio.Set("output_device_name", config.audio.output_device_name);
  audio.Set("input_device_id", config.audio.input_device_id);
  audio.Set("input_device_name", config.audio.input_device_name);

  Json& advanced = root.Set("advanced", Json::MakeObject());
  advanced.Set("fflags_path", config.advanced.fflags_path);
  advanced.Set("manage_fast_flags", config.advanced.manage_fast_flags);

  Json& updates = root.Set("updates", Json::MakeObject());
  updates.Set("auto_update", config.updates.auto_update);

  Json& integrations = root.Set("integrations", Json::MakeObject());
  integrations.Set("discord_rich_presence", config.integrations.discord_rich_presence);
  integrations.Set("discord_show_game", config.integrations.discord_show_game);
  integrations.Set("discord_join_button", config.integrations.discord_join_button);
  integrations.Set("discord_allow_game_presence", config.integrations.discord_allow_game_presence);
  integrations.Set("server_location", config.integrations.server_location);
  Json& mods = root.Set("mods", Json::MakeObject());
  mods.Set("enabled", config.mods.enabled);

  const auto app_json = [](Json& out, const InstalledApp& app) {
    out.Set("package", app.package);
    out.Set("version_code", app.version_code);
    out.Set("version_name", app.version_name);
    out.Set("signer_sha256", app.signer_sha256);
    out.Set("signer_status", app.signer_status);
    out.Set("abi", app.abi);
    out.Set("install_dir", app.install_dir);
    out.Set("installed_at", app.installed_at);
    out.Set("source_file", app.source_file);
  };

  Json& roblox = root.Set("roblox", Json::MakeObject());
  roblox.Set("auto_update", config.roblox.auto_update);
  roblox.Set("check_for_new_version", config.roblox.check_for_new_version);
  roblox.Set("last_check", config.roblox.last_check);
  if (config.roblox.previous.present()) {
    app_json(roblox.Set("previous", Json::MakeObject()), config.roblox.previous);
    roblox.Set("previous_snapshot", config.roblox.previous_snapshot);
  }

  if (config.installed.present()) app_json(root.Set("installed", Json::MakeObject()), config.installed);

  Json& ui = root.Set("ui", Json::MakeObject());
  ui.Set("last_page", config.last_page);
  ui.Set("language", LanguageOrAutomatic(config.language));
  return root;
}

std::string LanguageOrAutomatic(std::string_view language) {
  const LocaleInfo* locale = FindLocale(language);
  return locale != nullptr ? std::string(locale->code) : std::string(kAutomaticLanguage);
}

std::string SavedLanguage(const std::wstring& config_file) {
  std::string text;
  if (!ReadFileBytes(config_file, &text, nullptr, 4u << 20)) return kAutomaticLanguage;
  const std::optional<Json> json = Json::Parse(text, nullptr);
  const Json* ui = json && json->is_object() ? json->Find("ui") : nullptr;
  if (ui == nullptr || !ui->is_object()) return kAutomaticLanguage;
  return LanguageOrAutomatic(ui->GetString("language"));
}

LauncherConfig ConfigFromJson(const Json& json, std::vector<std::string>* warnings) {
  LauncherConfig config;
  Reader reader(json, warnings);
  if (!json.is_object()) {
    reader.Warn("configuration is not a JSON object; using defaults");
    return config;
  }
  const std::int64_t schema = ConfigSchemaOf(json);
  if (schema > kConfigSchemaVersion) {
    reader.Warn("configuration schema " + std::to_string(schema) + " is newer than this launcher understands (" +
                std::to_string(kConfigSchemaVersion) + "); unknown settings are ignored");
  }

  if (const Json* graphics = reader.Section("graphics")) {
    std::string renderer = config.graphics.renderer;
    reader.String(graphics, "graphics", "renderer", &renderer);
    if (renderer != "vulkan") {
      reader.Warn("graphics.renderer \"" + renderer + "\" is not supported; using vulkan");
    }
    reader.EnumValue(graphics, "graphics", "present_mode", kPresentModeNames, &config.graphics.present_mode);
    if (const Json* scale = graphics->Find("resolution_scale")) {
      const double value = scale->AsDouble(-1.0);
      if (!IsValidResolutionScale(value)) {
        reader.Warn("graphics.resolution_scale must be between 0.25 and 1.0; using 1.0");
      } else {
        config.graphics.resolution_scale = std::round(value * 100.0) / 100.0;
      }
    }
    if (const Json* stretch = graphics->Find("stretch")) {
      const double value = stretch->AsDouble(-1.0);
      if (value < 0.0 || value > kStretchMax) {
        reader.Warn("graphics.stretch must be between 0 and 0.5; using 0");
      } else {
        config.graphics.stretch = std::round(value * 100.0) / 100.0;
      }
    }
  }

  if (const Json* performance = reader.Section("performance")) {
    reader.Bool(performance, "performance", "high_priority", &config.performance.high_priority);
    reader.Bool(performance, "performance", "disable_power_throttling",
                &config.performance.disable_power_throttling);
    reader.Bool(performance, "performance", "timer_resolution_1ms", &config.performance.timer_resolution_1ms);
    reader.Bool(performance, "performance", "mmcss", &config.performance.mmcss);
  }

  if (const Json* audio = reader.Section("audio")) {
    reader.String(audio, "audio", "output_device_id", &config.audio.output_device_id);
    reader.String(audio, "audio", "output_device_name", &config.audio.output_device_name);
    reader.String(audio, "audio", "input_device_id", &config.audio.input_device_id);
    reader.String(audio, "audio", "input_device_name", &config.audio.input_device_name);
  }

  if (const Json* advanced = reader.Section("advanced")) {
    reader.String(advanced, "advanced", "fflags_path", &config.advanced.fflags_path);
    reader.Bool(advanced, "advanced", "manage_fast_flags", &config.advanced.manage_fast_flags);
  }

  if (const Json* updates = reader.Section("updates")) {
    reader.Bool(updates, "updates", "auto_update", &config.updates.auto_update);
  }

  if (const Json* integrations = reader.Section("integrations")) {
    IntegrationSettings& out = config.integrations;
    reader.Bool(integrations, "integrations", "discord_rich_presence", &out.discord_rich_presence);
    reader.Bool(integrations, "integrations", "discord_show_game", &out.discord_show_game);
    reader.Bool(integrations, "integrations", "discord_join_button", &out.discord_join_button);
    reader.Bool(integrations, "integrations", "discord_allow_game_presence", &out.discord_allow_game_presence);
    reader.Bool(integrations, "integrations", "server_location", &out.server_location);
  }
  if (const Json* mods = reader.Section("mods")) {
    reader.Bool(mods, "mods", "enabled", &config.mods.enabled);
  }

  const auto read_app = [&reader](const Json* section, const char* name) {
    InstalledApp app;
    reader.String(section, name, "package", &app.package);
    app.version_code = section->GetInt("version_code", 0);
    reader.String(section, name, "version_name", &app.version_name);
    reader.String(section, name, "signer_sha256", &app.signer_sha256);
    reader.String(section, name, "signer_status", &app.signer_status);
    reader.String(section, name, "abi", &app.abi);
    reader.String(section, name, "install_dir", &app.install_dir);
    reader.String(section, name, "installed_at", &app.installed_at);
    reader.String(section, name, "source_file", &app.source_file);
    return app;
  };

  if (const Json* roblox = reader.Section("roblox")) {
    reader.Bool(roblox, "roblox", "auto_update", &config.roblox.auto_update);
    reader.Bool(roblox, "roblox", "check_for_new_version", &config.roblox.check_for_new_version);
    if (const Json* last = roblox->Find("last_check"); last != nullptr && last->is_number()) {
      config.roblox.last_check = std::max<std::int64_t>(0, last->AsInt(0));
    }
    if (const Json* previous = roblox->Find("previous"); previous != nullptr && previous->is_object()) {
      config.roblox.previous = read_app(previous, "roblox.previous");
      reader.String(roblox, "roblox", "previous_snapshot", &config.roblox.previous_snapshot);
    }
  }

  if (const Json* installed = reader.Section("installed")) {
    const InstalledApp app = read_app(installed, "installed");
    if (!app.package.empty()) config.installed = app;
  }

  if (const Json* ui = reader.Section("ui")) {
    reader.String(ui, "ui", "last_page", &config.last_page);
    std::string language = config.language;
    reader.String(ui, "ui", "language", &language);
    config.language = LanguageOrAutomatic(language);
    if (language != config.language) reader.Warn("ui.language is not a language Elixir has; using the system's");
  }
  return config;
}

std::string SerializeConfig(const LauncherConfig& config) { return ConfigToJson(config).Dump(2); }

int ConfigSchemaOf(const Json& json) {
  const Json* schema = json.is_object() ? json.Find("schema") : nullptr;
  if (schema == nullptr) return 1;
  const std::int64_t value = schema->AsInt(1);
  return value < 1 ? 1 : value > 1000000 ? 1000000 : static_cast<int>(value);
}

std::vector<std::string> MigrateConfigJson(Json* json) {
  std::vector<std::string> steps;
  if (json == nullptr || !json->is_object()) return steps;
  int schema = ConfigSchemaOf(*json);
  if (schema >= kConfigSchemaVersion) return steps;
  if (schema == 1) {
    const LauncherConfig defaults;
    if (json->Find("integrations") == nullptr) {
      Json integrations = Json::MakeObject();
      integrations.Set("discord_rich_presence", defaults.integrations.discord_rich_presence);
      integrations.Set("discord_show_game", defaults.integrations.discord_show_game);
      integrations.Set("discord_join_button", defaults.integrations.discord_join_button);
      integrations.Set("discord_allow_game_presence", defaults.integrations.discord_allow_game_presence);
      integrations.Set("server_location", defaults.integrations.server_location);
      json->Set("integrations", std::move(integrations));
    }
    if (json->Find("roblox") == nullptr) {
      Json roblox = Json::MakeObject();
      roblox.Set("check_for_new_version", false);
      roblox.Set("auto_update", false);
      json->Set("roblox", std::move(roblox));
    }
    steps.push_back(
        "schema 1 -> 2: added \"integrations\" (Discord Rich Presence and server location, both off) and "
        "\"roblox\" (new-version notice, off)");
    schema = 2;
  }
  json->Set("schema", schema);
  return steps;
}

namespace {

bool TryLoad(const std::wstring& path, LauncherConfig* config, std::vector<std::string>* warnings,
             std::string* problem, int* schema = nullptr, std::vector<std::string>* migration = nullptr) {
  std::string bytes;
  std::wstring read_error;
  if (!ReadFileBytes(path, &bytes, &read_error, 4u << 20)) {
    *problem = WideToUtf8(read_error);
    return false;
  }
  std::string parse_error;
  auto json = Json::Parse(bytes, &parse_error);
  if (!json) {
    *problem = parse_error.empty() ? "invalid JSON" : parse_error;
    return false;
  }
  if (!json->is_object()) {
    *problem = "top-level value is not an object";
    return false;
  }
  if (schema != nullptr) *schema = ConfigSchemaOf(*json);
  std::vector<std::string> steps = MigrateConfigJson(&*json);
  if (migration != nullptr) *migration = std::move(steps);
  *config = ConfigFromJson(*json, warnings);
  return true;
}

}

LoadResult ConfigStore::Load() {
  LoadResult result;
  const std::wstring backup = BackupPathFor(path_);
  std::string problem;

  const auto note_save_failure = [&result](const std::wstring& error) {
    result.message = result.message.empty()
                         ? TF(L"{0} Try again.", error)
                         : TF(L"{0} They couldn’t be saved. {1} Try again.", result.message, error);
  };

  if (!PathIsFile(path_)) {
    std::string backup_problem;
    if (PathIsFile(backup) && TryLoad(backup, &result.config, &result.warnings, &backup_problem)) {
      result.outcome = LoadOutcome::kRecoveredFromBackup;
      result.message = T(L"Your settings file was missing. Elixir restored your settings from a backup.");
    } else {
      result.outcome = LoadOutcome::kCreatedDefaults;
    }
    std::wstring error;
    result.saved = SaveRepaired(result.config, &error);
    if (!result.saved) note_save_failure(error);
    return result;
  }

  int schema = kConfigSchemaVersion;
  std::vector<std::string> migration;
  if (TryLoad(path_, &result.config, &result.warnings, &problem, &schema, &migration)) {
    result.outcome = LoadOutcome::kLoaded;
    if (!migration.empty()) {
      result.migrated_from = schema;
      result.migration = std::move(migration);
      std::wstring error;
      result.saved = Save(result.config, &error);
      if (!result.saved) note_save_failure(error);
    }
    return result;
  }

  result.quarantined_path = path_ + L".corrupt-" + UtcFileStamp();
  if (!MoveFileExW(path_.c_str(), result.quarantined_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    if (!CopyFileW(path_.c_str(), result.quarantined_path.c_str(), FALSE)) result.quarantined_path.clear();
  }
  std::string backup_problem;
  result.config = LauncherConfig{};
  result.warnings.clear();
  if (PathIsFile(backup) && TryLoad(backup, &result.config, &result.warnings, &backup_problem)) {
    result.outcome = LoadOutcome::kRecoveredFromBackup;
    result.message = T(L"Your settings file was damaged. Elixir restored your previous settings.");
  } else {
    result.config = LauncherConfig{};
    result.outcome = LoadOutcome::kResetToDefaults;
    result.message = T(L"Your settings file was damaged. Elixir reset your settings to the defaults.");
  }
  std::wstring error;
  result.saved = SaveRepaired(result.config, &error);
  if (!result.saved) note_save_failure(error);
  return result;
}

bool ConfigStore::Save(const LauncherConfig& config, std::wstring* error) {
  return AtomicWriteFile(path_, SerializeConfig(config), AtomicWriteBackup::kKeepPrevious, error);
}

bool ConfigStore::SaveRepaired(const LauncherConfig& config, std::wstring* error) {
  return AtomicWriteFile(path_, SerializeConfig(config), AtomicWriteBackup::kNone, error);
}

}
