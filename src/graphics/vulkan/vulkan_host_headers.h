// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_GRAPHICS_VULKAN_VULKAN_HOST_HEADERS_H_
#define ELIXIR_GRAPHICS_VULKAN_VULKAN_HOST_HEADERS_H_

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES 1
#endif
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR 1
#endif
#ifndef VK_ENABLE_BETA_EXTENSIONS
#define VK_ENABLE_BETA_EXTENSIONS 1
#endif

#include <windows.h>

#include <vulkan/vulkan.h>

#if !defined(_WIN64)
#error "Elixir's Vulkan thunks assume the Microsoft x64 host convention"
#endif

static_assert(sizeof(VkDeviceSize) == 8 && sizeof(VkBool32) == 4 && sizeof(size_t) == 8,
              "Vulkan scalar types must match the LP64 guest");
static_assert(sizeof(VkBuffer) == 8, "non-dispatchable handles are 64-bit pointers on x64");

#endif
