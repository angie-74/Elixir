// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "graphics/vulkan/extension_policy.h"

#include <algorithm>
#include <cstring>

#include "graphics/vulkan/generated/vk_sysv_thunks.h"
#include "graphics/vulkan/present_policy.h"

namespace elixir::vulkan {
namespace {

namespace gen = generated;

bool HostHas(const std::vector<VkExtensionProperties>& host, const char* name) {
  return std::any_of(host.begin(), host.end(), [&](const VkExtensionProperties& e) {
    return std::strcmp(e.extensionName, name) == 0;
  });
}

void AppendUnique(std::vector<std::string>* names, std::string_view name) {
  if (std::find(names->begin(), names->end(), name) == names->end()) names->emplace_back(name);
}

}

bool IsHiddenExtension(std::string_view name) {
  if (name == VK_LUNARG_DIRECT_DRIVER_LOADING_EXTENSION_NAME) return true;
  return !gen::IsGuestExtension(name);
}

std::vector<VkExtensionProperties> GuestInstanceExtensions(
    const std::vector<VkExtensionProperties>& host, bool layer_query) {
  std::vector<VkExtensionProperties> guest;
  guest.reserve(host.size() + 1);
  for (const VkExtensionProperties& property : host) {
    if (!IsHiddenExtension(property.extensionName)) guest.push_back(property);
  }
  if (!layer_query && HostHas(host, VK_KHR_SURFACE_EXTENSION_NAME) &&
      HostHas(host, VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
    VkExtensionProperties android{};
    std::strncpy(android.extensionName, kAndroidSurfaceExtension, sizeof(android.extensionName) - 1);
    android.specVersion = kAndroidSurfaceSpecVersion;
    guest.push_back(android);
  }
  return guest;
}

std::vector<VkExtensionProperties> GuestDeviceExtensions(
    const std::vector<VkExtensionProperties>& host) {
  std::vector<VkExtensionProperties> guest;
  guest.reserve(host.size());
  for (const VkExtensionProperties& property : host) {
    if (!IsHiddenExtension(property.extensionName)) guest.push_back(property);
  }
  return guest;
}

VkResult PlanInstanceExtensions(const std::vector<std::string>& guest_requested,
                                const std::vector<VkExtensionProperties>& host_available,
                                InstanceExtensionPlan* plan) {
  if (plan == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  *plan = InstanceExtensionPlan{};
  for (const std::string& name : guest_requested) {
    if (name == kAndroidSurfaceExtension) {
      plan->android_surface = true;
      continue;
    }
    if (IsHiddenExtension(name)) {
      plan->dropped.push_back(name);
      continue;
    }
    AppendUnique(&plan->host_extensions, name);
  }
  if (plan->android_surface) {
    if (!HostHas(host_available, VK_KHR_SURFACE_EXTENSION_NAME) ||
        !HostHas(host_available, VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    AppendUnique(&plan->host_extensions, VK_KHR_SURFACE_EXTENSION_NAME);
    AppendUnique(&plan->host_extensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    EnableSurfaceMaintenanceExtensions(host_available, &plan->host_extensions);
  }
  const auto enabled = [&](const char* name) {
    return std::find(plan->host_extensions.begin(), plan->host_extensions.end(), name) !=
           plan->host_extensions.end();
  };
  plan->surface_maintenance = enabled(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME) &&
                              (enabled(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME) ||
                               enabled(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME));
  return VK_SUCCESS;
}

std::vector<std::string> PlanDeviceExtensions(const std::vector<std::string>& guest_requested,
                                              std::vector<std::string>* dropped) {
  std::vector<std::string> host;
  host.reserve(guest_requested.size());
  for (const std::string& name : guest_requested) {
    if (IsHiddenExtension(name)) {
      if (dropped != nullptr) dropped->push_back(name);
      continue;
    }
    AppendUnique(&host, name);
  }
  return host;
}

}
