// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "shaders/shader_chain.h"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

#include "shaders/builtin_shaders.h"
#include "shaders/shader_library.h"

namespace elixir::shaders {

namespace {

constexpr std::uint32_t kCacheMagic = 0x53584C45;
constexpr std::size_t kMaxCachedWords = 1u << 20;

std::uint64_t Hash(std::uint64_t seed, std::string_view text) {
  std::uint64_t hash = seed;
  for (const char c : text) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::wstring CacheName(const std::string& source, const CompileOptions& options) {
  const std::string salt = std::string(kCompilerRevision) + "|" + std::to_string(static_cast<int>(options.stage)) +
                           "|" + (options.optimize ? "o" : "n") + "|";
  const std::uint64_t a = Hash(Hash(1469598103934665603ull, salt), source);
  const std::uint64_t b = Hash(Hash(0x9E3779B97F4A7C15ull, source), salt);
  wchar_t name[48];
  std::swprintf(name, 48, L"%016llx%016llx.spv", static_cast<unsigned long long>(a),
                static_cast<unsigned long long>(b));
  return name;
}

bool ReadCache(const std::filesystem::path& file, std::vector<std::uint32_t>* words) {
  std::ifstream input(file, std::ios::binary);
  if (!input) return false;
  std::uint32_t header[2] = {0, 0};
  input.read(reinterpret_cast<char*>(header), sizeof(header));
  if (!input || header[0] != kCacheMagic || header[1] == 0 || header[1] > kMaxCachedWords) return false;
  words->resize(header[1]);
  input.read(reinterpret_cast<char*>(words->data()), static_cast<std::streamsize>(words->size() * 4));
  return static_cast<std::size_t>(input.gcount()) == words->size() * 4;
}

void WriteCache(const std::filesystem::path& file, const std::vector<std::uint32_t>& words) {
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  const std::filesystem::path partial = file.native() + L".part";
  {
    std::ofstream output(partial, std::ios::binary | std::ios::trunc);
    if (!output) return;
    const std::uint32_t header[2] = {kCacheMagic, static_cast<std::uint32_t>(words.size())};
    output.write(reinterpret_cast<const char*>(header), sizeof(header));
    output.write(reinterpret_cast<const char*>(words.data()), static_cast<std::streamsize>(words.size() * 4));
    if (!output) return;
  }
  if (!MoveFileExW(partial.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING)) DeleteFileW(partial.c_str());
}

vulkan::ChainValueKind Kind(UniformType type, bool* usable) {
  *usable = true;
  switch (type) {
    case UniformType::kFloat: return vulkan::ChainValueKind::kFloat;
    case UniformType::kInt: return vulkan::ChainValueKind::kInt;
    case UniformType::kUint: return vulkan::ChainValueKind::kUint;
    case UniformType::kVec2: return vulkan::ChainValueKind::kVec2;
    case UniformType::kVec3: return vulkan::ChainValueKind::kVec3;
    case UniformType::kVec4: return vulkan::ChainValueKind::kVec4;
    case UniformType::kIvec2: return vulkan::ChainValueKind::kIvec2;
    case UniformType::kIvec4: return vulkan::ChainValueKind::kIvec4;
    case UniformType::kOther: break;
  }
  *usable = false;
  return vulkan::ChainValueKind::kFloat;
}

bool ToPass(const CompiledShader& compiled, const std::string& label, int group, vulkan::ChainPass* pass,
            std::string* problem) {
  if (compiled.inputs.blocks.size() > 1) {
    *problem = label + ": a pass can have one uniform block";
    return false;
  }
  pass->label = label;
  pass->fragment = compiled.spirv;
  pass->group = group;
  pass->scale = compiled.pragmas.scale;
  pass->float_target = compiled.pragmas.format == TargetFormat::kRgba16f;
  if (!compiled.inputs.blocks.empty()) {
    const UniformBlock& block = compiled.inputs.blocks.front();
    pass->has_block = true;
    pass->block_binding = block.binding;
    pass->block_size = block.size;
    for (const UniformMember& member : block.members) {
      bool usable = false;
      const vulkan::ChainValueKind kind = Kind(member.type, &usable);
      if (usable && !member.name.empty()) pass->uniforms.push_back(vulkan::ChainUniform{member.name, member.offset, kind});
    }
  }
  for (const SamplerBinding& sampler : compiled.inputs.samplers) {
    pass->samplers.push_back(vulkan::ChainSampler{sampler.name, sampler.binding});
  }
  return true;
}

}

CompiledShader CompileCached(const std::string& source, const std::string& file_name, const CompileOptions& options,
                             const std::filesystem::path& cache) {
  if (cache.empty()) return CompileShader(source, file_name, options);
  const std::filesystem::path file = cache / CacheName(source, options);
  CompiledShader cached;
  if (ReadCache(file, &cached.spirv) && ParseShaderPragmas(source, &cached.pragmas, &cached.diagnostics) &&
      CheckSpirv(cached.spirv, nullptr) &&
      ReadShaderInterface(cached.spirv, options.stage, &cached.inputs, nullptr)) {
    cached.ok = true;
    return cached;
  }
  CompiledShader compiled = CompileShader(source, file_name, options);
  if (compiled.ok) WriteCache(file, compiled.spirv);
  return compiled;
}

ChainBuild BuildFrameChain(const ChainSettings& settings) {
  ChainBuild build;
  auto spec = std::make_shared<vulkan::FrameChainSpec>();
  const std::filesystem::path cache = settings.folder.empty() ? std::filesystem::path() : settings.folder / kCacheFolderName;
  CompileOptions vertex;
  vertex.stage = ShaderStage::kVertex;
  vertex.optimize = settings.optimize;
  CompileOptions fragment;
  fragment.optimize = settings.optimize;
  if (settings.upscale) {
    const CompiledShader scaler = CompileCached(kUpscaleFragmentShader, "upscale.frag", fragment, cache);
    vulkan::ChainPass pass;
    std::string problem;
    if (scaler.ok && ToPass(scaler, "upscaling", -1, &pass, &problem)) {
      pass.scaler = true;
      pass.scale = 1.0f;
      pass.values.emplace_back("sharpness", std::min(std::max(settings.sharpness, 0.0f), 1.0f));
      spec->passes.push_back(std::move(pass));
      build.scaler = true;
    } else {
      build.problems.push_back(scaler.ok ? problem : DescribeDiagnostics(scaler.diagnostics, "upscale.frag"));
    }
  }

  if (!settings.folder.empty()) {
    const ShaderLibrary library = ReadShaderLibrary(settings.folder);
    int group = 0;
    for (const InstalledShader& shader : library.shaders) {
      if (!library.enabled || !shader.enabled) continue;
      if (build.active.size() >= kMaxActiveShaders) {
        build.problems.push_back("at most 16 shaders run at once; " + shader.name + " is left out");
        continue;
      }
      std::vector<vulkan::ChainPass> passes;
      std::string problem;
      for (const std::filesystem::path& file : shader.passes) {
        const std::string name = PathToUtf8(file.filename());
        const std::string label = shader.folder ? shader.name + "/" + name : name;
        std::string source;
        if (!ReadSourceFile(file, &source)) {
          problem = label + ": the file could not be read";
          break;
        }
        const CompiledShader compiled = CompileCached(source, name, fragment, cache);
        if (!compiled.ok) {
          problem = DescribeDiagnostics(compiled.diagnostics, label);
          break;
        }
        vulkan::ChainPass pass;
        if (!ToPass(compiled, label, group, &pass, &problem)) break;
        for (const ShaderParameter& parameter : shader.parameters) {
          pass.values.emplace_back(parameter.name, ParameterValue(shader, parameter));
        }
        passes.push_back(std::move(pass));
      }
      if (!problem.empty() || passes.empty()) {
        build.problems.push_back(problem.empty() ? shader.name + ": no passes" : problem);
        continue;
      }
      for (vulkan::ChainPass& pass : passes) spec->passes.push_back(std::move(pass));
      build.active.push_back(shader.name);
      ++group;
    }
  }
  build.passes = spec->passes.size();
  if (spec->passes.empty()) return build;
  const CompiledShader triangle = CompileCached(kTriangleVertexShader, "triangle.vert", vertex, cache);
  if (!triangle.ok) {
    build.problems.push_back(DescribeDiagnostics(triangle.diagnostics, "triangle.vert"));
    build.passes = 0;
    build.scaler = false;
    build.active.clear();
    return build;
  }
  spec->triangle = triangle.spirv;
  build.spec = std::move(spec);
  return build;
}

std::vector<std::string> RecoverFromFailedStart(const std::filesystem::path& folder) {
  std::vector<std::string> disabled;
  if (folder.empty()) return disabled;
  const std::filesystem::path marker = folder / kCacheFolderName / kStartingFileName;
  std::vector<std::string> names;
  {
    std::ifstream input(marker, std::ios::binary);
    if (!input) return disabled;
    std::string line;
    while (std::getline(input, line) && names.size() < kMaxInstalledShaders) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (IsPlainFileName(line)) names.push_back(line);
    }
  }
  DeleteFileW(marker.c_str());
  if (names.empty()) return disabled;
  ShaderLibrary library = ReadShaderLibrary(folder);
  for (InstalledShader& shader : library.shaders) {
    if (shader.enabled && std::find(names.begin(), names.end(), shader.name) != names.end()) {
      shader.enabled = false;
      disabled.push_back(shader.name);
    }
  }
  if (!disabled.empty()) {
    WriteShaderLibrary(folder, library);
    std::ofstream note(folder / kCacheFolderName / L"recovered.txt", std::ios::binary | std::ios::app);
    for (const std::string& name : disabled) note << name << "\n";
  }
  return disabled;
}

struct ShaderService::Impl {
  ChainSettings settings;
  LogFunction log;
  HANDLE stop = nullptr;
  std::thread worker;
  bool started = false;
  bool marked = false;
  std::size_t last_passes = 0;
  std::uint64_t stamp_at_start = 0;

