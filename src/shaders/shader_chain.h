// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_SHADERS_SHADER_CHAIN_H_
#define ELIXIR_SHADERS_SHADER_CHAIN_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "graphics/vulkan/frame_chain.h"
#include "shaders/shader_compiler.h"

namespace elixir::shaders {

inline constexpr wchar_t kCacheFolderName[] = L".cache";
inline constexpr wchar_t kStartingFileName[] = L"starting.txt";
inline constexpr std::uint64_t kSettledFrames = 300;

struct ChainSettings {
  std::filesystem::path folder;
  bool upscale = false;
  float sharpness = 0.5f;
  bool optimize = true;
};

struct ChainBuild {
  std::shared_ptr<vulkan::FrameChainSpec> spec;
  std::vector<std::string> problems;
  std::vector<std::string> active;
  std::size_t passes = 0;
  bool scaler = false;
};

CompiledShader CompileCached(const std::string& source, const std::string& file_name, const CompileOptions& options,
                             const std::filesystem::path& cache);

ChainBuild BuildFrameChain(const ChainSettings& settings);

std::vector<std::string> RecoverFromFailedStart(const std::filesystem::path& folder);

class ShaderService final {
 public:
  using LogFunction = std::function<void(const std::string&)>;

  ShaderService(ChainSettings settings, LogFunction log);
  ~ShaderService();
  ShaderService(const ShaderService&) = delete;
  ShaderService& operator=(const ShaderService&) = delete;

  void Start();
  void Stop();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}

#endif
