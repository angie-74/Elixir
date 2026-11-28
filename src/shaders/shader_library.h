// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_SHADERS_SHADER_LIBRARY_H_
#define ELIXIR_SHADERS_SHADER_LIBRARY_H_

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "shaders/shader_text.h"

namespace elixir::shaders {

inline constexpr wchar_t kLibraryFileName[] = L"shaders.json";
inline constexpr std::size_t kMaxInstalledShaders = 64;

struct InstalledShader {
  std::string name;
  bool folder = false;
  bool enabled = false;
  std::vector<std::filesystem::path> passes;
  std::vector<ShaderParameter> parameters;
  std::map<std::string, float> values;
  std::string problem;
};

struct ShaderLibrary {
  bool enabled = false;
  std::vector<InstalledShader> shaders;
};

ShaderLibrary ReadShaderLibrary(const std::filesystem::path& folder);
bool WriteShaderLibrary(const std::filesystem::path& folder, const ShaderLibrary& library);

float ParameterValue(const InstalledShader& shader, const ShaderParameter& parameter);

bool ReadSourceFile(const std::filesystem::path& file, std::string* text);

std::uint64_t LibraryStamp(const std::filesystem::path& folder);

std::string PathToUtf8(const std::filesystem::path& path);
std::filesystem::path PathFromUtf8(const std::string& text);

}

#endif
