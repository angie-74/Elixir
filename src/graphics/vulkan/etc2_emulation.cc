// Copyright 2026 Mocktail Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from Mocktail's src/graphics/vulkan_etc2_emulation.cc.
// Modified by Elixir.
#include "graphics/vulkan/etc2_emulation.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "graphics/vulkan/texture_override.h"

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define ELIXIR_ETC2_SSE41 1
#endif

namespace elixir::vulkan {

std::uint32_t g_etc2_emulated_devices = 0;

bool LookupEmulatedEtc2Format(VkFormat format, Etc2EmulatedFormat* out) {
  Etc2EmulatedFormat mapped;
  switch (format) {
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
      mapped = {VK_FORMAT_R8G8B8A8_UNORM, EtcFormat::kEtc2Rgb8};
      break;
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
      mapped = {VK_FORMAT_R8G8B8A8_SRGB, EtcFormat::kEtc2Rgb8};
      break;
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
      mapped = {VK_FORMAT_R8G8B8A8_UNORM, EtcFormat::kEtc2Rgb8A1};
      break;
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
      mapped = {VK_FORMAT_R8G8B8A8_SRGB, EtcFormat::kEtc2Rgb8A1};
      break;
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
      mapped = {VK_FORMAT_R8G8B8A8_UNORM, EtcFormat::kEtc2Rgba8};
      break;
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
      mapped = {VK_FORMAT_R8G8B8A8_SRGB, EtcFormat::kEtc2Rgba8};
      break;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
      mapped = {VK_FORMAT_R16_UNORM, EtcFormat::kEacR11};
      break;
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
      mapped = {VK_FORMAT_R16_SNORM, EtcFormat::kEacR11Signed};
      break;
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
      mapped = {VK_FORMAT_R16G16_UNORM, EtcFormat::kEacRg11};
      break;
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
      mapped = {VK_FORMAT_R16G16_SNORM, EtcFormat::kEacRg11Signed};
      break;
    default:
      return false;
  }
  if (out != nullptr) {
    *out = mapped;
  }
  return true;
}

VkFormatFeatureFlags EmulatedEtc2FormatFeatures(VkFormatFeatureFlags host_features) {
  constexpr VkFormatFeatureFlags kCompressedFeatures =
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
      VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
  return host_features & kCompressedFeatures;
}

bool Etc2UploadByteCounts(EtcFormat format, const VkExtent3D& extent, std::uint32_t layer_count,
                          VkDeviceSize* compressed, VkDeviceSize* decoded) {
  if (extent.width == 0 || extent.height == 0 || extent.depth != 1 || layer_count == 0 || compressed == nullptr ||
      decoded == nullptr) {
    return false;
  }
  const VkDeviceSize blocks =
      ((static_cast<VkDeviceSize>(extent.width) + 3) / 4) * ((static_cast<VkDeviceSize>(extent.height) + 3) / 4);
  *compressed = blocks * EtcBlockBytes(format) * layer_count;
  *decoded = static_cast<VkDeviceSize>(extent.width) * extent.height * EtcDecodedTexelBytes(format) * layer_count;
  return true;
}

bool Etc2EmulationDisabled(const char* value) {
  return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

namespace {

bool EmulationDisabledByEnvironment() {
  static const bool disabled = Etc2EmulationDisabled(std::getenv("ELIXIR_DISABLE_ETC2_EMULATION"));
  return disabled;
}

}

bool Etc2SupportAdvertised(const char* value) { return value == nullptr || std::strcmp(value, "0") != 0; }

bool Etc2SupportAdvertised() {
  static const bool advertised = Etc2SupportAdvertised(std::getenv("ELIXIR_ADVERTISE_ETC2"));
  return advertised;
}

std::uint32_t SmallTextureUpscale(const char* value) {
  if (value == nullptr || value[0] < '0' || value[0] > '9') {
    return kDefaultSmallTextureUpscale;
  }
  const long parsed = std::strtol(value, nullptr, 10);
  if (parsed < 1) {
    return 1;
  }
  return parsed > 8 ? 8 : static_cast<std::uint32_t>(parsed);
}

namespace {

#if defined(ELIXIR_ETC2_SSE41)
__attribute__((target("sse4.1"))) void CopyFromMapped(std::uint8_t* destination, const std::uint8_t* source,
                                                       std::size_t size) {
  while (size != 0 && (reinterpret_cast<std::uintptr_t>(source) & 15) != 0) {
    *destination++ = *source++;
    --size;
  }
  auto* in = reinterpret_cast<__m128i*>(const_cast<std::uint8_t*>(source));
  for (; size >= 64; size -= 64, in += 4, destination += 64) {
    const __m128i a = _mm_stream_load_si128(in);
    const __m128i b = _mm_stream_load_si128(in + 1);
    const __m128i c = _mm_stream_load_si128(in + 2);
    const __m128i d = _mm_stream_load_si128(in + 3);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(destination), a);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + 16), b);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + 32), c);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + 48), d);
  }
  std::memcpy(destination, in, size);
}
#else
void CopyFromMapped(std::uint8_t* destination, const std::uint8_t* source, std::size_t size) {
  std::memcpy(destination, source, size);
}
#endif

bool ShouldLog(std::atomic<unsigned>* counter, unsigned limit) {
  return counter->fetch_add(1, std::memory_order_relaxed) < limit;
}

struct HostDevice {
  VkDevice device = VK_NULL_HANDLE;
  bool emulated = false;
  VkPhysicalDeviceMemoryProperties memory{};
  PFN_vkCreateImage create_image = nullptr;
  PFN_vkDestroyImage destroy_image = nullptr;
  PFN_vkCreateImageView create_image_view = nullptr;
  PFN_vkBindBufferMemory bind_buffer_memory = nullptr;
  PFN_vkBindBufferMemory2 bind_buffer_memory2 = nullptr;
  PFN_vkMapMemory map_memory = nullptr;
  PFN_vkMapMemory2 map_memory2 = nullptr;
  PFN_vkUnmapMemory unmap_memory = nullptr;
  PFN_vkUnmapMemory2 unmap_memory2 = nullptr;
  PFN_vkFreeMemory free_memory = nullptr;
  PFN_vkDestroyBuffer destroy_buffer = nullptr;
  PFN_vkCmdCopyBufferToImage copy_buffer_to_image = nullptr;
  PFN_vkCmdCopyBufferToImage2 copy_buffer_to_image2 = nullptr;
  PFN_vkCmdCopyImage copy_image = nullptr;
  PFN_vkCmdCopyImage2 copy_image2 = nullptr;
  PFN_vkCmdBlitImage blit_image = nullptr;
  PFN_vkCmdExecuteCommands execute_commands = nullptr;
  PFN_vkCreateBuffer create_buffer = nullptr;
  PFN_vkAllocateMemory allocate_memory = nullptr;
  PFN_vkGetBufferMemoryRequirements get_buffer_memory_requirements = nullptr;
};

template <typename Function>
Function DeviceProc(PFN_vkGetDeviceProcAddr get, VkDevice device, const char* name, const char* alias = nullptr) {
  PFN_vkVoidFunction proc = get(device, name);
  if (proc == nullptr && alias != nullptr) {
    proc = get(device, alias);
  }
  return reinterpret_cast<Function>(proc);
}

struct BufferBinding {
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize offset = 0;
};

struct Mapping {
  void* data = nullptr;
  VkDeviceSize offset = 0;
  VkDeviceSize size = VK_WHOLE_SIZE;
};

struct ImageRecord {
  VkDevice device = VK_NULL_HANDLE;
  Etc2EmulatedFormat format;
  std::uint32_t scale = 1;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::shared_ptr<const RgbaImage> override;
  std::shared_ptr<const RgbaImage> base;
};

bool IsColorFormat(EtcFormat format) {
  return format == EtcFormat::kEtc2Rgb8 || format == EtcFormat::kEtc2Rgb8A1 || format == EtcFormat::kEtc2Rgba8;
}

struct Staging {
  VkDevice device = VK_NULL_HANDLE;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  bool host_cached = false;
  VkDeviceSize capacity = 0;
  std::uint8_t* mapped = nullptr;
};

constexpr VkDeviceSize kIdleStagingLimitBytes = VkDeviceSize{64} << 20;
constexpr std::size_t kIdleStagingLimitCount = 32;
constexpr std::uint64_t kIdleStagingMilliseconds = 30000;
constexpr VkDeviceSize kStagingAlignment = 16;

std::uint64_t SteadyMilliseconds() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

