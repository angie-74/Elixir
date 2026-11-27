// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "graphics/vulkan/device_order.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iterator>

namespace elixir::vulkan {
namespace {

constexpr wchar_t kPreferencesKey[] = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";
constexpr std::string_view kPreferenceName = "GpuPreference=";
constexpr long kWindowsDecides = 0;
constexpr long kPowerSaving = 1;
constexpr long kHighPerformance = 2;

std::atomic<const DeviceOrderPolicy*> g_test_policy{nullptr};

std::string EnvironmentValue(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr ? value : "";
}

bool WindowsPreferenceNumber(std::string_view preference, long* number) {
  const std::size_t at = preference.find(kPreferenceName);
  if (at == std::string_view::npos) return false;
  std::size_t index = at + kPreferenceName.size();
  if (index >= preference.size() || preference[index] < '0' || preference[index] > '9') return false;
  constexpr long kTooLarge = 100000000;
  long value = 0;
  for (; index < preference.size() && preference[index] >= '0' && preference[index] <= '9'; ++index) {
    value = value < kTooLarge ? value * 10 + (preference[index] - '0') : kTooLarge * 10;
  }
  *number = value;
  return true;
}

}

DeviceOrderPolicy ResolveDeviceOrderPolicy(std::string_view requested, std::string_view windows_preference) {
  if (requested == "loader") return DeviceOrderPolicy::kLoader;
  if (requested == "discrete") return DeviceOrderPolicy::kPreferDiscrete;
  if (requested == "integrated") return DeviceOrderPolicy::kPreferIntegrated;
  long number = kWindowsDecides;
  if (!WindowsPreferenceNumber(windows_preference, &number)) return DeviceOrderPolicy::kPreferDiscrete;
  if (number == kWindowsDecides || number == kHighPerformance) return DeviceOrderPolicy::kPreferDiscrete;
  if (number == kPowerSaving) return DeviceOrderPolicy::kPreferIntegrated;
  return DeviceOrderPolicy::kLoader;
}

DeviceOrderPolicy CachedDeviceOrderPolicy() {
  if (const DeviceOrderPolicy* forced = g_test_policy.load(std::memory_order_acquire)) return *forced;
  static const DeviceOrderPolicy policy =
      ResolveDeviceOrderPolicy(EnvironmentValue("ELIXIR_GPU_ORDER"), WindowsGpuPreference());
  return policy;
}

void SetDeviceOrderPolicyForTest(const DeviceOrderPolicy* policy) {
  g_test_policy.store(policy, std::memory_order_release);
}

const char* DeviceOrderPolicyName(DeviceOrderPolicy policy) {
  switch (policy) {
    case DeviceOrderPolicy::kLoader: return "as Windows lists them";
    case DeviceOrderPolicy::kPreferDiscrete: return "the dedicated card first";
    case DeviceOrderPolicy::kPreferIntegrated: return "the power-saving card first";
  }
  return "unknown";
}

const char* PhysicalDeviceTypeName(VkPhysicalDeviceType type) {
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "dedicated";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "software";
    default: return "other";
  }
}

std::size_t PreferredDevice(DeviceOrderPolicy policy, const std::vector<DeviceCandidate>& devices) {
  if (policy == DeviceOrderPolicy::kLoader || devices.size() < 2) return 0;
  const VkPhysicalDeviceType wanted = policy == DeviceOrderPolicy::kPreferDiscrete
                                          ? VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                                          : VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
  for (std::size_t index = 0; index < devices.size(); ++index) {
    if (devices[index].type == wanted && devices[index].can_present) return index;
  }
  return 0;
}

DeviceCandidate DescribeDevice(VkPhysicalDevice device, const DeviceQueries& queries, std::string* name) {
  DeviceCandidate candidate;
  if (queries.properties != nullptr) {
    VkPhysicalDeviceProperties properties{};
    queries.properties(device, &properties);
    candidate.type = properties.deviceType;
    if (name != nullptr) {
      const char* begin = properties.deviceName;
      name->assign(begin, std::find(begin, begin + sizeof(properties.deviceName), '\0'));
    }
  }
  if (queries.queue_families == nullptr || queries.can_present == nullptr) return candidate;
  std::array<VkQueueFamilyProperties, 32> families{};
  std::uint32_t count = static_cast<std::uint32_t>(families.size());
  queries.queue_families(device, &count, families.data());
  if (count > families.size()) count = static_cast<std::uint32_t>(families.size());
  candidate.can_present = false;
  for (std::uint32_t index = 0; index < count; ++index) {
    if (families[index].queueCount == 0 || (families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) continue;
    if (queries.can_present(device, index) != VK_FALSE) {
      candidate.can_present = true;
      break;
    }
  }
  return candidate;
}

std::string WindowsGpuPreference(std::wstring_view program_path) {
  std::wstring program(program_path);
  if (program.empty()) {
    program.resize(32768);
    const DWORD length = GetModuleFileNameW(nullptr, program.data(), static_cast<DWORD>(program.size()));
    if (length == 0 || length >= program.size()) return {};
    program.resize(length);
  }
  wchar_t data[512] = {};
  DWORD bytes = sizeof(data) - sizeof(wchar_t);
  if (RegGetValueW(HKEY_CURRENT_USER, kPreferencesKey, program.c_str(), RRF_RT_REG_SZ, nullptr, data, &bytes) !=
      ERROR_SUCCESS) {
    return {};
  }
  std::string text;
  for (const wchar_t* at = data; at != std::end(data) && *at != L'\0'; ++at) {
    text.push_back(*at < 128 ? static_cast<char>(*at) : '?');
  }
  return text;
}

}
