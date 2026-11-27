// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_GRAPHICS_VULKAN_PRESENT_SCALING_H_
#define ELIXIR_GRAPHICS_VULKAN_PRESENT_SCALING_H_

#include <string>
#include <vector>

#include "graphics/render_scale.h"
#include "graphics/vulkan/vulkan_host_headers.h"

namespace elixir::vulkan {

enum class PresentStretch {
  kUnknown,
  kUnsupported,
  kSupported,
};

void SetPresentStretchWanted(bool wanted);
bool PresentStretchWanted();

PresentStretch PresentStretchState();
void NotePresentStretch(bool supported);
void NotePresentStretchRefused();
void ResetPresentStretchForTest();

const char* SwapchainMaintenanceExtension(const std::vector<std::string>& instance_extensions,
                                          const std::vector<VkExtensionProperties>& device_extensions);

struct PresentScaling {
  bool stretches = false;
  VkExtent2D current_extent{};
};

VkResult QueryPresentScaling(PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR query,
                             VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                             VkPresentModeKHR present_mode, PresentScaling* scaling);

bool NeedsStretch(const VkExtent2D& image, const VkExtent2D& current);

void ScaleSurfaceCapabilities(const RenderScale& render_scale, VkSurfaceCapabilitiesKHR* capabilities);

}

#endif
