// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "shaders/shader_compiler.h"

#include <cstdlib>
#include <mutex>
#include <utility>

#include "SPIRV/GlslangToSpv.h"
#include "glslang/Public/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include "spirv-tools/libspirv.hpp"
#include "spirv-tools/optimizer.hpp"

namespace elixir::shaders {

namespace {

std::mutex& CompilerMutex() {
  static std::mutex mutex;
  return mutex;
}

void EnsureProcess() {
  static const bool started = glslang::InitializeProcess();
  static_cast<void>(started);
}

EShLanguage Language(ShaderStage stage) {
  switch (stage) {
    case ShaderStage::kVertex: return EShLangVertex;
    case ShaderStage::kCompute: return EShLangCompute;
    case ShaderStage::kFragment: break;
  }
  return EShLangFragment;
}

void AddLog(std::string_view log, std::vector<Diagnostic>* diagnostics) {
  std::size_t begin = 0;
  while (begin < log.size()) {
    std::size_t end = log.find('\n', begin);
    if (end == std::string_view::npos) end = log.size();
    std::string_view line = log.substr(begin, end - begin);
    begin = end + 1;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.remove_suffix(1);
    if (line.empty()) continue;
    Diagnostic diagnostic;
    if (line.substr(0, 9) == "WARNING: ") {
      diagnostic.error = false;
      line.remove_prefix(9);
    } else if (line.substr(0, 7) == "ERROR: ") {
      line.remove_prefix(7);
    } else {
      continue;
    }
    const std::size_t first = line.find(':');
    const std::size_t second = first == std::string_view::npos ? first : line.find(':', first + 1);
    if (second != std::string_view::npos) {
      const std::string number(line.substr(first + 1, second - first - 1));
      char* stop = nullptr;
      const long parsed = std::strtol(number.c_str(), &stop, 10);
      if (stop != number.c_str() && *stop == '\0' && parsed >= 0 && parsed < 1000000) {
        diagnostic.line = static_cast<int>(parsed);
        line.remove_prefix(second + 1);
        while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
      }
    }
    if (line.find("compilation errors") != std::string_view::npos ||
        line.find("compilation terminated") != std::string_view::npos) {
      continue;
    }
    diagnostic.message = std::string(line.substr(0, 400));
    if (diagnostics->size() < 24) diagnostics->push_back(std::move(diagnostic));
  }
}

}

bool CheckSpirv(const std::vector<std::uint32_t>& spirv, std::string* problem) {
  spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_0);
  std::string first;
  tools.SetMessageConsumer(
      [&first](spv_message_level_t level, const char*, const spv_position_t&, const char* message) {
        if (level <= SPV_MSG_ERROR && first.empty() && message != nullptr) first = message;
      });
  if (tools.Validate(spirv)) return true;
  if (problem != nullptr) *problem = first.empty() ? std::string("the compiled shader is not valid") : first;
  return false;
}

CompiledShader CompileShader(std::string_view source, std::string_view file_name, const CompileOptions& options) {
  CompiledShader result;
  if (!ParseShaderPragmas(source, &result.pragmas, &result.diagnostics)) return result;
  if (options.stage == ShaderStage::kCompute) {
    result.diagnostics.push_back(Diagnostic{0, "compute shaders are not run by this version of Elixir", true});
    return result;
  }
  const std::string text(source);
  const std::string name(file_name);
  std::vector<std::uint32_t> spirv;
  {
    std::lock_guard<std::mutex> lock(CompilerMutex());
    EnsureProcess();
    const EShLanguage language = Language(options.stage);
    glslang::TShader shader(language);
    const char* strings[] = {text.c_str()};
    const int lengths[] = {static_cast<int>(text.size())};
    const char* names[] = {name.c_str()};
    shader.setStringsWithLengthsAndNames(strings, lengths, names, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    const EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
    const bool parsed = shader.parse(GetDefaultResources(), 450, ENoProfile, false, false, messages);
    AddLog(shader.getInfoLog(), &result.diagnostics);
    if (!parsed) {
      if (result.diagnostics.empty()) result.diagnostics.push_back(Diagnostic{0, "the shader did not compile", true});
      return result;
    }
    glslang::TProgram program;
    program.addShader(&shader);
    const bool linked = program.link(messages);
    AddLog(program.getInfoLog(), &result.diagnostics);
    if (!linked || program.getIntermediate(language) == nullptr) {
      if (result.diagnostics.empty()) result.diagnostics.push_back(Diagnostic{0, "the shader did not link", true});
      return result;
    }
    glslang::SpvOptions spv_options;
    spv_options.disableOptimizer = true;
    spv_options.validate = false;
    spv::SpvBuildLogger logger;
    glslang::GlslangToSpv(*program.getIntermediate(language), spirv, &logger, &spv_options);
  }
  std::string problem;
  if (spirv.empty() || !CheckSpirv(spirv, &problem)) {
    result.diagnostics.push_back(
        Diagnostic{0, problem.empty() ? std::string("the shader produced no code") : problem, true});
    return result;
  }
  if (options.optimize) {
    spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_0);
    optimizer.SetMessageConsumer([](spv_message_level_t, const char*, const spv_position_t&, const char*) {});
    optimizer.RegisterPerformancePasses();
    std::vector<std::uint32_t> optimized;
    if (optimizer.Run(spirv.data(), spirv.size(), &optimized) && !optimized.empty() &&
        CheckSpirv(optimized, nullptr)) {
      spirv = std::move(optimized);
    }
  }
  if (!ReadShaderInterface(spirv, options.stage, &result.inputs, &problem)) {
    result.diagnostics.push_back(Diagnostic{0, problem, true});
    return result;
  }
  result.spirv = std::move(spirv);
  result.ok = true;
  return result;
}

std::string DescribeDiagnostics(const std::vector<Diagnostic>& diagnostics, std::string_view file_name) {
  std::string out;
  for (const Diagnostic& diagnostic : diagnostics) {
    if (!out.empty()) out += "\n";
    out += std::string(file_name);
    if (diagnostic.line > 0) out += ":" + std::to_string(diagnostic.line);
    out += diagnostic.error ? ": " : ": warning: ";
    out += diagnostic.message;
  }
  return out;
}

}