struct PendingUpload {
  VkDevice device = VK_NULL_HANDLE;
  VkBuffer source = VK_NULL_HANDLE;
  VkDeviceSize source_offset = 0;
  VkDeviceSize source_row_stride = 0;
  VkDeviceSize source_layer_stride = 0;
  VkDeviceSize source_span = 0;
  VkDeviceSize compressed = 0;
  VkDeviceSize decoded = 0;
  VkDeviceSize target_offset = 0;
  EtcFormat format = EtcFormat::kEtc2Rgb8;
  VkImage image = VK_NULL_HANDLE;
  std::uint32_t mip_level = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t layers = 0;
  std::uint32_t scale = 1;
  std::uint32_t source_width = 0;
  std::uint32_t source_height = 0;
  std::uint32_t target_width = 0;
  std::uint32_t target_height = 0;
  bool full_mip = false;
  VkDeviceSize target_bytes = 0;
  std::uint8_t* target = nullptr;
  bool target_cached = false;
};

struct CommandRecord {
  std::vector<PendingUpload> uploads;
  std::vector<Staging> staging;
  VkDeviceSize staging_used = 0;
  std::vector<VkCommandBuffer> secondaries;
};

struct PoolBinding {
  VkDevice device = VK_NULL_HANDLE;
  VkCommandPool pool = VK_NULL_HANDLE;
};

std::atomic<unsigned> g_image_logs{0};
std::atomic<unsigned> g_decode_logs{0};
std::atomic<unsigned> g_failure_logs{0};

struct Counters {
  std::atomic<std::uint64_t> emulated_devices{0};
  std::atomic<std::uint64_t> images{0};
  std::atomic<std::uint64_t> recorded_uploads{0};
  std::atomic<std::uint64_t> decoded_uploads{0};
  std::atomic<std::uint64_t> compressed_bytes{0};
  std::atomic<std::uint64_t> failures{0};
  std::atomic<std::uint64_t> dropped_readbacks{0};
  std::atomic<std::uint64_t> decoded_bytes{0};
  std::atomic<std::uint64_t> decode_microseconds{0};
  std::atomic<std::uint64_t> longest_decode_microseconds{0};
  std::atomic<std::uint64_t> staging_made{0};
  std::atomic<std::uint64_t> staging_reused{0};
  std::atomic<std::uint64_t> staging_shared{0};
};

Counters& Stats() {
  static Counters* counters = new Counters;
  return *counters;
}

void LogFailure(const char* message) {
  Stats().failures.fetch_add(1, std::memory_order_relaxed);
  if (ShouldLog(&g_failure_logs, 8)) {
    std::fprintf(stderr, "[vulkan] ETC2 emulation: %s\n", message);
  }
}

