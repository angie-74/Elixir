// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/runtime_flags.h"

#include <windows.h>

#define JSON_NOEXCEPTION 1
#include <nlohmann/json.hpp>

#include "elixir_runtime/text.h"
#include "runtime/crash_report_policy.h"
#include "runtime/http_client_policy.h"
#include "runtime/roblox_desktop_app_policy.h"
#include "runtime/texture_memory_policy.h"

namespace elixir::runtime {
namespace {

using elixir::fflags::FlagSet;
namespace mr = mocktail::runtime;

constexpr char kDesktopAppPolicyOverride[] = "FStringAppConfigurationOverrideAppPolicy";

bool MergeInto(FlagSet* target, const std::string& policy_json, const char* policy, std::string* error,
               const char* skip = nullptr, FlagSet* skipped_target = nullptr) {
  const nlohmann::json object = nlohmann::json::parse(policy_json, nullptr, false, false);
  if (object.is_discarded() || !object.is_object()) {
    *error = std::string(policy) + " policy produced no JSON object";
    return false;
  }
  for (const auto& [name, value] : object.items()) {
    if (!value.is_string()) {
      *error = std::string(policy) + " policy produced a non-string value for " + name;
      return false;
    }
    FlagSet* destination = target;
    if (skip != nullptr && name == skip) {
      if (skipped_target == nullptr) continue;
      destination = skipped_target;
    }
    const std::string text = value.get<std::string>();
    if (const std::string* existing = destination->Find(name); existing != nullptr && *existing != text) {
      *error = "runtime policies disagree on " + name + " (\"" + *existing + "\" and \"" + text + "\")";
      return false;
    }
    destination->Set(name, text);
  }
  return true;
}

bool FileExists(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

nlohmann::ordered_json FlagObject(const FlagSet& flags) {
  nlohmann::ordered_json object = nlohmann::ordered_json::object();
  for (const elixir::fflags::Flag& flag : flags.flags()) object[flag.name] = flag.value;
  return object;
}

}

const char* FlagStatusId(elixir::fflags::FlagStatus status) {
  switch (status) {
    case elixir::fflags::FlagStatus::kApplied: return "applied";
    case elixir::fflags::FlagStatus::kNotAllowlisted: return "not_allowlisted";
    case elixir::fflags::FlagStatus::kInvalidValue: return "invalid_value";
    case elixir::fflags::FlagStatus::kOverriddenByRuntime: return "managed_by_elixir";
  }
  return "unknown";
}

RuntimeFlagSets BuildRuntimeFlagSets(const RuntimeFlagInputs& inputs) {
  RuntimeFlagSets sets;
  std::string json;
  std::string error;
  const auto fail = [&sets](std::string message) {
    sets.error = std::move(message);
    return sets;
  };
  if (!mr::MergeFrameRateClientSettingsOverrides(inputs.frame_rate, "{}", &json, &error) ||
      !MergeInto(&sets.mandatory, json, "frame-rate", &error)) {
    return fail("frame-rate policy: " + error);
  }
  if (!mr::MergePerformanceClientSettingsOverrides(inputs.performance, "{}", &json, &error) ||
      !MergeInto(&sets.mandatory, json, "performance", &error)) {
    return fail("performance policy: " + error);
  }
  if (!mr::MergeHttpClientSettingsOverrides("{}", &json, &error) ||
      !MergeInto(&sets.mandatory, json, "HttpClient", &error)) {
    return fail("HttpClient policy: " + error);
  }
  if (!mr::MergeCrashReportClientSettingsOverrides("{}", &json, &error) ||
      !MergeInto(&sets.mandatory, json, "crash-report", &error)) {
    return fail("crash-report policy: " + error);
  }
  if (!mr::MergeAudioCaptureClientSettingsOverrides(inputs.microphone_enabled, "{}", &json, &error) ||
      !MergeInto(&sets.mandatory, json, "microphone", &error)) {
    return fail("microphone permission policy: " + error);
  }
  if (inputs.gpu_memory_bytes != 0 &&
      (!mr::MergeTextureMemoryClientSettingsOverrides(inputs.gpu_memory_bytes, "{}", &json, &error) ||
       !MergeInto(&sets.defaults, json, "texture-memory", &error))) {
    return fail("texture-memory policy: " + error);
  }
  if (inputs.desktop_playability) {
    const std::string policy = inputs.desktop_app_policy_json.value_or("{}");
    if (!mr::MergeDesktopAppPolicyClientSettingsOverride(policy, "{}", &json, &error)) {
      return fail("desktop app-policy: " + error);
    }
    if (inputs.desktop_app_policy_json.has_value()) {
      if (!MergeInto(&sets.defaults, json, "desktop app-policy", &error, kDesktopAppPolicyOverride,
                     &sets.mandatory)) {
        return fail("desktop app-policy: " + error);
      }
    } else {
      if (!MergeInto(&sets.defaults, json, "desktop app-policy", &error, kDesktopAppPolicyOverride, nullptr)) {
        return fail("desktop app-policy: " + error);
      }
      sets.pending.push_back(
          {kDesktopAppPolicyOverride,
           "set at start-up from Roblox's cached app policy (appStorage.json) or the APK's "
           "guac/defaultConfigs/GuacDefaultPolicy-GlobalDist.json; not computed in this mode"});
    }
  }
  return sets;
}

UserFlags LoadUserFlags(const std::filesystem::path& path) {
  UserFlags user;
  user.path = path;
  if (path.empty()) return user;
  const elixir::fflags::ParseResult parsed = elixir::fflags::LoadFlagsFile(path);
  if (!parsed.ok) {
    user.error = parsed.error;
    if (FileExists(path)) {
      user.error += " (at byte " + std::to_string(parsed.error_offset) + ")";
    }
    return user;
  }
  user.loaded = FileExists(path);
  user.flags = parsed.flags;
  user.warnings = parsed.warnings;
  return user;
}

std::string RuntimeFlagsJson(const RuntimeFlagInputs& inputs, const RuntimeFlagSets& sets, const UserFlags& user,
                             const elixir::fflags::Allowlist& allowlist,
                             const elixir::fflags::Resolution& resolution, const std::string& device_profile) {
  nlohmann::ordered_json document = nlohmann::ordered_json::object();
  document["schema"] = 1;
  document["device_profile"] = device_profile;
  nlohmann::ordered_json input = nlohmann::ordered_json::object();
  input["fps_limit"] = inputs.frame_rate.mode == mr::FrameRateLimitMode::kFixed ? inputs.frame_rate.fixed_fps : 0;
  input["physics_worker_mode"] = std::string(mr::PhysicsWorkerModeName(inputs.performance.physics_worker_mode));
  input["multithreaded_rendering"] = inputs.performance.multithreaded_rendering;
  input["physical_cores"] = inputs.performance.physical_core_count;
  input["host_memory_mib"] = inputs.host_memory_bytes / (1024ULL * 1024ULL);
  input["gpu_memory_bytes"] = inputs.gpu_memory_bytes;
  if (inputs.gpu_memory_bytes != 0) {
    input["gpu_adapter"] = inputs.gpu_adapter_name;
    input["gpu_memory_source"] = inputs.gpu_memory_source;
  }
  input["microphone_enabled"] = inputs.microphone_enabled;
  document["inputs"] = input;
  document["mandatory"] = FlagObject(sets.mandatory);
  document["defaults"] = FlagObject(sets.defaults);
  document["effective"] = FlagObject(resolution.effective);
  nlohmann::ordered_json reports = nlohmann::ordered_json::array();
  for (const elixir::fflags::FlagReport& report : resolution.user) {
    nlohmann::ordered_json entry = nlohmann::ordered_json::object();
    entry["name"] = report.name;
    entry["value"] = report.value;
    entry["status"] = FlagStatusId(report.status);
    entry["status_text"] = elixir::fflags::FlagStatusName(report.status);
    entry["detail"] = report.detail;
    reports.push_back(entry);
  }
  document["user"] = reports;
  nlohmann::ordered_json file = nlohmann::ordered_json::object();
  file["path"] = NativeUtf8(user.path);
  file["loaded"] = user.loaded;
  file["error"] = user.error;
  file["warnings"] = user.warnings;
  document["user_file"] = file;
  nlohmann::ordered_json list = nlohmann::ordered_json::object();
  list["source"] = allowlist.source();
  list["updated"] = allowlist.source_updated();
  list["count"] = allowlist.entries().size();
  document["allowlist"] = list;
  nlohmann::ordered_json pending = nlohmann::ordered_json::array();
  for (const PendingRuntimeFlag& flag : sets.pending) {
    nlohmann::ordered_json entry = nlohmann::ordered_json::object();
    entry["name"] = flag.name;
    entry["reason"] = flag.reason;
    pending.push_back(entry);
  }
  document["pending"] = pending;
  return document.dump(2, ' ', false, nlohmann::ordered_json::error_handler_t::replace) + "\n";
}

}
