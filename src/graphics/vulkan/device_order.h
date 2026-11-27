// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_GRAPHICS_VULKAN_DEVICE_ORDER_H_
#define ELIXIR_GRAPHICS_VULKAN_DEVICE_ORDER_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/vulkan/vulkan_host_headers.h"

namespace elixir::vulkan {

enum class DeviceOrderPolicy {
  kLoader,
  kPreferDiscrete,
  kPreferIntegrated,
};

struct DeviceCandidate {
  VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
  bool can_present = true;
};

struct DeviceQueries {
  PFN_vkGetPhysicalDeviceProperties properties = nullptr;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties queue_families = nullptr;
  PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR can_present = nullptr;
};

DeviceOrderPolicy ResolveDeviceOrderPolicy(std::string_view requested, std::string_view windows_preference);
DeviceOrderPolicy CachedDeviceOrderPolicy();
void SetDeviceOrderPolicyForTest(const DeviceOrderPolicy* policy);
const char* DeviceOrderPolicyName(DeviceOrderPolicy policy);
const char* PhysicalDeviceTypeName(VkPhysicalDeviceType type);

std::size_t PreferredDevice(DeviceOrderPolicy policy, const std::vector<DeviceCandidate>& devices);
DeviceCandidate DescribeDevice(VkPhysicalDevice device, const DeviceQueries& queries, std::string* name = nullptr);

std::string WindowsGpuPreference(std::wstring_view program_path = {});

}

#endif
