// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_GPU_MEMORY_H_
#define ELIXIR_RUNTIME_GPU_MEMORY_H_

#include <cstdint>
#include <optional>
#include <string>

namespace elixir::runtime {

enum class VulkanGpuMemorySource { kDedicatedVideoMemory, kUmaLocalBudget };

struct VulkanGpuMemory {
  std::string adapter_name;
  std::uint64_t bytes;
  VulkanGpuMemorySource source;
};

std::optional<VulkanGpuMemory> DetectVulkanGpuMemory();

}

#endif