  std::filesystem::path Marker() const { return settings.folder / kCacheFolderName / kStartingFileName; }

  void Say(const std::string& text) const {
    if (log) log(text);
  }

  void Mark(const std::vector<std::string>& active) {
    if (settings.folder.empty()) return;
    if (active.empty()) {
      DeleteFileW(Marker().c_str());
      marked = false;
      return;
    }
    std::error_code error;
    std::filesystem::create_directories(settings.folder / kCacheFolderName, error);
    std::ofstream output(Marker(), std::ios::binary | std::ios::trunc);
    for (const std::string& name : active) output << name << "\n";
    marked = true;
  }

  void Apply(bool first) {
    ChainBuild build = BuildFrameChain(settings);
    for (const std::string& problem : build.problems) Say("shaders: " + problem);
    const bool wanted = build.spec != nullptr && !build.spec->passes.empty();
    const std::size_t shader_passes = build.passes - (build.scaler ? 1 : 0);
    Mark(build.active);
    vulkan::SetFrameChainSpec(wanted ? std::shared_ptr<const vulkan::FrameChainSpec>(build.spec) : nullptr);
    Say("shaders: " + std::to_string(build.active.size()) + " on (" + std::to_string(shader_passes) + " passes)" +
        (build.scaler ? ", upscaling on" : ", upscaling off") + (first ? "" : "; reloaded"));
    if (!first && shader_passes > 0 && last_passes == 0 && !vulkan::FrameChainsLive()) {
      vulkan::RequestSwapchainRebuild();
    }
    last_passes = shader_passes;
  }

