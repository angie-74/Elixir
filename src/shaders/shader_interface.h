// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_SHADERS_SHADER_INTERFACE_H_
#define ELIXIR_SHADERS_SHADER_INTERFACE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "shaders/shader_text.h"

namespace elixir::shaders {

inline constexpr std::uint32_t kMaxBindings = 16;
inline constexpr std::uint32_t kMaxUniformBlockBytes = 4096;

enum class UniformType { kFloat, kInt, kUint, kVec2, kVec3, kVec4, kIvec2, kIvec4, kOther };

struct UniformMember {
  std::string name;
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
  UniformType type = UniformType::kOther;
};

struct UniformBlock {
  std::uint32_t binding = 0;
  std::uint32_t size = 0;
  std::vector<UniformMember> members;
};

struct SamplerBinding {
  std::string name;
  std::uint32_t binding = 0;
};

struct ShaderInterface {
  std::vector<UniformBlock> blocks;
  std::vector<SamplerBinding> samplers;
};

bool ReadShaderInterface(const std::vector<std::uint32_t>& spirv, ShaderStage stage, ShaderInterface* out,
                         std::string* problem);

}

#endif