bool CreateStaging(const HostDevice& dev, VkDeviceSize size, Staging* staging, std::uint8_t** mapped) {
  if (dev.create_buffer == nullptr || dev.allocate_memory == nullptr ||
      dev.get_buffer_memory_requirements == nullptr || dev.bind_buffer_memory == nullptr ||
      dev.map_memory == nullptr || dev.destroy_buffer == nullptr || dev.free_memory == nullptr || size == 0) {
    return false;
  }
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VkBuffer buffer = VK_NULL_HANDLE;
  if (dev.create_buffer(dev.device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
    return false;
  }
  VkMemoryRequirements requirements{};
  dev.get_buffer_memory_requirements(dev.device, buffer, &requirements);
  constexpr VkMemoryPropertyFlags kRequired = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  constexpr VkMemoryPropertyFlags kExcluded = VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD |
                                              VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD |
                                              VK_MEMORY_PROPERTY_PROTECTED_BIT;
  std::uint32_t type = UINT32_MAX;
  for (std::uint32_t index = 0; index < dev.memory.memoryTypeCount; ++index) {
    if ((requirements.memoryTypeBits & (1U << index)) != 0 &&
        (dev.memory.memoryTypes[index].propertyFlags & kRequired) == kRequired &&
        (dev.memory.memoryTypes[index].propertyFlags & kExcluded) == 0) {
      if (type == UINT32_MAX) {
        type = index;
      }
      if ((dev.memory.memoryTypes[index].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0) {
        type = index;
        break;
      }
    }
  }
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkMemoryAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate_info.allocationSize = requirements.size;
  allocate_info.memoryTypeIndex = type;
  void* data = nullptr;
  if (type == UINT32_MAX || dev.allocate_memory(dev.device, &allocate_info, nullptr, &memory) != VK_SUCCESS) {
    dev.destroy_buffer(dev.device, buffer, nullptr);
    return false;
  }
  if (dev.bind_buffer_memory(dev.device, buffer, memory, 0) != VK_SUCCESS ||
      dev.map_memory(dev.device, memory, 0, VK_WHOLE_SIZE, 0, &data) != VK_SUCCESS) {
    dev.destroy_buffer(dev.device, buffer, nullptr);
    dev.free_memory(dev.device, memory, nullptr);
    return false;
  }
  *staging = {dev.device, buffer, memory,
              (dev.memory.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0, size,
              static_cast<std::uint8_t*>(data)};
  *mapped = static_cast<std::uint8_t*>(data);
  return true;
}

void DestroyStaging(const HostDevice& dev, const Staging& staging) {
  if (dev.destroy_buffer != nullptr) {
    dev.destroy_buffer(dev.device, staging.buffer, nullptr);
  }
  if (dev.free_memory != nullptr) {
    dev.free_memory(dev.device, staging.memory, nullptr);
  }
}

bool ScaledAxis(std::uint32_t base, std::uint32_t scale, std::uint32_t mip, std::int32_t offset,
                std::uint32_t extent, std::uint32_t* texels, std::uint32_t* host_start, std::uint32_t* host_end) {
  const std::uint32_t app_mip = std::max<std::uint32_t>(1, base >> mip);
  const std::uint32_t host_mip = std::max<std::uint32_t>(1, (base * scale) >> mip);
  if (offset < 0 || static_cast<std::uint32_t>(offset) >= app_mip) {
    return false;
  }
  const std::uint32_t app_end = std::min(app_mip, static_cast<std::uint32_t>(offset) + extent);
  const std::uint32_t start = static_cast<std::uint32_t>(offset) * scale;
  const std::uint32_t end = app_end == app_mip ? host_mip : std::min(host_mip, app_end * scale);
  if (end <= start) {
    return false;
  }
  *texels = app_end - static_cast<std::uint32_t>(offset);
  *host_start = start;
  *host_end = end;
  return true;
}

bool HostRegion(const ImageRecord* record, std::uint32_t mip, VkOffset3D offset, VkExtent3D extent,
                VkOffset3D* start, VkOffset3D* end) {
  if (record == nullptr || record->scale <= 1) {
    *start = offset;
    *end = {offset.x + static_cast<std::int32_t>(extent.width), offset.y + static_cast<std::int32_t>(extent.height),
            offset.z + static_cast<std::int32_t>(extent.depth)};
    return true;
  }
  std::uint32_t texels = 0;
  std::uint32_t x0 = 0;
  std::uint32_t x1 = 0;
  std::uint32_t y0 = 0;
  std::uint32_t y1 = 0;
  if (!ScaledAxis(record->width, record->scale, mip, offset.x, extent.width, &texels, &x0, &x1) ||
      !ScaledAxis(record->height, record->scale, mip, offset.y, extent.height, &texels, &y0, &y1)) {
    return false;
  }
  *start = {static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0), 0};
  *end = {static_cast<std::int32_t>(x1), static_cast<std::int32_t>(y1), 1};
  return true;
}

constexpr VkDeviceSize kStagingRegionAlignment = 16;

template <typename Region>
bool PlanRegions(const ImageRecord& record, VkDevice device, VkBuffer source, VkImage destination,
                 std::uint32_t count, const Region* regions, std::vector<Region>* rewritten,
                 std::vector<PendingUpload>* uploads, VkDeviceSize* total) {
  *total = 0;
  if (regions == nullptr || count == 0) {
    return false;
  }
  const std::uint32_t scale = record.scale;
  for (std::uint32_t index = 0; index < count; ++index) {
    const Region& region = regions[index];
    *total = (*total + kStagingRegionAlignment - 1) & ~(kStagingRegionAlignment - 1);
    PendingUpload upload;
    if (!Etc2UploadByteCounts(record.format.etc_format, region.imageExtent, region.imageSubresource.layerCount,
                              &upload.compressed, &upload.decoded)) {
      return false;
    }
    if (scale > 1 && region.imageSubresource.layerCount != 1) {
      return false;
    }
    upload.device = device;
    upload.source = source;
    upload.source_offset = region.bufferOffset;
    upload.target_offset = *total;
    upload.format = record.format.etc_format;
    upload.image = destination;
    upload.mip_level = region.imageSubresource.mipLevel;
    upload.width = region.imageExtent.width;
    upload.height = region.imageExtent.height;
    upload.layers = region.imageSubresource.layerCount;
    const VkDeviceSize row_texels = region.bufferRowLength != 0 ? region.bufferRowLength : upload.width;
    const VkDeviceSize layer_rows = region.bufferImageHeight != 0 ? region.bufferImageHeight : upload.height;
    if (row_texels < upload.width || layer_rows < upload.height) {
      return false;
    }
    const VkDeviceSize block_bytes = EtcBlockBytes(upload.format);
    const VkDeviceSize block_rows = (static_cast<VkDeviceSize>(upload.height) + 3) / 4;
    const VkDeviceSize packed_row = ((static_cast<VkDeviceSize>(upload.width) + 3) / 4) * block_bytes;
    upload.source_row_stride = ((row_texels + 3) / 4) * block_bytes;
    upload.source_layer_stride = upload.source_row_stride * ((layer_rows + 3) / 4);
    upload.source_span =
        (upload.layers - 1) * upload.source_layer_stride + (block_rows - 1) * upload.source_row_stride + packed_row;
    upload.scale = scale;
    upload.source_width = upload.width;
    upload.source_height = upload.height;
    upload.target_width = upload.width;
    upload.target_height = upload.height;
    upload.target_bytes = upload.decoded;
    Region copy = region;
    copy.bufferOffset = *total;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    if (scale > 1) {
      const std::uint32_t mip = region.imageSubresource.mipLevel;
      const auto axis = [&](std::uint32_t base, std::int32_t offset, std::uint32_t extent, std::uint32_t* source_texels,
                            std::uint32_t* target, std::int32_t* host_offset, std::uint32_t* host_extent) {
        std::uint32_t start = 0;
        std::uint32_t end = 0;
        if (!ScaledAxis(base, scale, mip, offset, extent, source_texels, &start, &end)) {
          return false;
        }
        *target = end - start;
        *host_offset = static_cast<std::int32_t>(start);
        *host_extent = end - start;
        return true;
      };
      if (!axis(record.width, region.imageOffset.x, region.imageExtent.width, &upload.source_width,
                &upload.target_width, &copy.imageOffset.x, &copy.imageExtent.width) ||
          !axis(record.height, region.imageOffset.y, region.imageExtent.height, &upload.source_height,
                &upload.target_height, &copy.imageOffset.y, &copy.imageExtent.height)) {
        return false;
      }
      upload.target_bytes = static_cast<VkDeviceSize>(upload.target_width) * upload.target_height *
                            EtcDecodedTexelBytes(record.format.etc_format);
      upload.full_mip = region.imageOffset.x == 0 && region.imageOffset.y == 0 &&
                        upload.source_width == std::max<std::uint32_t>(1, record.width >> mip) &&
                        upload.source_height == std::max<std::uint32_t>(1, record.height >> mip);
    }
    rewritten->push_back(copy);
    uploads->push_back(upload);
    *total += upload.target_bytes;
  }
  return true;
}

}

struct VulkanEtc2Emulation::State {
  std::mutex mutex;
  std::shared_mutex devices_mutex;
  std::mutex commands_mutex;
  std::mutex memory_mutex;
  std::unordered_map<VkPhysicalDevice, bool> physical_devices;
  std::vector<std::unique_ptr<HostDevice>> devices;
  std::atomic<std::uint32_t> emulated_device_count{0};
  std::unordered_map<VkImage, ImageRecord> images;
  std::unordered_map<VkBuffer, BufferBinding> buffers;
  std::unordered_map<VkDeviceMemory, Mapping> mappings;
  std::unordered_map<VkCommandBuffer, CommandRecord> commands;
  std::unordered_map<VkCommandBuffer, PoolBinding> pools;
  std::atomic<bool> has_commands{false};
  TextureOverrides overrides = TextureOverrides::FromEnvironment();
  const std::uint32_t upscale = SmallTextureUpscale(std::getenv("ELIXIR_SMALL_TEXTURE_UPSCALE"));

  std::shared_ptr<const RgbaImage> FindOverride(const PendingUpload& upload, const std::uint8_t* compressed,
                                                const std::uint8_t* decoded) {
    if (!overrides.enabled() || upload.layers != 1 || !IsColorFormat(upload.format)) {
      return nullptr;
    }
    std::shared_ptr<const RgbaImage> replacement;
    if (upload.mip_level == 0) {
      const std::uint64_t hash = HashBytes(compressed, upload.compressed);
      overrides.Dump(hash, upload.width, upload.height, decoded);
      replacement = overrides.Lookup(hash);
      std::lock_guard<std::mutex> lock(mutex);
      const auto record = images.find(upload.image);
      if (record != images.end()) {
        record->second.override = replacement;
      }
    } else {
      std::lock_guard<std::mutex> lock(mutex);
      const auto record = images.find(upload.image);
      if (record != images.end()) {
        replacement = record->second.override;
      }
    }
    return replacement;
  }

  void EmitUpload(const PendingUpload& upload, const std::uint8_t* compressed, const std::uint8_t* decoded) {
    const std::shared_ptr<const RgbaImage> replacement = FindOverride(upload, compressed, decoded);
    if (replacement != nullptr) {
      ResampleRgba(*replacement, upload.target_width, upload.target_height, upload.target);
    } else if (upload.scale > 1) {
      std::shared_ptr<const RgbaImage> base;
      if (upload.full_mip) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto record = images.find(upload.image);
        if (record != images.end()) {
          if (upload.mip_level == 0) {
            auto level0 = std::make_shared<RgbaImage>();
            level0->width = upload.source_width;
            level0->height = upload.source_height;
            level0->pixels.assign(decoded, decoded + upload.decoded);
            if (upload.source_width != upload.width || upload.source_height != upload.height) {
              level0->pixels.clear();
              for (std::uint32_t y = 0; y < upload.source_height; ++y) {
                const std::uint8_t* row = decoded + static_cast<std::size_t>(y) * upload.width * 4;
                level0->pixels.insert(level0->pixels.end(), row, row + upload.source_width * 4);
              }
            }
            record->second.base = level0;
          }
          base = record->second.base;
        }
      }
      if (base != nullptr) {
        ResampleRgba(*base, upload.target_width, upload.target_height, upload.target);
        return;
      }
      const std::uint8_t* source = decoded;
      std::vector<std::uint8_t> cropped;
      if (upload.source_width != upload.width || upload.source_height != upload.height) {
        const std::size_t row = static_cast<std::size_t>(upload.source_width) * 4;
        cropped.resize(row * upload.source_height);
        for (std::uint32_t y = 0; y < upload.source_height; ++y) {
          std::memcpy(cropped.data() + y * row, decoded + static_cast<std::size_t>(y) * upload.width * 4, row);
        }
        source = cropped.data();
      }
      ResampleRgba(source, upload.source_width, upload.source_height, upload.target_width, upload.target_height,
                   upload.target);
    } else {
      std::memcpy(upload.target, decoded, upload.decoded);
    }
  }

  const HostDevice* Find(VkDevice device) {
    std::shared_lock<std::shared_mutex> lock(devices_mutex);
    for (const auto& candidate : devices) {
      if (candidate->device == device) {
        return candidate.get();
      }
    }
    return nullptr;
  }

  bool BlitScaledCopy(const HostDevice& dev, VkCommandBuffer command_buffer, VkImage source,
                      VkImageLayout source_layout, VkImage destination, VkImageLayout destination_layout,
                      std::uint32_t count, const VkImageCopy* regions);

  bool LookupImage(VkImage image, ImageRecord* record) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = images.find(image);
    if (found == images.end()) {
      return false;
    }
    *record = found->second;
    return true;
  }

  const HostDevice* ImageDevice(VkImage image, ImageRecord* record) {
    if (!LookupImage(image, record)) return nullptr;
    const HostDevice* dev = Find(record->device);
    return dev != nullptr && dev->emulated ? dev : nullptr;
  }

  void Record(VkCommandBuffer command_buffer, std::vector<PendingUpload> uploads) {
    std::lock_guard<std::mutex> lock(commands_mutex);
    CommandRecord& record = commands[command_buffer];
    record.uploads.insert(record.uploads.end(), uploads.begin(), uploads.end());
    has_commands.store(true, std::memory_order_release);
  }

  std::atomic<VkDeviceSize> staging_chunk{kDefaultUploadChunkBytes};

  bool ReserveStaging(const HostDevice& dev, VkCommandBuffer command_buffer, VkDeviceSize size, Staging* staging,
                      VkDeviceSize* offset) {
    {
      std::lock_guard<std::mutex> lock(commands_mutex);
      const auto found = commands.find(command_buffer);
      if (found != commands.end() && !found->second.staging.empty()) {
        CommandRecord& record = found->second;
        const Staging& last = record.staging.back();
        const VkDeviceSize start = (record.staging_used + kStagingAlignment - 1) & ~(kStagingAlignment - 1);
        if (last.device == dev.device && start <= last.capacity && size <= last.capacity - start) {
          *staging = last;
          *offset = start;
          record.staging_used = start + size;
          Stats().staging_shared.fetch_add(1, std::memory_order_relaxed);
          return true;
        }
      }
    }
    Staging fresh;
    if (!AcquireStaging(dev, std::max(size, staging_chunk.load(std::memory_order_relaxed)), &fresh)) return false;
    std::lock_guard<std::mutex> lock(commands_mutex);
    CommandRecord& record = commands[command_buffer];
    record.staging.push_back(fresh);
    record.staging_used = size;
    has_commands.store(true, std::memory_order_release);
    *staging = fresh;
    *offset = 0;
    return true;
  }

  std::vector<Staging> TakeLocked(VkCommandBuffer command_buffer) {
    std::vector<Staging> doomed;
    const auto record = commands.find(command_buffer);
    if (record == commands.end()) {
      return doomed;
    }
    doomed = std::move(record->second.staging);
    commands.erase(record);
    has_commands.store(!commands.empty(), std::memory_order_release);
    return doomed;
  }

  void DestroyAll(const std::vector<Staging>& doomed) {
    for (const Staging& staging : doomed) {
      if (const HostDevice* dev = Find(staging.device); dev != nullptr) {
        DestroyStaging(*dev, staging);
      }
    }
  }

  struct IdleStaging {
    Staging staging;
    std::uint64_t since = 0;
  };
  std::mutex staging_mutex;
  std::vector<IdleStaging> idle_staging;
  VkDeviceSize idle_staging_bytes = 0;
  std::atomic<std::uint64_t> staging_age{0};

  std::uint64_t StagingNow() const { return SteadyMilliseconds() + staging_age.load(std::memory_order_relaxed); }

  void TakeExpiredLocked(std::uint64_t now, std::vector<Staging>* expired) {
    for (auto it = idle_staging.begin(); it != idle_staging.end();) {
      if (now - it->since >= kIdleStagingMilliseconds) {
        expired->push_back(it->staging);
        idle_staging_bytes -= it->staging.capacity;
        it = idle_staging.erase(it);
      } else {
        ++it;
      }
    }
  }

  bool AcquireStaging(const HostDevice& dev, VkDeviceSize size, Staging* staging) {
    std::vector<Staging> expired;
    bool reused = false;
    {
      std::lock_guard<std::mutex> lock(staging_mutex);
      TakeExpiredLocked(StagingNow(), &expired);
      std::size_t best = idle_staging.size();
      for (std::size_t index = 0; index < idle_staging.size(); ++index) {
        const Staging& candidate = idle_staging[index].staging;
        if (candidate.device != dev.device || candidate.capacity < size || candidate.capacity / 2 > size) continue;
        if (best == idle_staging.size() || candidate.capacity < idle_staging[best].staging.capacity) best = index;
      }
      if (best != idle_staging.size()) {
        *staging = idle_staging[best].staging;
        idle_staging_bytes -= staging->capacity;
        idle_staging.erase(idle_staging.begin() + static_cast<std::ptrdiff_t>(best));
        reused = true;
      }
    }
    DestroyAll(expired);
    if (reused) {
      Stats().staging_reused.fetch_add(1, std::memory_order_relaxed);
      return true;
    }
    std::uint8_t* mapped = nullptr;
    if (!CreateStaging(dev, size, staging, &mapped)) return false;
    Stats().staging_made.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  void ReleaseStaging(const std::vector<Staging>& released) {
    if (released.empty()) return;
    std::vector<Staging> doomed;
    {
      std::lock_guard<std::mutex> lock(staging_mutex);
      const std::uint64_t now = StagingNow();
      TakeExpiredLocked(now, &doomed);
      for (const Staging& staging : released) {
        if (staging.mapped == nullptr || staging.capacity > kIdleStagingLimitBytes) {
          doomed.push_back(staging);
          continue;
        }
        idle_staging.push_back({staging, now});
        idle_staging_bytes += staging.capacity;
      }
      while (!idle_staging.empty() &&
             (idle_staging.size() > kIdleStagingLimitCount || idle_staging_bytes > kIdleStagingLimitBytes)) {
        doomed.push_back(idle_staging.front().staging);
        idle_staging_bytes -= idle_staging.front().staging.capacity;
        idle_staging.erase(idle_staging.begin());
      }
    }
    DestroyAll(doomed);
  }

  std::vector<Staging> TakeIdleStaging(VkDevice device) {
    std::vector<Staging> taken;
    std::lock_guard<std::mutex> lock(staging_mutex);
    for (auto it = idle_staging.begin(); it != idle_staging.end();) {
      if (it->staging.device == device) {
        taken.push_back(it->staging);
        idle_staging_bytes -= it->staging.capacity;
        it = idle_staging.erase(it);
      } else {
        ++it;
      }
    }
    return taken;
  }
};

