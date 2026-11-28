// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_GRAPHICS_VULKAN_FRAME_CHAIN_H_
#define ELIXIR_GRAPHICS_VULKAN_FRAME_CHAIN_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "graphics/vulkan/vulkan_host_headers.h"

namespace elixir::vulkan {

enum class ChainValueKind : std::uint32_t { kFloat, kInt, kUint, kVec2, kVec3, kVec4, kIvec2, kIvec4 };

struct ChainUniform {
  std::string name;
  std::uint32_t offset = 0;
  ChainValueKind kind = ChainValueKind::kFloat;
};

struct ChainSampler {
  std::string name;
  std::uint32_t binding = 0;
};

struct ChainPass {
  std::string label;
  std::vector<std::uint32_t> fragment;
  bool has_block = false;
  std::uint32_t block_binding = 0;
  std::uint32_t block_size = 0;
  std::vector<ChainUniform> uniforms;
  std::vector<ChainSampler> samplers;
  std::vector<std::pair<std::string, float>> values;
  float scale = 1.0f;
  bool float_target = false;
  int group = 0;
  bool scaler = false;
};

struct FrameChainSpec {
  std::vector<std::uint32_t> triangle;
  std::vector<ChainPass> passes;
};

struct FrameChainStatus {
  std::uint64_t chains_created = 0;
  std::uint64_t frames = 0;
  std::uint64_t rebuilds = 0;
  std::uint64_t failures = 0;
  std::uint32_t live_chains = 0;
  std::uint32_t passes = 0;
  std::uint32_t render_width = 0;
  std::uint32_t render_height = 0;
  std::uint32_t window_width = 0;
  std::uint32_t window_height = 0;
  double frame_ms = 0.0;
  double gpu_ms_total = 0.0;
  std::uint64_t gpu_frames = 0;
  std::string last_problem;
};

struct ChainCreateRequest {
  VkDevice device = VK_NULL_HANDLE;
  const VkSwapchainCreateInfoKHR* guest = nullptr;
  VkSwapchainCreateInfoKHR host{};
  VkExtent2D window_extent{};
  PFN_vkCreateSwapchainKHR create = nullptr;
};

void SetFrameChainSpec(std::shared_ptr<const FrameChainSpec> spec);
std::shared_ptr<const FrameChainSpec> CurrentFrameChainSpec();

bool FrameChainWantedFor(VkExtent2D render_extent, VkExtent2D window_extent);

class FrameChains final {
 public:
  void RegisterDevice(VkDevice device, VkPhysicalDevice physical_device,
                      const VkPhysicalDeviceMemoryProperties& memory, PFN_vkGetDeviceProcAddr get_device_proc_addr,
                      float timestamp_period = 0.0f);
  void DestroyDevice(VkDevice device);
  void NoteQueue(VkDevice device, VkQueue queue, std::uint32_t family);

  bool CreateSwapchain(const ChainCreateRequest& request, VkSwapchainKHR* swapchain, VkResult* result);
  bool GetImages(VkSwapchainKHR swapchain, std::uint32_t* count, VkImage* images, VkResult* result);
  void DestroySwapchain(VkSwapchainKHR swapchain);

  bool Present(VkQueue queue, const VkPresentInfoKHR* guest, PFN_vkQueuePresentKHR present, VkResult* result);

  FrameChainStatus Status();
};

FrameChains& Chains();

extern std::uint32_t g_live_frame_chains;
[[gnu::always_inline]] inline bool FrameChainsLive() noexcept {
  return __atomic_load_n(&g_live_frame_chains, __ATOMIC_RELAXED) != 0;
}

void SetFrameChainTiming(bool measure);

void RequestSwapchainRebuild();
bool SwapchainRebuildRequested();
bool TakeSwapchainRebuildRequest();
void NudgeSurfaceCapabilities(VkSurfaceCapabilitiesKHR* capabilities);

}

#endif
