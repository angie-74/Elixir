// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_SHADERS_SHADER_TEXT_H_
#define ELIXIR_SHADERS_SHADER_TEXT_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace elixir::shaders {

inline constexpr std::size_t kMaxSourceBytes = 256 * 1024;
inline constexpr std::size_t kMaxParameters = 32;
inline constexpr std::size_t kMaxTextures = 4;
inline constexpr std::size_t kMaxPassesPerShader = 8;
inline constexpr std::size_t kMaxActiveShaders = 16;
inline constexpr float kMinPassScale = 0.0625f;

enum class ShaderStage { kFragment, kVertex, kCompute };

enum class TargetFormat { kRgba8, kRgba16f };

struct Diagnostic {
  int line = 0;
  std::string message;
  bool error = true;
};

struct ShaderParameter {
  std::string name;
  std::string label;
  float initial = 0.0f;
  float minimum = 0.0f;
  float maximum = 1.0f;
  float step = 0.0f;
};

struct ShaderTexture {
  std::string name;
  std::string file;
};

struct ShaderPragmas {
  std::vector<ShaderParameter> parameters;
  std::vector<ShaderTexture> textures;
  float scale = 1.0f;
  bool has_scale = false;
  TargetFormat format = TargetFormat::kRgba8;
};

bool IsShaderIdentifier(std::string_view text);
bool IsPlainFileName(std::string_view text);

bool StageOfFileName(std::string_view file_name, ShaderStage* stage);

bool ParseShaderPragmas(std::string_view source, ShaderPragmas* pragmas, std::vector<Diagnostic>* diagnostics);

float ClampParameter(const ShaderParameter& parameter, float value);

}

#endif
