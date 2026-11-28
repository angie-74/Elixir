// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_SHADERS_SHADER_COMPILER_H_
#define ELIXIR_SHADERS_SHADER_COMPILER_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "shaders/shader_interface.h"
#include "shaders/shader_text.h"

namespace elixir::shaders {

inline constexpr char kCompilerRevision[] = "elixir-shaders-1 glslang-vulkan-sdk-1.4.363.0";

struct CompileOptions {
  ShaderStage stage = ShaderStage::kFragment;
  bool optimize = true;
};

struct CompiledShader {
  bool ok = false;
  std::vector<std::uint32_t> spirv;
  ShaderInterface inputs;
  ShaderPragmas pragmas;
  std::vector<Diagnostic> diagnostics;
};

CompiledShader CompileShader(std::string_view source, std::string_view file_name, const CompileOptions& options);

bool CheckSpirv(const std::vector<std::uint32_t>& spirv, std::string* problem);

std::string DescribeDiagnostics(const std::vector<Diagnostic>& diagnostics, std::string_view file_name);

}

#endif
