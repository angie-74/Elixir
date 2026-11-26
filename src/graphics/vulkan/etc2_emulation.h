// Copyright 2026 Mocktail Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from Mocktail's include/mocktail/graphics/vulkan_etc2_emulation.h.
// Modified by Elixir.
#ifndef ELIXIR_GRAPHICS_VULKAN_ETC2_EMULATION_H_
#define ELIXIR_GRAPHICS_VULKAN_ETC2_EMULATION_H_

#include <cstddef>
#include <cstdint>

#include "graphics/vulkan/etc2_decoder.h"
#include "graphics/vulkan/vulkan_host_headers.h"

namespace elixir::vulkan {

struct Etc2EmulatedFormat {
  VkFormat host_format = VK_FORMAT_UNDEFINED;
  EtcFormat etc_format = EtcFormat::kEtc2Rgb8;
};

bool LookupEmulatedEtc2Format(VkFormat format, Etc2EmulatedFormat* out);

VkFormatFeatureFlags EmulatedEtc2FormatFeatures(VkFormatFeatureFlags host_features);

bool Etc2UploadByteCounts(EtcFormat format, const VkExtent3D& extent, std::uint32_t layer_count,
                          VkDeviceSize* compressed, VkDeviceSize* decoded);

bool Etc2SupportAdvertised(const char* value);
bool Etc2SupportAdvertised();
bool Etc2EmulationDisabled(const char* value);

inline constexpr std::uint32_t kSmallTextureMaxExtent = 64;
inline constexpr std::uint32_t kDefaultSmallTextureUpscale = 1;
std::uint32_t SmallTextureUpscale(const char* value);

struct Etc2Statistics {
  std::uint64_t emulated_devices = 0;
  std::uint64_t images = 0;
  std::uint64_t recorded_uploads = 0;
  std::uint64_t decoded_uploads = 0;
  std::uint64_t compressed_bytes = 0;
  std::uint64_t failures = 0;
  std::uint64_t dropped_readbacks = 0;
};

class VulkanEtc2Emulation final {
 public:
  VulkanEtc2Emulation();
  ~VulkanEtc2Emulation();
  VulkanEtc2Emulation(const VulkanEtc2Emulation&) = delete;
  VulkanEtc2Emulation& operator=(const VulkanEtc2Emulation&) = delete;

  bool PhysicalDeviceNeedsEmulation(VkPhysicalDevice physical_device, PFN_vkGetPhysicalDeviceFeatures host);
  void ForgetPhysicalDevices(const VkPhysicalDevice* physical_devices, std::size_t count);

  void RegisterDevice(VkDevice device, VkPhysicalDevice physical_device, bool emulated,
                      const VkPhysicalDeviceMemoryProperties& memory,
                      PFN_vkGetDeviceProcAddr get_device_proc_addr);
  void DestroyDevice(VkDevice device);

  bool Active() const;
  bool Emulates(VkDevice device);
  bool IsEmulatedImage(VkImage image);

  bool HostImageCreateInfo(VkDevice device, const VkImageCreateInfo& info, VkImageCreateInfo* host,
                           std::uint32_t* scale = nullptr);

  VkResult CreateImage(VkDevice device, const VkImageCreateInfo* create_info,
                       const VkAllocationCallbacks* allocator, VkImage* image);
  void DestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* allocator);
  VkResult CreateImageView(VkDevice device, const VkImageViewCreateInfo* create_info,
                           const VkAllocationCallbacks* allocator, VkImageView* view);
  VkResult BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset);
  VkResult BindBufferMemory2(VkDevice device, std::uint32_t count, const VkBindBufferMemoryInfo* infos);
  VkResult MapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                     VkMemoryMapFlags flags, void** data);
  VkResult MapMemory2(VkDevice device, const VkMemoryMapInfo* info, void** data);
  void UnmapMemory(VkDevice device, VkDeviceMemory memory);
  VkResult UnmapMemory2(VkDevice device, const VkMemoryUnmapInfo* info);
  void FreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator);
  void DestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* allocator);

  bool CmdCopyBufferToImage(VkCommandBuffer command_buffer, VkBuffer source, VkImage destination,
                            VkImageLayout layout, std::uint32_t region_count, const VkBufferImageCopy* regions);
  bool CmdCopyBufferToImage2(VkCommandBuffer command_buffer, const VkCopyBufferToImageInfo2* info);
  bool CmdCopyImage(VkCommandBuffer command_buffer, VkImage source, VkImageLayout source_layout,
                    VkImage destination, VkImageLayout destination_layout, std::uint32_t region_count,
                    const VkImageCopy* regions);
  bool CmdCopyImage2(VkCommandBuffer command_buffer, const VkCopyImageInfo2* info);
  bool DropImageToBufferCopy(VkImage source);
  void CmdExecuteCommands(VkCommandBuffer command_buffer, std::uint32_t count, const VkCommandBuffer* secondaries);

  void AllocateCommandBuffers(VkDevice device, VkCommandPool pool, std::uint32_t count,
                              const VkCommandBuffer* command_buffers);
  void FreeCommandBuffers(std::uint32_t count, const VkCommandBuffer* command_buffers);
  void ReleaseCommandPool(VkDevice device, VkCommandPool pool, bool destroyed);

  bool HasPendingUploads() const;
  void PrepareSubmit(const VkCommandBuffer* command_buffers, std::uint32_t count);
  void ReleaseCommandBuffer(VkCommandBuffer command_buffer);

  Etc2Statistics Statistics() const;

  struct State;

 private:
  State* state_;
};

VulkanEtc2Emulation& Etc2Emulation();

extern std::uint32_t g_etc2_emulated_devices;
[[gnu::always_inline]] inline bool Etc2EmulationActive() noexcept {
  return __atomic_load_n(&g_etc2_emulated_devices, __ATOMIC_RELAXED) != 0;
}

}

#endif
