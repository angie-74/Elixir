// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/gpu_memory.h"

#include <windows.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

#include "graphics/vulkan/device_order.h"
#include "graphics/vulkan/guest_vulkan.h"

namespace elixir::runtime {
namespace {

struct VulkanAdapter {
  VkPhysicalDeviceProperties properties;
  VkPhysicalDeviceIDProperties identity;
};

bool ProbeAdapter(PFN_vkGetInstanceProcAddr get_proc, VkInstance* instance, PFN_vkDestroyInstance* destroy,
                  VulkanAdapter* adapter) {
  const auto create = reinterpret_cast<PFN_vkCreateInstance>(get_proc(VK_NULL_HANDLE, "vkCreateInstance"));
  if (create == nullptr) return false;

  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "Elixir";
  app.apiVersion = VK_API_VERSION_1_1;
  const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
  VkInstanceCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  info.pApplicationInfo = &app;
  info.enabledExtensionCount = static_cast<std::uint32_t>(std::size(extensions));
  info.ppEnabledExtensionNames = extensions;
  if (create(&info, nullptr, instance) != VK_SUCCESS) return false;

  *destroy = reinterpret_cast<PFN_vkDestroyInstance>(get_proc(*instance, "vkDestroyInstance"));
  const auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
      get_proc(*instance, "vkEnumeratePhysicalDevices"));
  const auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
      get_proc(*instance, "vkGetPhysicalDeviceProperties2"));
  const auto queue_families = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
      get_proc(*instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
  const auto can_present = reinterpret_cast<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>(
      get_proc(*instance, "vkGetPhysicalDeviceWin32PresentationSupportKHR"));
  if (*destroy == nullptr || enumerate == nullptr || properties == nullptr || queue_families == nullptr ||
      can_present == nullptr) return false;

  std::array<VkPhysicalDevice, 16> devices{};
  std::uint32_t count = static_cast<std::uint32_t>(devices.size());
  const VkResult enumerated = enumerate(*instance, &count, devices.data());
  if ((enumerated != VK_SUCCESS && enumerated != VK_INCOMPLETE) || count == 0) return false;
  vulkan::DeviceQueries queries;
  queries.properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
      get_proc(*instance, "vkGetPhysicalDeviceProperties"));
  queries.queue_families = queue_families;
  queries.can_present = can_present;
  std::vector<vulkan::DeviceCandidate> candidates;
  for (std::uint32_t index = 0; index < count; ++index) {
    candidates.push_back(vulkan::DescribeDevice(devices[index], queries));
  }
  const VkPhysicalDevice physical_device =
      devices[vulkan::PreferredDevice(vulkan::CachedDeviceOrderPolicy(), candidates)];

  VkPhysicalDeviceProperties2 device{};
  device.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  adapter->identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
  device.pNext = &adapter->identity;
  properties(physical_device, &device);
  adapter->properties = device.properties;
  if (device.properties.apiVersion < VK_API_VERSION_1_1 || !adapter->identity.deviceLUIDValid ||
      (device.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
       device.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)) return false;

  std::array<VkQueueFamilyProperties, 64> families{};
  count = static_cast<std::uint32_t>(families.size());
  queue_families(physical_device, &count, families.data());
  if (count > families.size()) return false;
  bool graphics_queue = false;
  bool present_queue = false;
  for (std::uint32_t index = 0; index < count; ++index) {
    if (families[index].queueCount == 0) continue;
    graphics_queue |= (families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
    present_queue |= can_present(physical_device, index) != VK_FALSE;
  }
  return graphics_queue && present_queue;
}

bool ProbeAdapterGuarded(PFN_vkGetInstanceProcAddr get_proc, VulkanAdapter* adapter) {
  VkInstance instance = VK_NULL_HANDLE;
  PFN_vkDestroyInstance destroy = nullptr;
  bool found = false;
  __try {
    found = ProbeAdapter(get_proc, &instance, &destroy, adapter);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    found = false;
  }
  __try {
    if (destroy != nullptr && instance != VK_NULL_HANDLE) destroy(instance, nullptr);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return found;
}

}

std::optional<VulkanGpuMemory> DetectVulkanGpuMemory() {
  if (!vulkan::InitializeHostVulkan()) return std::nullopt;
  const auto get_proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      vulkan::CurrentHostEntryPoint("vkGetInstanceProcAddr"));
  VulkanAdapter physical_adapter{};
  if (get_proc == nullptr || !ProbeAdapterGuarded(get_proc, &physical_adapter)) return std::nullopt;

  LUID luid{};
  static_assert(sizeof(luid) == VK_LUID_SIZE);
  std::memcpy(&luid, physical_adapter.identity.deviceLUID, sizeof(luid));
  Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) ||
      FAILED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter)))) return std::nullopt;

  DXGI_ADAPTER_DESC1 desc{};
  if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 ||
      desc.VendorId != physical_adapter.properties.vendorID || desc.DeviceId != physical_adapter.properties.deviceID) {
    return std::nullopt;
  }
  const auto& name = physical_adapter.properties.deviceName;
  std::string adapter_name(std::begin(name), std::find(std::begin(name), std::end(name), '\0'));
  if (adapter_name.empty()) return std::nullopt;
  if (desc.DedicatedVideoMemory != 0) {
    return VulkanGpuMemory{std::move(adapter_name), desc.DedicatedVideoMemory,
                           VulkanGpuMemorySource::kDedicatedVideoMemory};
  }
  if (physical_adapter.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) return std::nullopt;

  std::uint32_t node_mask = physical_adapter.identity.deviceNodeMask;
  if (node_mask == 0 || (node_mask & (node_mask - 1)) != 0) return std::nullopt;
  UINT node = 0;
  while ((node_mask >>= 1) != 0) ++node;
  Microsoft::WRL::ComPtr<IDXGIAdapter3> budgeting_adapter;
  DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
  if (FAILED(adapter.As(&budgeting_adapter)) ||
      FAILED(budgeting_adapter->QueryVideoMemoryInfo(node, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory)) ||
      memory.Budget == 0) return std::nullopt;
  return VulkanGpuMemory{std::move(adapter_name), memory.Budget, VulkanGpuMemorySource::kUmaLocalBudget};
}

}
