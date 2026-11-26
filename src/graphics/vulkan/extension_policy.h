// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_GRAPHICS_VULKAN_EXTENSION_POLICY_H_
#define ELIXIR_GRAPHICS_VULKAN_EXTENSION_POLICY_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/vulkan/vulkan_host_headers.h"

namespace elixir::vulkan {

inline constexpr char kAndroidSurfaceExtension[] = "VK_KHR_android_surface";
inline constexpr std::uint32_t kAndroidSurfaceSpecVersion = 6;

bool IsHiddenExtension(std::string_view name);

std::vector<VkExtensionProperties> GuestInstanceExtensions(
    const std::vector<VkExtensionProperties>& host, bool layer_query);

std::vector<VkExtensionProperties> GuestDeviceExtensions(
    const std::vector<VkExtensionProperties>& host);

struct InstanceExtensionPlan {
  std::vector<std::string> host_extensions;
  std::vector<std::string> dropped;
  bool android_surface = false;
  bool surface_maintenance = false;
};

VkResult PlanInstanceExtensions(const std::vector<std::string>& guest_requested,
                                const std::vector<VkExtensionProperties>& host_available,
                                InstanceExtensionPlan* plan);

std::vector<std::string> PlanDeviceExtensions(const std::vector<std::string>& guest_requested,
                                              std::vector<std::string>* dropped);

template <typename T>
VkResult CopyEnumeration(const std::vector<T>& values, std::uint32_t* count, T* output) {
  if (count == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  if (output == nullptr) {
    *count = static_cast<std::uint32_t>(values.size());
    return VK_SUCCESS;
  }
  const std::uint32_t written = *count < values.size() ? *count : static_cast<std::uint32_t>(values.size());
  for (std::uint32_t i = 0; i < written; ++i) output[i] = values[i];
  *count = written;
  return written < values.size() ? VK_INCOMPLETE : VK_SUCCESS;
}

}

#endif