VulkanEtc2Emulation::VulkanEtc2Emulation() : state_(new State) {}

VulkanEtc2Emulation::~VulkanEtc2Emulation() { delete state_; }

bool VulkanEtc2Emulation::PhysicalDeviceNeedsEmulation(VkPhysicalDevice physical_device,
                                                       PFN_vkGetPhysicalDeviceFeatures host) {
  if (EmulationDisabledByEnvironment() || host == nullptr || physical_device == VK_NULL_HANDLE) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto found = state_->physical_devices.find(physical_device);
    if (found != state_->physical_devices.end()) {
      return found->second;
    }
  }
  VkPhysicalDeviceFeatures features{};
  host(physical_device, &features);
  const bool needed = features.textureCompressionETC2 != VK_TRUE;
  std::lock_guard<std::mutex> lock(state_->mutex);
  state_->physical_devices[physical_device] = needed;
  return needed;
}

void VulkanEtc2Emulation::ForgetPhysicalDevices(const VkPhysicalDevice* physical_devices, std::size_t count) {
  if (physical_devices == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(state_->mutex);
  for (std::size_t index = 0; index < count; ++index) {
    state_->physical_devices.erase(physical_devices[index]);
  }
}

void VulkanEtc2Emulation::RegisterDevice(VkDevice device, VkPhysicalDevice physical_device, bool emulated,
                                         const VkPhysicalDeviceMemoryProperties& memory,
                                         PFN_vkGetDeviceProcAddr get_device_proc_addr) {
  static_cast<void>(physical_device);
  if (device == VK_NULL_HANDLE || get_device_proc_addr == nullptr) {
    return;
  }
  auto dev = std::make_unique<HostDevice>();
  const auto get = get_device_proc_addr;
  dev->device = device;
  dev->emulated = emulated;
  dev->memory = memory;
  dev->create_image = DeviceProc<PFN_vkCreateImage>(get, device, "vkCreateImage");
  dev->destroy_image = DeviceProc<PFN_vkDestroyImage>(get, device, "vkDestroyImage");
  dev->create_image_view = DeviceProc<PFN_vkCreateImageView>(get, device, "vkCreateImageView");
  dev->bind_buffer_memory = DeviceProc<PFN_vkBindBufferMemory>(get, device, "vkBindBufferMemory");
  dev->bind_buffer_memory2 =
      DeviceProc<PFN_vkBindBufferMemory2>(get, device, "vkBindBufferMemory2", "vkBindBufferMemory2KHR");
  dev->map_memory = DeviceProc<PFN_vkMapMemory>(get, device, "vkMapMemory");
  dev->map_memory2 = DeviceProc<PFN_vkMapMemory2>(get, device, "vkMapMemory2", "vkMapMemory2KHR");
  dev->unmap_memory = DeviceProc<PFN_vkUnmapMemory>(get, device, "vkUnmapMemory");
  dev->unmap_memory2 = DeviceProc<PFN_vkUnmapMemory2>(get, device, "vkUnmapMemory2", "vkUnmapMemory2KHR");
  dev->free_memory = DeviceProc<PFN_vkFreeMemory>(get, device, "vkFreeMemory");
  dev->destroy_buffer = DeviceProc<PFN_vkDestroyBuffer>(get, device, "vkDestroyBuffer");
  dev->copy_buffer_to_image = DeviceProc<PFN_vkCmdCopyBufferToImage>(get, device, "vkCmdCopyBufferToImage");
  dev->copy_buffer_to_image2 = DeviceProc<PFN_vkCmdCopyBufferToImage2>(get, device, "vkCmdCopyBufferToImage2",
                                                                      "vkCmdCopyBufferToImage2KHR");
  dev->copy_image = DeviceProc<PFN_vkCmdCopyImage>(get, device, "vkCmdCopyImage");
  dev->copy_image2 = DeviceProc<PFN_vkCmdCopyImage2>(get, device, "vkCmdCopyImage2", "vkCmdCopyImage2KHR");
  dev->blit_image = DeviceProc<PFN_vkCmdBlitImage>(get, device, "vkCmdBlitImage");
  dev->execute_commands = DeviceProc<PFN_vkCmdExecuteCommands>(get, device, "vkCmdExecuteCommands");
  dev->create_buffer = DeviceProc<PFN_vkCreateBuffer>(get, device, "vkCreateBuffer");
  dev->allocate_memory = DeviceProc<PFN_vkAllocateMemory>(get, device, "vkAllocateMemory");
  dev->get_buffer_memory_requirements =
      DeviceProc<PFN_vkGetBufferMemoryRequirements>(get, device, "vkGetBufferMemoryRequirements");
  if (emulated) {
    Stats().emulated_devices.fetch_add(1, std::memory_order_relaxed);
    std::fprintf(stderr,
                 "[vulkan] ETC2/EAC emulation enabled: compressed uploads decode to RGBA8/R16 host images\n");
  }
  std::lock_guard<std::shared_mutex> lock(state_->devices_mutex);
  for (auto it = state_->devices.begin(); it != state_->devices.end();) {
    if ((*it)->device == device) {
      if ((*it)->emulated) { state_->emulated_device_count.fetch_sub(1, std::memory_order_relaxed); __atomic_fetch_sub(&g_etc2_emulated_devices, 1u, __ATOMIC_RELAXED); }
      it = state_->devices.erase(it);
    } else {
      ++it;
    }
  }
  if (emulated) { state_->emulated_device_count.fetch_add(1, std::memory_order_relaxed); __atomic_fetch_add(&g_etc2_emulated_devices, 1u, __ATOMIC_RELAXED); }
  state_->devices.push_back(std::move(dev));
}

void VulkanEtc2Emulation::DestroyDevice(VkDevice device) {
  std::vector<Staging> doomed;
  {
    std::lock_guard<std::mutex> lock(state_->commands_mutex);
    for (auto it = state_->commands.begin(); it != state_->commands.end();) {
      const bool owned = std::any_of(it->second.staging.begin(), it->second.staging.end(),
                                     [device](const Staging& staging) { return staging.device == device; });
      if (owned) {
        doomed.insert(doomed.end(), it->second.staging.begin(), it->second.staging.end());
        it = state_->commands.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = state_->pools.begin(); it != state_->pools.end();) {
      it = it->second.device == device ? state_->pools.erase(it) : std::next(it);
    }
    state_->has_commands.store(!state_->commands.empty(), std::memory_order_release);
  }
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    for (auto it = state_->images.begin(); it != state_->images.end();) {
      it = it->second.device == device ? state_->images.erase(it) : std::next(it);
    }
  }
  const std::vector<Staging> idle = state_->TakeIdleStaging(device);
  doomed.insert(doomed.end(), idle.begin(), idle.end());
  if (const HostDevice* dev = state_->Find(device); dev != nullptr) {
    for (const Staging& staging : doomed) {
      DestroyStaging(*dev, staging);
    }
  }
  std::lock_guard<std::shared_mutex> lock(state_->devices_mutex);
  for (auto it = state_->devices.begin(); it != state_->devices.end();) {
    if ((*it)->device == device) {
      if ((*it)->emulated) { state_->emulated_device_count.fetch_sub(1, std::memory_order_relaxed); __atomic_fetch_sub(&g_etc2_emulated_devices, 1u, __ATOMIC_RELAXED); }
      it = state_->devices.erase(it);
    } else {
      ++it;
    }
  }
}

bool VulkanEtc2Emulation::Active() const {
  return state_->emulated_device_count.load(std::memory_order_relaxed) != 0;
}

bool VulkanEtc2Emulation::Emulates(VkDevice device) {
  const HostDevice* dev = state_->Find(device);
  return dev != nullptr && dev->emulated;
}

bool VulkanEtc2Emulation::IsEmulatedImage(VkImage image) {
  std::lock_guard<std::mutex> lock(state_->mutex);
  return state_->images.count(image) != 0;
}

bool VulkanEtc2Emulation::HostImageCreateInfo(VkDevice device, const VkImageCreateInfo& info,
                                              VkImageCreateInfo* host, std::uint32_t* scale_out) {
  Etc2EmulatedFormat format;
  if (host == nullptr || !LookupEmulatedEtc2Format(info.format, &format) || !Emulates(device)) {
    return false;
  }
  *host = info;
  host->format = format.host_format;
  host->flags &= ~static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT);
  std::uint32_t scale = 1;
  if (state_->upscale > 1 && IsColorFormat(format.etc_format) && info.imageType == VK_IMAGE_TYPE_2D &&
      info.arrayLayers == 1 && info.extent.depth == 1 && info.extent.width <= kSmallTextureMaxExtent &&
      info.extent.height <= kSmallTextureMaxExtent) {
    scale = state_->upscale;
    host->extent.width *= scale;
    host->extent.height *= scale;
  }
  if (scale_out != nullptr) *scale_out = scale;
  return true;
}

