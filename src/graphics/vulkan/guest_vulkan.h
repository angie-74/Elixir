// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_GRAPHICS_VULKAN_GUEST_VULKAN_H_
#define ELIXIR_GRAPHICS_VULKAN_GUEST_VULKAN_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "abi/sysv_abi.h"
#include "graphics/vulkan/vulkan_host_headers.h"

namespace elixir::vulkan {

void RegisterNativeWindow(const void* anative_window, HWND hwnd);
void UnregisterNativeWindow(const void* anative_window);
HWND FindNativeWindow(const void* anative_window);

using PresentHook = void (*)(VkResult result);
void SetPresentHook(PresentHook hook);
std::uint64_t PresentedFrameCount();

bool InitializeHostVulkan(std::string* error = nullptr);

void RegisterVulkanExports(std::unordered_map<std::string, void*>& exports);

using GuestGetInstanceProcAddrFn = abi::SysvFn<PFN_vkVoidFunction(VkInstance, const char*)>;
GuestGetInstanceProcAddrFn GuestGetInstanceProcAddr();

struct DispatchState {
  std::size_t live_instances = 0;
  std::size_t live_devices = 0;
  VkDevice specialized_device = VK_NULL_HANDLE;
  std::size_t specialized_commands = 0;
  std::size_t live_debug_callbacks = 0;
};
DispatchState GetDispatchState();
PFN_vkVoidFunction CurrentHostEntryPoint(const char* name);

}

#endif
