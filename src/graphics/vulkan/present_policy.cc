// Copyright 2026 Mocktail Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from Mocktail's src/graphics/present_mode_policy.cc and
// src/graphics/vulkan_swapchain_policy.cc.
// Modified by Elixir.
#include "graphics/vulkan/present_policy.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace elixir::vulkan {
namespace {

std::string_view EnvironmentOr(const char* name, const char* fallback) {
  const char* value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') return fallback;
  return value;
}

}

PresentModePolicy ResolvePresentModePolicy(std::string_view vsync_value,
                                           std::string_view frame_rate_value,
                                           std::string_view present_mode) {
  if (present_mode == "fifo") return PresentModePolicy::kFifo;
  if (present_mode == "mailbox") return PresentModePolicy::kMailbox;
  if (present_mode == "immediate") return PresentModePolicy::kImmediate;
  if (vsync_value == "on" || vsync_value == "1") return PresentModePolicy::kVsync;
  if (vsync_value == "off" || vsync_value == "0") return PresentModePolicy::kUnthrottled;
  if (frame_rate_value == "unlimited") return PresentModePolicy::kUnthrottled;
  if (frame_rate_value.empty() || frame_rate_value == "-1") return PresentModePolicy::kHostDefault;
  return PresentModePolicy::kVsync;
}

PresentModePolicy CachedPresentModePolicy() {
  static const PresentModePolicy policy = [] {
    return ResolvePresentModePolicy(EnvironmentOr("ELIXIR_VSYNC", "auto"),
                                    EnvironmentOr("ELIXIR_FRAME_RATE_LIMIT", "-1"),
                                    EnvironmentOr("ELIXIR_PRESENT_MODE", "auto"));
  }();
  return policy;
}

std::vector<VkPresentModeKHR> FilterPresentModes(PresentModePolicy policy,
                                                 const std::vector<VkPresentModeKHR>& host_modes) {
  if (policy == PresentModePolicy::kHostDefault || host_modes.empty()) return host_modes;
  const auto supported = [&](VkPresentModeKHR mode) {
    return std::find(host_modes.begin(), host_modes.end(), mode) != host_modes.end();
  };
  if (const auto explicit_mode = ExplicitPresentMode(policy)) {
    if (supported(*explicit_mode)) return {*explicit_mode};
    if (supported(VK_PRESENT_MODE_FIFO_KHR)) return {VK_PRESENT_MODE_FIFO_KHR};
    return {};
  }
  static constexpr VkPresentModeKHR kVsyncModes[] = {
      VK_PRESENT_MODE_FIFO_LATEST_READY_KHR, VK_PRESENT_MODE_MAILBOX_KHR,
      VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR};
  static constexpr VkPresentModeKHR kUnthrottledModes[] = {
      VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR,
      VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR};
  const VkPresentModeKHR* preferred =
      policy == PresentModePolicy::kVsync ? kVsyncModes : kUnthrottledModes;
  for (std::size_t i = 0; i < 4; ++i) {
    if (supported(preferred[i])) return {preferred[i]};
  }
  return host_modes;
}

std::optional<VkPresentModeKHR> ExplicitPresentMode(PresentModePolicy policy) {
  switch (policy) {
    case PresentModePolicy::kFifo: return VK_PRESENT_MODE_FIFO_KHR;
    case PresentModePolicy::kMailbox: return VK_PRESENT_MODE_MAILBOX_KHR;
    case PresentModePolicy::kImmediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
    default: return std::nullopt;
  }
}

const char* PresentModeKhrName(VkPresentModeKHR mode) {
  switch (mode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR: return "immediate";
    case VK_PRESENT_MODE_MAILBOX_KHR: return "mailbox";
    case VK_PRESENT_MODE_FIFO_KHR: return "fifo";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "fifo-relaxed";
    case VK_PRESENT_MODE_FIFO_LATEST_READY_KHR: return "fifo-latest-ready";
    default: return "other";
  }
}

const char* PresentModePolicyName(PresentModePolicy policy) {
  switch (policy) {
    case PresentModePolicy::kHostDefault: return "host-default";
    case PresentModePolicy::kVsync: return "vsync";
    case PresentModePolicy::kUnthrottled: return "unthrottled";
    case PresentModePolicy::kFifo: return "fifo";
    case PresentModePolicy::kMailbox: return "mailbox";
    case PresentModePolicy::kImmediate: return "immediate";
  }
  return "unknown";
}