VkResult VulkanEtc2Emulation::CreateImage(VkDevice device, const VkImageCreateInfo* create_info,
                                          const VkAllocationCallbacks* allocator, VkImage* image) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->create_image == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  Etc2EmulatedFormat format;
  if (!dev->emulated || create_info == nullptr || !LookupEmulatedEtc2Format(create_info->format, &format)) {
    return dev->create_image(device, create_info, allocator, image);
  }
  VkImageCreateInfo host_info{};
  std::uint32_t scale = 1;
  HostImageCreateInfo(device, *create_info, &host_info, &scale);
  const VkResult result = dev->create_image(device, &host_info, allocator, image);
  if (result == VK_SUCCESS && image != nullptr) {
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      state_->images[*image] = {device, format, scale, create_info->extent.width, create_info->extent.height,
                                nullptr, nullptr};
    }
    Stats().images.fetch_add(1, std::memory_order_relaxed);
    if (ShouldLog(&g_image_logs, 4)) {
      std::fprintf(stderr, "[vulkan] ETC2 image %ux%u mips=%u format=%d -> host format=%d\n",
                   create_info->extent.width, create_info->extent.height, create_info->mipLevels,
                   static_cast<int>(create_info->format), static_cast<int>(format.host_format));
    }
  }
  return result;
}

void VulkanEtc2Emulation::DestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* allocator) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->destroy_image == nullptr) {
    return;
  }
  if (dev->emulated) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->images.erase(image);
  }
  dev->destroy_image(device, image, allocator);
}

VkResult VulkanEtc2Emulation::CreateImageView(VkDevice device, const VkImageViewCreateInfo* create_info,
                                              const VkAllocationCallbacks* allocator, VkImageView* view) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->create_image_view == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  Etc2EmulatedFormat format;
  if (!dev->emulated || create_info == nullptr || !LookupEmulatedEtc2Format(create_info->format, &format)) {
    return dev->create_image_view(device, create_info, allocator, view);
  }
  VkImageViewCreateInfo host_info = *create_info;
  host_info.format = format.host_format;
  return dev->create_image_view(device, &host_info, allocator, view);
}

VkResult VulkanEtc2Emulation::BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
                                               VkDeviceSize offset) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->bind_buffer_memory == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  const VkResult result = dev->bind_buffer_memory(device, buffer, memory, offset);
  if (result == VK_SUCCESS && dev->emulated) {
    std::lock_guard<std::mutex> lock(state_->memory_mutex);
    state_->buffers[buffer] = {memory, offset};
  }
  return result;
}

VkResult VulkanEtc2Emulation::BindBufferMemory2(VkDevice device, std::uint32_t count,
                                                const VkBindBufferMemoryInfo* infos) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->bind_buffer_memory2 == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  const VkResult result = dev->bind_buffer_memory2(device, count, infos);
  if (result == VK_SUCCESS && dev->emulated && infos != nullptr) {
    std::lock_guard<std::mutex> lock(state_->memory_mutex);
    for (std::uint32_t index = 0; index < count; ++index) {
      state_->buffers[infos[index].buffer] = {infos[index].memory, infos[index].memoryOffset};
    }
  }
  return result;
}

VkResult VulkanEtc2Emulation::MapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                                        VkDeviceSize size, VkMemoryMapFlags flags, void** data) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->map_memory == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  std::unique_lock<std::mutex> lock(state_->memory_mutex, std::defer_lock);
  if (dev->emulated) {
    lock.lock();
  }
  const VkResult result = dev->map_memory(device, memory, offset, size, flags, data);
  if (result == VK_SUCCESS && dev->emulated && data != nullptr) {
    state_->mappings[memory] = {*data, offset, size};
  }
  return result;
}

