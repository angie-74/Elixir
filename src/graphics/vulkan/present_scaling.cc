// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "graphics/vulkan/present_scaling.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

#include "graphics/render_scale.h"

namespace elixir::vulkan {
namespace {

std::atomic<bool> g_wanted{false};
std::atomic<bool> g_refused{false};
std::atomic<PresentStretch> g_state{PresentStretch::kUnknown};

bool Enabled(const std::vector<std::string>& names, const char* name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

bool Offered(const std::vector<VkExtensionProperties>& extensions, const char* name) {
  return std::any_of(extensions.begin(), extensions.end(), [&](const VkExtensionProperties& extension) {
    return std::strcmp(extension.extensionName, name) == 0;
  });
}

}

void SetPresentStretchWanted(bool wanted) { g_wanted.store(wanted, std::memory_order_release); }

bool PresentStretchWanted() { return g_wanted.load(std::memory_order_acquire); }

PresentStretch PresentStretchState() {
  if (g_refused.load(std::memory_order_acquire)) return PresentStretch::kUnsupported;
  return g_state.load(std::memory_order_acquire);
}

void NotePresentStretch(bool supported) {
  g_state.store(supported ? PresentStretch::kSupported : PresentStretch::kUnsupported, std::memory_order_release);
}

void NotePresentStretchRefused() { g_refused.store(true, std::memory_order_release); }

void ResetPresentStretchForTest() {
  g_wanted.store(false, std::memory_order_release);
  g_refused.store(false, std::memory_order_release);
  g_state.store(PresentStretch::kUnknown, std::memory_order_release);
}

const char* SwapchainMaintenanceExtension(const std::vector<std::string>& instance_extensions,
                                          const std::vector<VkExtensionProperties>& device_extensions) {
  if (!Enabled(instance_extensions, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME)) return nullptr;
  if (Enabled(instance_extensions, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME) &&
      Offered(device_extensions, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) {
    return VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
  }
  if (Enabled(instance_extensions, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME) &&
      Offered(device_extensions, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) {
    return VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
  }
  return nullptr;
}

VkResult QueryPresentScaling(PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR query,
                             VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                             VkPresentModeKHR present_mode, PresentScaling* scaling) {
  if (query == nullptr || scaling == nullptr || physical_device == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  *scaling = PresentScaling{};
  VkSurfacePresentModeKHR mode{};
  mode.sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_KHR;
  mode.presentMode = present_mode;
  VkPhysicalDeviceSurfaceInfo2KHR info{};
  info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
  info.pNext = &mode;
  info.surface = surface;
  VkSurfacePresentScalingCapabilitiesKHR capabilities{};
  capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_SCALING_CAPABILITIES_KHR;
  VkSurfaceCapabilities2KHR output{};
  output.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
  output.pNext = &capabilities;
  const VkResult result = query(physical_device, &info, &output);
  if (result != VK_SUCCESS) return result;
  scaling->stretches = (capabilities.supportedPresentScaling & VK_PRESENT_SCALING_STRETCH_BIT_KHR) != 0;
  scaling->current_extent = output.surfaceCapabilities.currentExtent;
  return VK_SUCCESS;
}

void ScaleSurfaceCapabilities(float render_scale, VkSurfaceCapabilitiesKHR* capabilities) {
  if (capabilities == nullptr || !RendersSmaller(render_scale)) return;
  VkExtent2D& current = capabilities->currentExtent;
  if (current.width == UINT32_MAX || current.height == UINT32_MAX || current.width == 0 || current.height == 0) {
    return;
  }
  current.width = static_cast<std::uint32_t>(ScaledExtent(current.width, render_scale));
  current.height = static_cast<std::uint32_t>(ScaledExtent(current.height, render_scale));
  capabilities->minImageExtent.width = std::min(capabilities->minImageExtent.width, current.width);
  capabilities->minImageExtent.height = std::min(capabilities->minImageExtent.height, current.height);
  capabilities->maxImageExtent.width = std::max(capabilities->maxImageExtent.width, current.width);
  capabilities->maxImageExtent.height = std::max(capabilities->maxImageExtent.height, current.height);
}

bool NeedsStretch(const VkExtent2D& image, const VkExtent2D& current) {
  if (current.width == UINT32_MAX || current.height == UINT32_MAX) return false;
  if (current.width == 0 || current.height == 0 || image.width == 0 || image.height == 0) return false;
  return image.width != current.width || image.height != current.height;
}

}
