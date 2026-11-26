// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The presets use Fishstrap's flags and values (Bloxstrap/FastFlagManager.cs,
// MIT).
#include <algorithm>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "fflags/fflags.h"

namespace elixir::fflags {
namespace {

Preset MakePreset(std::string id, std::string group, std::string title, std::string description,
                  PresetControl control, const char* off_label, std::vector<std::string> flags,
                  std::initializer_list<std::pair<const char*, std::vector<Flag>>> options) {
  Preset preset{std::move(id), std::move(group), std::move(title), std::move(description), {}};
  preset.control = control;
  PresetOption off{off_label, {}, flags};
  preset.options.push_back(std::move(off));
  for (const auto& [label, set] : options) {
    PresetOption option{label, set, {}};
    for (const std::string& name : flags) {
      const bool written = std::any_of(set.begin(), set.end(), [&](const Flag& f) { return f.name == name; });
      if (!written) option.remove.push_back(name);
    }
    preset.options.push_back(std::move(option));
  }
  return preset;
}

constexpr const char* kGeometry = "Geometry";
constexpr const char* kRendering = "Rendering and Graphics";

std::vector<Preset> BuildPresets() {
  std::vector<Preset> presets;

  {
    const std::vector<std::string> lod = {"DFIntCSGLevelOfDetailSwitchingDistance",
                                          "DFIntCSGLevelOfDetailSwitchingDistanceL12",
                                          "DFIntCSGLevelOfDetailSwitchingDistanceL23",
                                          "DFIntCSGLevelOfDetailSwitchingDistanceL34"};
    const auto level = [&](int v) {
      std::vector<Flag> set;
      for (int i = 0; i < 4; ++i) set.push_back({lod[i], std::to_string((std::clamp(v - i, 0, 3) + 1) * 250)});
      return set;
    };
    Preset mesh = MakePreset("mesh_detail", kGeometry, "Mesh detail", "Control how detailed meshes appear in-game.",
                             PresetControl::kToggleSlider, "Off", lod,
                             {{"Lowest", level(0)}, {"Low", level(1)}, {"High", level(2)}, {"Highest", level(3)}});
    mesh.low_label = "Lowest quality";
    mesh.high_label = "Highest quality";
    mesh.note = "Lower qualities may cause meshes to disappear.";
    presets.push_back(std::move(mesh));
  }

  {
    presets.push_back(MakePreset("msaa", kRendering, "Anti-aliasing quality (MSAA)", "", PresetControl::kChoice,
                                 "Automatic", {"FIntDebugForceMSAASamples"},
                                 {{"1x", {{"FIntDebugForceMSAASamples", "1"}}},
                                  {"2x", {{"FIntDebugForceMSAASamples", "2"}}},
                                  {"4x", {{"FIntDebugForceMSAASamples", "4"}}}}));
  }

  presets.push_back(MakePreset(
      "dpi_scale", kRendering, "Preserve rendering quality with display scaling",
      "Roblox reduces your rendering quality depending on how your display is scaled in Windows.",
      PresetControl::kToggle, "Off", {"DFFlagDisableDPIScale"}, {{"On", {{"DFFlagDisableDPIScale", "True"}}}}));

  {
    Preset quality = MakePreset("graphics_quality", kRendering, "Graphics quality override",
                                "Choose the graphics quality Roblox should use.", PresetControl::kToggleSlider, "Off",
                                {"DFIntDebugFRMQualityLevelOverride"}, {});
    for (int level = 1; level <= 21; ++level) {
      quality.options.push_back(
          PresetOption{std::to_string(level), {{"DFIntDebugFRMQualityLevelOverride", std::to_string(level)}}, {}});
    }
    quality.low_label = "Lowest quality";
    quality.high_label = "Highest quality";
    quality.show_value = true;
    presets.push_back(std::move(quality));
  }

  presets.push_back(MakePreset("grass", kRendering, "Hide grass", "Turns grass off, which can run faster.",
                               PresetControl::kToggle, "Off", {"FIntFRMMinGrassDistance", "FIntFRMMaxGrassDistance"},
                               {{"On", {{"FIntFRMMinGrassDistance", "0"}, {"FIntFRMMaxGrassDistance", "0"}}}}));
  presets.push_back(MakePreset("gray_sky", kRendering, "Gray sky", "Replaces the sky with a plain gray one.",
                               PresetControl::kToggle, "Off", {"FFlagDebugSkyGray"},
                               {{"On", {{"FFlagDebugSkyGray", "True"}}}}));
  presets.push_back(MakePreset("pause_voxelizer", kRendering, "Pause lighting updates",
                               "Can run faster, but lighting may not update when the world changes.",
                               PresetControl::kToggle, "Off", {"DFFlagDebugPauseVoxelizer"},
                               {{"On", {{"DFFlagDebugPauseVoxelizer", "True"}}}}));
  return presets;
}

}

const std::vector<Preset>& BuiltinPresets() {
  static const std::vector<Preset> presets = BuildPresets();
  return presets;
}

}