VkResult VulkanEtc2Emulation::MapMemory2(VkDevice device, const VkMemoryMapInfo* info, void** data) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->map_memory2 == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  std::unique_lock<std::mutex> lock(state_->memory_mutex, std::defer_lock);
  if (dev->emulated) {
    lock.lock();
  }
  const VkResult result = dev->map_memory2(device, info, data);
  if (result == VK_SUCCESS && dev->emulated && info != nullptr && data != nullptr) {
    state_->mappings[info->memory] = {*data, info->offset, info->size};
  }
  return result;
}

void VulkanEtc2Emulation::UnmapMemory(VkDevice device, VkDeviceMemory memory) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->unmap_memory == nullptr) {
    return;
  }
  std::unique_lock<std::mutex> lock(state_->memory_mutex, std::defer_lock);
  if (dev->emulated) {
    lock.lock();
    state_->mappings.erase(memory);
  }
  dev->unmap_memory(device, memory);
}

VkResult VulkanEtc2Emulation::UnmapMemory2(VkDevice device, const VkMemoryUnmapInfo* info) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->unmap_memory2 == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  std::unique_lock<std::mutex> lock(state_->memory_mutex, std::defer_lock);
  if (dev->emulated && info != nullptr) {
    lock.lock();
    state_->mappings.erase(info->memory);
  }
  return dev->unmap_memory2(device, info);
}

void VulkanEtc2Emulation::FreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->free_memory == nullptr) {
    return;
  }
  std::unique_lock<std::mutex> lock(state_->memory_mutex, std::defer_lock);
  if (dev->emulated) {
    lock.lock();
    state_->mappings.erase(memory);
  }
  dev->free_memory(device, memory, allocator);
}

void VulkanEtc2Emulation::DestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* allocator) {
  const HostDevice* dev = state_->Find(device);
  if (dev == nullptr || dev->destroy_buffer == nullptr) {
    return;
  }
  if (dev->emulated) {
    std::lock_guard<std::mutex> lock(state_->memory_mutex);
    state_->buffers.erase(buffer);
  }
  dev->destroy_buffer(device, buffer, allocator);
}

bool VulkanEtc2Emulation::CmdCopyBufferToImage(VkCommandBuffer command_buffer, VkBuffer source, VkImage destination,
                                               VkImageLayout layout, std::uint32_t region_count,
                                               const VkBufferImageCopy* regions) {
  ImageRecord record;
  const HostDevice* dev = state_->ImageDevice(destination, &record);
  if (dev == nullptr || dev->copy_buffer_to_image == nullptr) {
    return false;
  }
  std::vector<VkBufferImageCopy> rewritten;
  std::vector<PendingUpload> uploads;
  VkDeviceSize total = 0;
  Staging staging;
  std::uint8_t* mapped = nullptr;
  if (!PlanRegions(record, dev->device, source, destination, region_count, regions, &rewritten, &uploads, &total)) {
    LogFailure("skipped an upload with an unsupported region");
    return true;
  }
  VkDeviceSize base = 0;
  if (!state_->ReserveStaging(*dev, command_buffer, total, &staging, &base)) {
    LogFailure("could not allocate a host-visible staging buffer");
    return true;
  }
  mapped = staging.mapped + base;
  for (PendingUpload& upload : uploads) {
    upload.target = mapped + upload.target_offset;
    upload.target_cached = staging.host_cached;
  }
  for (VkBufferImageCopy& copy : rewritten) copy.bufferOffset += base;
  Stats().recorded_uploads.fetch_add(uploads.size(), std::memory_order_relaxed);
  state_->Record(command_buffer, std::move(uploads));
  dev->copy_buffer_to_image(command_buffer, staging.buffer, destination, layout, region_count, rewritten.data());
  return true;
}

bool VulkanEtc2Emulation::CmdCopyBufferToImage2(VkCommandBuffer command_buffer,
                                                const VkCopyBufferToImageInfo2* info) {
  if (info == nullptr) {
    return false;
  }
  ImageRecord record;
  const HostDevice* dev = state_->ImageDevice(info->dstImage, &record);
  if (dev == nullptr || dev->copy_buffer_to_image2 == nullptr) {
    return false;
  }
  std::vector<VkBufferImageCopy2> rewritten;
  std::vector<PendingUpload> uploads;
  VkDeviceSize total = 0;
  Staging staging;
  std::uint8_t* mapped = nullptr;
  if (!PlanRegions(record, dev->device, info->srcBuffer, info->dstImage, info->regionCount, info->pRegions,
                   &rewritten, &uploads, &total)) {
    LogFailure("skipped an upload with an unsupported region");
    return true;
  }
  VkDeviceSize base = 0;
  if (!state_->ReserveStaging(*dev, command_buffer, total, &staging, &base)) {
    LogFailure("could not allocate a host-visible staging buffer");
    return true;
  }
  mapped = staging.mapped + base;
  for (PendingUpload& upload : uploads) {
    upload.target = mapped + upload.target_offset;
    upload.target_cached = staging.host_cached;
  }
  for (VkBufferImageCopy2& copy : rewritten) copy.bufferOffset += base;
  Stats().recorded_uploads.fetch_add(uploads.size(), std::memory_order_relaxed);
  state_->Record(command_buffer, std::move(uploads));
  VkCopyBufferToImageInfo2 host_info = *info;
  host_info.srcBuffer = staging.buffer;
  host_info.pRegions = rewritten.data();
  dev->copy_buffer_to_image2(command_buffer, &host_info);
  return true;
}

bool VulkanEtc2Emulation::State::BlitScaledCopy(const HostDevice& dev, VkCommandBuffer command_buffer,
                                                VkImage source, VkImageLayout source_layout, VkImage destination,
                                                VkImageLayout destination_layout, std::uint32_t count,
                                                const VkImageCopy* regions) {
  ImageRecord source_record;
  ImageRecord destination_record;
  const bool source_scaled = LookupImage(source, &source_record) && source_record.scale > 1;
  const bool destination_scaled = LookupImage(destination, &destination_record) && destination_record.scale > 1;
  if ((!source_scaled && !destination_scaled) || dev.blit_image == nullptr || regions == nullptr) {
    return false;
  }
  std::vector<VkImageBlit> blits;
  blits.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    const VkImageCopy& region = regions[index];
    VkImageBlit blit{};
    blit.srcSubresource = region.srcSubresource;
    blit.dstSubresource = region.dstSubresource;
    if (!HostRegion(source_scaled ? &source_record : nullptr, region.srcSubresource.mipLevel, region.srcOffset,
                    region.extent, &blit.srcOffsets[0], &blit.srcOffsets[1]) ||
        !HostRegion(destination_scaled ? &destination_record : nullptr, region.dstSubresource.mipLevel,
                    region.dstOffset, region.extent, &blit.dstOffsets[0], &blit.dstOffsets[1])) {
      LogFailure("skipped an image copy with an unsupported region");
      return true;
    }
    blits.push_back(blit);
  }
  const bool same_scale = source_scaled && destination_scaled && source_record.scale == destination_record.scale;
  dev.blit_image(command_buffer, source, source_layout, destination, destination_layout,
                 static_cast<std::uint32_t>(blits.size()), blits.data(),
                 same_scale ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
  return true;
}

bool VulkanEtc2Emulation::CmdCopyImage(VkCommandBuffer command_buffer, VkImage source, VkImageLayout source_layout,
                                       VkImage destination, VkImageLayout destination_layout,
                                       std::uint32_t region_count, const VkImageCopy* regions) {
  if (state_->upscale <= 1) {
    return false;
  }
  ImageRecord record;
  const HostDevice* dev = state_->ImageDevice(source, &record);
  if (dev == nullptr) dev = state_->ImageDevice(destination, &record);
  return dev != nullptr && state_->BlitScaledCopy(*dev, command_buffer, source, source_layout, destination,
                                                  destination_layout, region_count, regions);
}

bool VulkanEtc2Emulation::CmdCopyImage2(VkCommandBuffer command_buffer, const VkCopyImageInfo2* info) {
  if (state_->upscale <= 1 || info == nullptr || info->pRegions == nullptr) {
    return false;
  }
  ImageRecord record;
  const HostDevice* dev = state_->ImageDevice(info->srcImage, &record);
  if (dev == nullptr) dev = state_->ImageDevice(info->dstImage, &record);
  if (dev == nullptr) {
    return false;
  }
  std::vector<VkImageCopy> regions;
  regions.reserve(info->regionCount);
  for (std::uint32_t index = 0; index < info->regionCount; ++index) {
    const VkImageCopy2& region = info->pRegions[index];
    regions.push_back({region.srcSubresource, region.srcOffset, region.dstSubresource, region.dstOffset,
                       region.extent});
  }
  return state_->BlitScaledCopy(*dev, command_buffer, info->srcImage, info->srcImageLayout, info->dstImage,
                                info->dstImageLayout, info->regionCount, regions.data());
}