std::uint32_t PreferSwapchainMinImageCount(PresentModePolicy policy, std::uint32_t requested,
                                           std::uint32_t min_images, std::uint32_t max_images) {
  std::uint32_t count = std::max(requested, min_images);
  if (policy == PresentModePolicy::kUnthrottled) {
    constexpr std::uint32_t kUnthrottledImages = 5;
    count = std::max(count, kUnthrottledImages);
    if (min_images != 0) count = std::max(count, min_images + 2);
  } else if (policy == PresentModePolicy::kVsync) {
    constexpr std::uint32_t kVsyncImages = 4;
    count = std::max(count, kVsyncImages);
    if (min_images != 0) count = std::max(count, min_images + 2);
  } else if (policy == PresentModePolicy::kMailbox) {
    count = std::max(count, 3U);
    if (min_images < UINT32_MAX) count = std::max(count, min_images + 1);
  }
  if (max_images != 0) count = std::min(count, max_images);
  return count;
}

bool EnableSurfaceMaintenanceExtensions(const std::vector<VkExtensionProperties>& supported,
                                        std::vector<std::string>* enabled) {
  if (enabled == nullptr) return false;
  const auto supports = [&](const char* name) {
    return std::any_of(supported.begin(), supported.end(), [&](const VkExtensionProperties& e) {
      return std::strcmp(e.extensionName, name) == 0;
    });
  };
  const char* maintenance = supports(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME)
                                ? VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME
                                : VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
  if (!supports(maintenance) || !supports(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME) ||
      !supports(VK_KHR_SURFACE_EXTENSION_NAME)) {
    return false;
  }
  for (const char* name : {VK_KHR_SURFACE_EXTENSION_NAME,
                           VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, maintenance}) {
    if (std::find(enabled->begin(), enabled->end(), name) == enabled->end()) {
      enabled->emplace_back(name);
    }
  }
  return true;
}

VkResult ApplySwapchainPresentPolicy(PresentModePolicy policy, VkPhysicalDevice physical_device,
                                     const VulkanSurfaceQueries& queries,
                                     VkSwapchainCreateInfoKHR* create_info) {
  if (create_info == nullptr || physical_device == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  VkSwapchainCreateInfoKHR selected = *create_info;
  if (policy != PresentModePolicy::kHostDefault) {
    if (queries.present_modes == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    std::uint32_t count = 0;
    VkResult result = queries.present_modes(physical_device, selected.surface, &count, nullptr);
    if (result != VK_SUCCESS) return result;
    if (count == 0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkPresentModeKHR> modes(count);
    result = queries.present_modes(physical_device, selected.surface, &count, modes.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) return result;
    modes.resize(std::min<std::size_t>(count, modes.size()));
    const auto filtered = FilterPresentModes(policy, modes);
    if (filtered.empty()) return VK_ERROR_INITIALIZATION_FAILED;
    selected.presentMode = filtered.front();
  }

  VkSurfaceCapabilitiesKHR capabilities{};
  VkResult result;
  if (queries.mode_capabilities != nullptr) {
    VkSurfacePresentModeKHR mode{};
    mode.sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_KHR;
    mode.presentMode = selected.presentMode;
    VkPhysicalDeviceSurfaceInfo2KHR info{};
    info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
    info.pNext = &mode;
    info.surface = selected.surface;
    VkSurfaceCapabilities2KHR output{};
    output.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
    result = queries.mode_capabilities(physical_device, &info, &output);
    capabilities = output.surfaceCapabilities;
  } else {
    if (queries.capabilities == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    result = queries.capabilities(physical_device, selected.surface, &capabilities);
  }
  if (result != VK_SUCCESS) return result;

  auto image_policy = policy;
  if (selected.presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
    image_policy = PresentModePolicy::kMailbox;
  } else if (const auto explicit_mode = ExplicitPresentMode(policy);
             explicit_mode && selected.presentMode != *explicit_mode) {
    image_policy = PresentModePolicy::kFifo;
  }
  selected.minImageCount = PreferSwapchainMinImageCount(
      image_policy, selected.minImageCount, capabilities.minImageCount, capabilities.maxImageCount);
  *create_info = selected;
  return VK_SUCCESS;
}

}
