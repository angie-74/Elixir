// Copyright 2026 Mocktail Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from Mocktail's include/mocktail/graphics/present_mode_policy.h,
// vulkan_swapchain_policy.h and android_vulkan_wsi_adapter.h.
// Modified by Elixir.
#ifndef ELIXIR_GRAPHICS_VULKAN_PRESENT_POLICY_H_
#define ELIXIR_GRAPHICS_VULKAN_PRESENT_POLICY_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/vulkan/vulkan_host_headers.h"

namespace elixir::vulkan {

enum class PresentModePolicy {
  kHostDefault,
  kVsync,
  kUnthrottled,
  kFifo,
  kMailbox,
  kImmediate,
};

PresentModePolicy ResolvePresentModePolicy(std::string_view vsync_value,
                                           std::string_view frame_rate_value,
                                           std::string_view present_mode = "auto");
PresentModePolicy CachedPresentModePolicy();

std::optional<VkPresentModeKHR> ExplicitPresentMode(PresentModePolicy policy);

std::vector<VkPresentModeKHR> FilterPresentModes(PresentModePolicy policy,
                                                 const std::vector<VkPresentModeKHR>& host_modes);
const char* PresentModePolicyName(PresentModePolicy policy);
const char* PresentModeKhrName(VkPresentModeKHR mode);

std::uint32_t PreferSwapchainMinImageCount(PresentModePolicy policy, std::uint32_t requested,
                                           std::uint32_t min_images, std::uint32_t max_images);

bool EnableSurfaceMaintenanceExtensions(const std::vector<VkExtensionProperties>& supported,
                                        std::vector<std::string>* enabled);

struct VulkanSurfaceQueries {
  PFN_vkGetPhysicalDeviceSurfacePresentModesKHR present_modes = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR capabilities = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR mode_capabilities = nullptr;
};

VkResult ApplySwapchainPresentPolicy(PresentModePolicy policy, VkPhysicalDevice physical_device,
                                     const VulkanSurfaceQueries& queries,
                                     VkSwapchainCreateInfoKHR* create_info);

inline VkResult NormalizeAndroidSwapchainResult(VkResult result) {
  return result == VK_SUBOPTIMAL_KHR ? VK_SUCCESS : result;
}

}

#endif