bool VulkanEtc2Emulation::DropImageToBufferCopy(VkImage source) {
  ImageRecord record;
  if (state_->ImageDevice(source, &record) == nullptr) {
    return false;
  }
  Stats().dropped_readbacks.fetch_add(1, std::memory_order_relaxed);
  static std::atomic<unsigned> logs{0};
  if (ShouldLog(&logs, 4)) {
    std::fprintf(stderr,
                 "[vulkan] ETC2 emulation: dropped a vkCmdCopyImageToBuffer from an emulated image (the host "
                 "image holds decoded texels, not compressed blocks)\n");
  }
  return true;
}

void VulkanEtc2Emulation::CmdExecuteCommands(VkCommandBuffer command_buffer, std::uint32_t count,
                                             const VkCommandBuffer* secondaries) {
  if (secondaries != nullptr && state_->has_commands.load(std::memory_order_acquire)) {
    std::lock_guard<std::mutex> lock(state_->commands_mutex);
    for (std::uint32_t index = 0; index < count; ++index) {
      if (state_->commands.count(secondaries[index]) != 0) {
        state_->commands[command_buffer].secondaries.push_back(secondaries[index]);
      }
    }
  }
}

void VulkanEtc2Emulation::AllocateCommandBuffers(VkDevice device, VkCommandPool pool, std::uint32_t count,
                                                 const VkCommandBuffer* command_buffers) {
  if (command_buffers == nullptr || count == 0 || !Emulates(device)) {
    return;
  }
  std::lock_guard<std::mutex> lock(state_->commands_mutex);
  for (std::uint32_t index = 0; index < count; ++index) {
    state_->pools[command_buffers[index]] = {device, pool};
  }
}

void VulkanEtc2Emulation::FreeCommandBuffers(std::uint32_t count, const VkCommandBuffer* command_buffers) {
  if (command_buffers == nullptr) {
    return;
  }
  std::vector<Staging> doomed;
  {
    std::lock_guard<std::mutex> lock(state_->commands_mutex);
    for (std::uint32_t index = 0; index < count; ++index) {
      if (command_buffers[index] == VK_NULL_HANDLE) continue;
      std::vector<Staging> staging = state_->TakeLocked(command_buffers[index]);
      doomed.insert(doomed.end(), staging.begin(), staging.end());
      state_->pools.erase(command_buffers[index]);
    }
  }
  state_->ReleaseStaging(doomed);
}

void VulkanEtc2Emulation::ReleaseCommandPool(VkDevice device, VkCommandPool pool, bool destroyed) {
  std::vector<Staging> doomed;
  {
    std::lock_guard<std::mutex> lock(state_->commands_mutex);
    for (auto it = state_->pools.begin(); it != state_->pools.end();) {
      if (it->second.device != device || it->second.pool != pool) {
        ++it;
        continue;
      }
      std::vector<Staging> staging = state_->TakeLocked(it->first);
      doomed.insert(doomed.end(), staging.begin(), staging.end());
      it = destroyed ? state_->pools.erase(it) : std::next(it);
    }
  }
  state_->ReleaseStaging(doomed);
}

bool VulkanEtc2Emulation::HasPendingUploads() const { return state_->has_commands.load(std::memory_order_acquire); }

void VulkanEtc2Emulation::PrepareSubmit(const VkCommandBuffer* command_buffers, std::uint32_t count) {
  if (command_buffers == nullptr || count == 0 || !state_->has_commands.load(std::memory_order_acquire)) {
    return;
  }
  struct Work {
    PendingUpload upload;
    std::uint8_t* source = nullptr;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize memory_offset = 0;
  };
  std::vector<PendingUpload> uploads;
  {
    std::lock_guard<std::mutex> lock(state_->commands_mutex);
    std::unordered_set<VkCommandBuffer> visited;
    auto collect = [this, &uploads, &visited](VkCommandBuffer command_buffer) {
      const auto record = state_->commands.find(command_buffer);
      if (record == state_->commands.end() || !visited.insert(command_buffer).second) {
        return;
      }
      uploads.insert(uploads.end(), record->second.uploads.begin(), record->second.uploads.end());
    };
    for (std::uint32_t index = 0; index < count; ++index) {
      collect(command_buffers[index]);
      const auto record = state_->commands.find(command_buffers[index]);
      if (record != state_->commands.end()) {
        const std::vector<VkCommandBuffer> secondaries = record->second.secondaries;
        for (VkCommandBuffer secondary : secondaries) {
          collect(secondary);
        }
      }
    }
  }
  if (uploads.empty()) {
    return;
  }
  std::vector<Work> work;
  work.reserve(uploads.size());
  std::unique_lock<std::mutex> memory_lock(state_->memory_mutex);
  {
    for (const PendingUpload& upload : uploads) {
      const auto binding = state_->buffers.find(upload.source);
      if (binding == state_->buffers.end()) {
        LogFailure("upload source buffer has no tracked memory binding");
        continue;
      }
      Work item;
      item.upload = upload;
      const VkDeviceSize absolute = binding->second.offset + upload.source_offset;
      const auto mapping = state_->mappings.find(binding->second.memory);
      if (mapping == state_->mappings.end()) {
        item.memory = binding->second.memory;
        item.memory_offset = absolute;
      } else if (absolute >= mapping->second.offset &&
                 (mapping->second.size == VK_WHOLE_SIZE ||
                  (absolute - mapping->second.offset <= mapping->second.size &&
                   upload.source_span <= mapping->second.size - (absolute - mapping->second.offset)))) {
        item.source = static_cast<std::uint8_t*>(mapping->second.data) + (absolute - mapping->second.offset);
      } else {
        LogFailure("upload source lies outside the mapped range");
        continue;
      }
      work.push_back(item);
    }
  }
  if (work.empty()) {
    return;
  }
  struct TemporaryMapping {
    const HostDevice* dev = nullptr;
    std::uint8_t* data = nullptr;
  };
  std::unordered_map<VkDeviceMemory, TemporaryMapping> temporary;
  struct Copy {
    const std::uint8_t* source = nullptr;
    VkDeviceSize compressed = 0;
    VkDeviceSize decoded = 0;
  };
  std::vector<Copy> copies;
  std::vector<const PendingUpload*> copied_uploads;
  VkDeviceSize compressed_total = 0;
  VkDeviceSize decoded_total = 0;
  for (const Work& item : work) {
    const PendingUpload& upload = item.upload;
    const std::uint8_t* source = item.source;
    if (source == nullptr) {
      auto found = temporary.find(item.memory);
      if (found == temporary.end()) {
        const HostDevice* dev = state_->Find(upload.device);
        void* data = nullptr;
        if (dev == nullptr || dev->map_memory == nullptr ||
            dev->map_memory(dev->device, item.memory, 0, VK_WHOLE_SIZE, 0, &data) != VK_SUCCESS) {
          LogFailure("could not map an unmapped upload source");
          continue;
        }
        found = temporary.emplace(item.memory, TemporaryMapping{dev, static_cast<std::uint8_t*>(data)}).first;
      }
      source = found->second.data + item.memory_offset;
    }
    const bool direct = upload.target_cached && upload.scale == 1 && !state_->overrides.enabled();
    const VkDeviceSize decoded = direct ? 0 : upload.decoded;
    copies.push_back({source, upload.compressed, decoded});
    copied_uploads.push_back(&upload);
    compressed_total += upload.compressed;
    decoded_total += decoded;
  }

  static thread_local std::vector<std::uint8_t> scratch_source;
  static thread_local std::vector<std::uint8_t> scratch_decoded;
  if (scratch_source.size() < compressed_total) {
    scratch_source.resize(compressed_total);
  }
  if (scratch_decoded.size() < decoded_total) {
    scratch_decoded.resize(decoded_total);
  }
  std::vector<EtcDecodeJob> jobs;
  VkDeviceSize compressed_offset = 0;
  VkDeviceSize decoded_offset = 0;
  for (std::size_t index = 0; index < copies.size(); ++index) {
    const Copy& copy = copies[index];
    const PendingUpload& upload = *copied_uploads[index];
    std::uint8_t* source = scratch_source.data() + compressed_offset;
    std::uint8_t* decoded = copy.decoded == 0 ? upload.target : scratch_decoded.data() + decoded_offset;
    const VkDeviceSize compressed_layer = upload.compressed / upload.layers;
    const VkDeviceSize decoded_layer = upload.decoded / upload.layers;
    if (upload.source_span == copy.compressed) {
      CopyFromMapped(source, copy.source, copy.compressed);
    } else {
      const VkDeviceSize block_rows = (static_cast<VkDeviceSize>(upload.height) + 3) / 4;
      const VkDeviceSize packed_row = compressed_layer / block_rows;
      for (std::uint32_t layer = 0; layer < upload.layers; ++layer) {
        for (VkDeviceSize row = 0; row < block_rows; ++row) {
          CopyFromMapped(source + layer * compressed_layer + row * packed_row,
                         copy.source + layer * upload.source_layer_stride + row * upload.source_row_stride,
                         packed_row);
        }
      }
    }
    for (std::uint32_t layer = 0; layer < upload.layers; ++layer) {
      EtcDecodeJob job;
      job.format = upload.format;
      job.source = source + layer * compressed_layer;
      job.source_bytes = compressed_layer;
      job.width = upload.width;
      job.height = upload.height;
      job.destination = decoded + layer * decoded_layer;
      job.destination_bytes = decoded_layer;
      jobs.push_back(job);
    }
    compressed_offset += copy.compressed;
    decoded_offset += copy.decoded;
    if (ShouldLog(&g_decode_logs, 4)) {
      std::fprintf(stderr, "[vulkan] ETC2 upload decoded %ux%u layers=%u\n", upload.width, upload.height,
                   upload.layers);
    }
  }
  for (const auto& [memory, mapping] : temporary) {
    if (mapping.dev->unmap_memory != nullptr) {
      mapping.dev->unmap_memory(mapping.dev->device, memory);
    }
  }
  memory_lock.unlock();
  const auto decode_start = std::chrono::steady_clock::now();
  DecodeEtcJobs(jobs.data(), jobs.size(), DecodeWorkerCount());
  const auto decode_microseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - decode_start).count());
  std::uint64_t decoded_bytes = 0;
  for (const EtcDecodeJob& job : jobs) {
    if (!job.ok) {
      LogFailure("could not decode an upload layer");
    } else {
      decoded_bytes += job.destination_bytes;
    }
  }
  Counters& counters = Stats();
  counters.decoded_uploads.fetch_add(copied_uploads.size(), std::memory_order_relaxed);
  counters.compressed_bytes.fetch_add(compressed_total, std::memory_order_relaxed);
  counters.decoded_bytes.fetch_add(decoded_bytes, std::memory_order_relaxed);
  counters.decode_microseconds.fetch_add(decode_microseconds, std::memory_order_relaxed);
  std::uint64_t longest = counters.longest_decode_microseconds.load(std::memory_order_relaxed);
  while (decode_microseconds > longest &&
         !counters.longest_decode_microseconds.compare_exchange_weak(longest, decode_microseconds,
                                                                     std::memory_order_relaxed)) {
  }
  compressed_offset = 0;
  decoded_offset = 0;
  for (std::size_t index = 0; index < copied_uploads.size(); ++index) {
    if (copies[index].decoded != 0) {
      state_->EmitUpload(*copied_uploads[index], scratch_source.data() + compressed_offset,
                         scratch_decoded.data() + decoded_offset);
    }
    compressed_offset += copies[index].compressed;
    decoded_offset += copies[index].decoded;
  }
}