  void Run() {
    std::uint64_t seen = stamp_at_start;
    std::uint64_t pending = seen;
    while (WaitForSingleObject(stop, 500) == WAIT_TIMEOUT) {
      if (marked && vulkan::Chains().Status().frames >= kSettledFrames) {
        DeleteFileW(Marker().c_str());
        marked = false;
      }
      if (settings.folder.empty()) continue;
      const std::uint64_t now = LibraryStamp(settings.folder);
      if (now == seen) {
        pending = seen;
        continue;
      }
      if (now != pending) {
        pending = now;
        continue;
      }
      seen = now;
      try {
        Apply(false);
      } catch (...) {
        Say("shaders: the shaders could not be reloaded");
      }
    }
  }
};

ShaderService::ShaderService(ChainSettings settings, LogFunction log) : impl_(std::make_unique<Impl>()) {
  impl_->settings = std::move(settings);
  impl_->log = std::move(log);
}

ShaderService::~ShaderService() { Stop(); }

void ShaderService::Start() {
  if (impl_->started) return;
  impl_->started = true;
  for (const std::string& name : RecoverFromFailedStart(impl_->settings.folder)) {
    impl_->Say("shaders: " + name + " was turned off because Roblox closed while it was starting last time");
  }
  impl_->stamp_at_start = impl_->settings.folder.empty() ? 0 : LibraryStamp(impl_->settings.folder);
  try {
    impl_->Apply(true);
  } catch (...) {
    impl_->Say("shaders: the shaders could not be loaded");
  }
  impl_->stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (impl_->stop != nullptr) impl_->worker = std::thread([impl = impl_.get()] { impl->Run(); });
}

void ShaderService::Stop() {
  if (!impl_->started) return;
  impl_->started = false;
  if (impl_->worker.joinable()) {
    SetEvent(impl_->stop);
    impl_->worker.join();
  }
  if (impl_->stop != nullptr) CloseHandle(impl_->stop);
  impl_->stop = nullptr;
  if (impl_->marked) DeleteFileW(impl_->Marker().c_str());
  impl_->marked = false;
  vulkan::SetFrameChainSpec(nullptr);
}

}