void VulkanEtc2Emulation::ReleaseCommandBuffer(VkCommandBuffer command_buffer) {
  if (!state_->has_commands.load(std::memory_order_acquire)) {
    return;
  }
  std::vector<Staging> doomed;
  {
    std::lock_guard<std::mutex> lock(state_->commands_mutex);
    doomed = state_->TakeLocked(command_buffer);
  }
  state_->ReleaseStaging(doomed);
}

void VulkanEtc2Emulation::SetUploadChunkBytes(VkDeviceSize bytes) {
  state_->staging_chunk.store(bytes, std::memory_order_relaxed);
}

void VulkanEtc2Emulation::AgeIdleUploadBuffers(std::uint64_t milliseconds) {
  state_->staging_age.fetch_add(milliseconds, std::memory_order_relaxed);
}

Etc2Statistics VulkanEtc2Emulation::Statistics() const {
  const Counters& counters = Stats();
  Etc2Statistics result;
  result.emulated_devices = counters.emulated_devices.load(std::memory_order_relaxed);
  result.images = counters.images.load(std::memory_order_relaxed);
  result.recorded_uploads = counters.recorded_uploads.load(std::memory_order_relaxed);
  result.decoded_uploads = counters.decoded_uploads.load(std::memory_order_relaxed);
  result.compressed_bytes = counters.compressed_bytes.load(std::memory_order_relaxed);
  result.failures = counters.failures.load(std::memory_order_relaxed);
  result.dropped_readbacks = counters.dropped_readbacks.load(std::memory_order_relaxed);
  result.decoded_bytes = counters.decoded_bytes.load(std::memory_order_relaxed);
  result.decode_microseconds = counters.decode_microseconds.load(std::memory_order_relaxed);
  result.upload_buffers_made = counters.staging_made.load(std::memory_order_relaxed);
  result.upload_buffers_reused = counters.staging_reused.load(std::memory_order_relaxed);
  result.uploads_sharing_a_buffer = counters.staging_shared.load(std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(state_->staging_mutex);
    result.idle_upload_buffers = state_->idle_staging.size();
    result.idle_upload_bytes = state_->idle_staging_bytes;
  }
  return result;
}

std::string Etc2DecodeReport() {
  static std::atomic<std::uint64_t> seen_uploads{0};
  static std::atomic<std::uint64_t> seen_compressed{0};
  static std::atomic<std::uint64_t> seen_decoded{0};
  static std::atomic<std::uint64_t> seen_microseconds{0};
  static std::atomic<std::uint64_t> seen_made{0};
  static std::atomic<std::uint64_t> seen_reused{0};
  static std::atomic<std::uint64_t> seen_shared{0};
  Counters& counters = Stats();
  const std::uint64_t uploads = counters.decoded_uploads.load(std::memory_order_relaxed);
  const std::uint64_t compressed = counters.compressed_bytes.load(std::memory_order_relaxed);
  const std::uint64_t decoded = counters.decoded_bytes.load(std::memory_order_relaxed);
  const std::uint64_t microseconds = counters.decode_microseconds.load(std::memory_order_relaxed);
  const std::uint64_t longest = counters.longest_decode_microseconds.exchange(0, std::memory_order_relaxed);
  const std::uint64_t new_uploads = uploads - seen_uploads.exchange(uploads, std::memory_order_relaxed);
  const std::uint64_t new_compressed = compressed - seen_compressed.exchange(compressed, std::memory_order_relaxed);
  const std::uint64_t new_decoded = decoded - seen_decoded.exchange(decoded, std::memory_order_relaxed);
  const std::uint64_t new_microseconds =
      microseconds - seen_microseconds.exchange(microseconds, std::memory_order_relaxed);
  const std::uint64_t made = counters.staging_made.load(std::memory_order_relaxed);
  const std::uint64_t reused = counters.staging_reused.load(std::memory_order_relaxed);
  const std::uint64_t new_made = made - seen_made.exchange(made, std::memory_order_relaxed);
  const std::uint64_t new_reused = reused - seen_reused.exchange(reused, std::memory_order_relaxed);
  const std::uint64_t shared = counters.staging_shared.load(std::memory_order_relaxed);
  const std::uint64_t new_shared = shared - seen_shared.exchange(shared, std::memory_order_relaxed);
  if (new_uploads == 0) {
    return {};
  }
  constexpr double kMegabyte = 1024.0 * 1024.0;
  char text[320];
  std::snprintf(text, sizeof(text),
                "texture decoding: %llu %s, %.2f MB of ETC textures became %.2f MB in %.2f ms, the longest submit "
                "took %.2f ms; upload buffers: %llu made, %llu used again, %llu uploads shared one%s",
                static_cast<unsigned long long>(new_uploads), new_uploads == 1 ? "upload" : "uploads",
                static_cast<double>(new_compressed) / kMegabyte, static_cast<double>(new_decoded) / kMegabyte,
                static_cast<double>(new_microseconds) / 1000.0, static_cast<double>(longest) / 1000.0,
                static_cast<unsigned long long>(new_made), static_cast<unsigned long long>(new_reused),
                static_cast<unsigned long long>(new_shared),
                EtcDecoderIsVectorized() ? "" : " (the portable decoder: this processor has no SSSE3)");
  return text;
}

VulkanEtc2Emulation& Etc2Emulation() {
  static VulkanEtc2Emulation* emulation = new VulkanEtc2Emulation;
  return *emulation;
}

}
