// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "graphics/vulkan/guest_vulkan.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "graphics/render_scale.h"
#include "graphics/vulkan/device_order.h"
#include "graphics/vulkan/etc2_emulation.h"
#include "graphics/vulkan/extension_policy.h"
#include "graphics/vulkan/generated/vk_sysv_thunks.h"
#include "graphics/vulkan/present_policy.h"
#include "graphics/vulkan/present_scaling.h"
#include "platform/windows/guest_tls.h"

namespace elixir::vulkan {
namespace {

namespace gen = generated;
using gen::CommandInfo;
using gen::CommandLevel;
using gen::kCommandCount;
using gen::kCommands;

void Log(const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  std::fputs("[vulkan] ", stderr);
  std::vfprintf(stderr, format, args);
  std::fputc('\n', stderr);
  va_end(args);
}

struct GuestCallback {
  void* function = nullptr;
  void* user_data = nullptr;
};

using GuestDebugUtilsFn = abi::SysvFn<VkBool32(VkDebugUtilsMessageSeverityFlagBitsEXT,
                                               VkDebugUtilsMessageTypeFlagsEXT,
                                               const VkDebugUtilsMessengerCallbackDataEXT*, void*)>;
using GuestDebugReportFn =
    abi::SysvFn<VkBool32(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, std::uint64_t,
                         std::size_t, std::int32_t, const char*, const char*, void*)>;
using GuestMemoryReportFn = abi::SysvFn<void(const VkDeviceMemoryReportCallbackDataEXT*, void*)>;

VkBool32 VKAPI_CALL HostDebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                           VkDebugUtilsMessageTypeFlagsEXT types,
                                           const VkDebugUtilsMessengerCallbackDataEXT* data,
                                           void* user_data) {
  const auto* callback = static_cast<const GuestCallback*>(user_data);
  if (callback == nullptr || callback->function == nullptr) return VK_FALSE;
  platform::EnsureGuestThread();
  return reinterpret_cast<GuestDebugUtilsFn>(callback->function)(severity, types, data,
                                                                 callback->user_data);
}

VkBool32 VKAPI_CALL HostDebugReportCallback(VkDebugReportFlagsEXT flags,
                                            VkDebugReportObjectTypeEXT object_type,
                                            std::uint64_t object, std::size_t location,
                                            std::int32_t message_code, const char* layer_prefix,
                                            const char* message, void* user_data) {
  const auto* callback = static_cast<const GuestCallback*>(user_data);
  if (callback == nullptr || callback->function == nullptr) return VK_FALSE;
  platform::EnsureGuestThread();
  return reinterpret_cast<GuestDebugReportFn>(callback->function)(
      flags, object_type, object, location, message_code, layer_prefix, message,
      callback->user_data);
}

void VKAPI_CALL HostMemoryReportCallback(const VkDeviceMemoryReportCallbackDataEXT* data,
                                         void* user_data) {
  const auto* callback = static_cast<const GuestCallback*>(user_data);
  if (callback == nullptr || callback->function == nullptr) return;
  platform::EnsureGuestThread();
  reinterpret_cast<GuestMemoryReportFn>(callback->function)(data, callback->user_data);
}

using CallbackList = std::vector<std::unique_ptr<GuestCallback>>;

GuestCallback* NewCallback(CallbackList* owner, const void* function, void* user_data) {
  owner->push_back(std::make_unique<GuestCallback>());
  owner->back()->function = const_cast<void*>(function);
  owner->back()->user_data = user_data;
  return owner->back().get();
}

struct ChainCopy {
  std::vector<std::unique_ptr<unsigned char[]>> storage;
  const void* head = nullptr;
};

template <typename Interesting, typename Edit>
VkResult CopyChain(const void* head, Interesting interesting, Edit edit, ChainCopy* out) {
  bool any = false;
  for (auto* node = static_cast<const VkBaseInStructure*>(head); node != nullptr; node = node->pNext) {
    if (interesting(node->sType)) {
      any = true;
      break;
    }
  }
  out->head = head;
  if (!any) return VK_SUCCESS;
  out->head = nullptr;
  VkBaseOutStructure* tail = nullptr;
  const auto append = [&](const void* node) {
    if (tail == nullptr) {
      out->head = node;
    } else {
      tail->pNext = static_cast<VkBaseOutStructure*>(const_cast<void*>(node));
    }
  };
  for (auto* node = static_cast<const VkBaseInStructure*>(head); node != nullptr;) {
    const VkBaseInStructure* next = node->pNext;
    const std::size_t size = gen::StructSizeForSType(node->sType);
    if (size == 0) {
      for (auto* rest = node; rest != nullptr; rest = rest->pNext) {
        if (interesting(rest->sType)) {
          Log("pNext chain: structure type %d precedes a callback structure and is unknown to "
              "vk.xml %u; cannot rewrite the chain", static_cast<int>(node->sType),
              gen::kRegistryHeaderVersion);
          return VK_ERROR_INITIALIZATION_FAILED;
        }
      }
      append(node);
      return VK_SUCCESS;
    }
    auto buffer = std::make_unique<unsigned char[]>(size);
    std::memcpy(buffer.get(), node, size);
    auto* copy = reinterpret_cast<VkBaseOutStructure*>(buffer.get());
    copy->pNext = nullptr;
    if (!interesting(node->sType) || edit(copy)) {
      append(copy);
      tail = copy;
      out->storage.push_back(std::move(buffer));
    }
    node = next;
  }
  return VK_SUCCESS;
}

struct InstanceRecord {
  std::vector<std::string> host_extensions;
  bool android_surface = false;
  bool surface_maintenance = false;
  CallbackList create_callbacks;
};

struct DeviceRecord {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  CallbackList memory_report_callbacks;
  bool exclusive_fullscreen_control = false;
  bool present_scaling = false;
};

struct CallbackObject {
  VkInstance instance = VK_NULL_HANDLE;
  CallbackList callback;
};

struct State {
  std::mutex mutex;
  std::unordered_map<VkInstance, std::unique_ptr<InstanceRecord>> instances;
  std::unordered_map<VkPhysicalDevice, VkInstance> physical_devices;
  std::unordered_map<VkDevice, std::unique_ptr<DeviceRecord>> devices;
  std::unordered_map<std::uint64_t, CallbackObject> messengers;
  std::unordered_map<std::uint64_t, CallbackObject> report_callbacks;
  PFN_vkVoidFunction trampolines[kCommandCount] = {};
  PFN_vkVoidFunction device_procs[kCommandCount] = {};
  PFN_vkVoidFunction fallback_procs[kCommandCount] = {};
  VkDevice specialized = VK_NULL_HANDLE;
  std::size_t specialized_count = 0;
};

State& GetState() {
  static State* state = new State;
  return *state;
}

CallbackObject TakeCallbackObject(std::unordered_map<std::uint64_t, CallbackObject>* objects,
                                  std::uint64_t handle) {
  CallbackObject object;
  std::lock_guard<std::mutex> lock(GetState().mutex);
  const auto it = objects->find(handle);
  if (it != objects->end()) {
    object = std::move(it->second);
    objects->erase(it);
  }
  return object;
}

struct WindowRecord {
  HWND hwnd = nullptr;
  RenderScale render_scale;
};

std::mutex& WindowMutex() {
  static std::mutex* mutex = new std::mutex;
  return *mutex;
}
std::unordered_map<const void*, WindowRecord>& Windows() {
  static auto* windows = new std::unordered_map<const void*, WindowRecord>;
  return *windows;
}
std::unordered_map<std::uint64_t, const void*>& SurfaceWindows() {
  static auto* surfaces = new std::unordered_map<std::uint64_t, const void*>;
  return *surfaces;
}

void RememberSurfaceWindow(VkSurfaceKHR surface, const void* anative_window) {
  std::lock_guard<std::mutex> lock(WindowMutex());
  SurfaceWindows()[reinterpret_cast<std::uint64_t>(surface)] = anative_window;
}

void ForgetSurfaceWindow(VkSurfaceKHR surface) {
  std::lock_guard<std::mutex> lock(WindowMutex());
  SurfaceWindows().erase(reinterpret_cast<std::uint64_t>(surface));
}

RenderScale SurfaceRenderScale(VkSurfaceKHR surface) {
  std::lock_guard<std::mutex> lock(WindowMutex());
  const auto owner = SurfaceWindows().find(reinterpret_cast<std::uint64_t>(surface));
  if (owner == SurfaceWindows().end()) return {};
  const auto window = Windows().find(owner->second);
  return window != Windows().end() ? window->second.render_scale : RenderScale{};
}

void ScaleSurfaceCapabilitiesForGuest(VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* capabilities) {
  if (capabilities == nullptr || !PresentStretchWanted()) return;
  if (PresentStretchState() == PresentStretch::kUnsupported) return;
  ScaleSurfaceCapabilities(SurfaceRenderScale(surface), capabilities);
}

void PublishLocked(State& state, std::size_t index) {
  PFN_vkVoidFunction entry = state.trampolines[index];
  if (kCommands[index].level == CommandLevel::kDevice && state.specialized != VK_NULL_HANDLE &&
      state.device_procs[index] != nullptr) {
    entry = state.device_procs[index];
  }
  if (entry == nullptr) entry = state.fallback_procs[index];
  gen::SetHostSlot(index, entry);
}

struct HostLoader {
  HMODULE module = nullptr;
  PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
  std::string error;

  HostLoader() {
    wchar_t directory[MAX_PATH];
    const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH - 16) {
      error = "GetSystemDirectoryW failed";
      return;
    }
    std::wstring path(directory, length);
    path += L"\\vulkan-1.dll";
    module = LoadLibraryW(path.c_str());
    if (module == nullptr) {
      error = "vulkan-1.dll is not installed (LoadLibraryW error " +
              std::to_string(GetLastError()) + ")";
      return;
    }
    get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        reinterpret_cast<void*>(GetProcAddress(module, "vkGetInstanceProcAddr")));
    if (get_instance_proc_addr == nullptr) {
      error = "vulkan-1.dll does not export vkGetInstanceProcAddr";
      return;
    }
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    for (std::size_t i = 0; i < kCommandCount; ++i) {
      if (kCommands[i].level != CommandLevel::kGlobal) continue;
      state.trampolines[i] = get_instance_proc_addr(VK_NULL_HANDLE, kCommands[i].name);
      PublishLocked(state, i);
    }
    state.trampolines[gen::kCmd_vkGetInstanceProcAddr] =
        reinterpret_cast<PFN_vkVoidFunction>(get_instance_proc_addr);
    PublishLocked(state, gen::kCmd_vkGetInstanceProcAddr);
  }
};

const HostLoader& Host() {
  static HostLoader* loader = new HostLoader;
  return *loader;
}
bool HostReady() { return Host().get_instance_proc_addr != nullptr; }

PFN_vkGetInstanceProcAddr HostGetInstanceProcAddr() { return ELIXIR_VK_HOST(vkGetInstanceProcAddr); }

PFN_vkVoidFunction GuestProc(const CommandInfo& info) {
  return reinterpret_cast<PFN_vkVoidFunction>(info.guest_proc);
}

bool ProvidedByAny(const CommandInfo& info, const std::vector<std::string>& extensions) {
  if (info.extensions == nullptr) return false;
  for (const std::string& name : extensions) {
    const char* list = info.extensions;
    while (*list != '\0') {
      const char* end = std::strchr(list, ',');
      const std::size_t n = end != nullptr ? static_cast<std::size_t>(end - list) : std::strlen(list);
      if (n == name.size() && std::strncmp(list, name.data(), n) == 0) return true;
      if (end == nullptr) break;
      list = end + 1;
    }
  }
  return false;
}

void FillInstanceLocked(State& state, VkInstance instance,
                        const std::vector<std::string>& extensions) {
  const PFN_vkGetInstanceProcAddr gipa = HostGetInstanceProcAddr();
  if (gipa == nullptr) return;
  for (std::size_t i = 0; i < kCommandCount; ++i) {
    const CommandInfo& info = kCommands[i];
    if (info.level == CommandLevel::kGlobal || (info.flags & gen::kCommandAndroid) != 0) continue;
    const bool wanted = (info.flags & (gen::kCommandCore | gen::kCommandLibvulkanExport)) != 0 ||
                        ProvidedByAny(info, extensions);
    if (!wanted) continue;
    if (PFN_vkVoidFunction entry = gipa(instance, info.name)) {
      state.trampolines[i] = entry;
      PublishLocked(state, i);
    }
  }
}

void RespecializeLocked(State& state) {
  const VkDevice target = state.devices.size() == 1 ? state.devices.begin()->first : VK_NULL_HANDLE;
  if (target == state.specialized) return;
  std::fill(std::begin(state.device_procs), std::end(state.device_procs), nullptr);
  state.specialized = target;
  state.specialized_count = 0;
  const auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      state.trampolines[gen::kCmd_vkGetDeviceProcAddr]);
  for (std::size_t i = 0; i < kCommandCount; ++i) {
    const CommandInfo& info = kCommands[i];
    if (info.level != CommandLevel::kDevice || (info.flags & gen::kCommandAndroid) != 0) continue;
    if (target != VK_NULL_HANDLE && gdpa != nullptr) {
      state.device_procs[i] = gdpa(target, info.name);
      if (state.device_procs[i] != nullptr) ++state.specialized_count;
    }
    PublishLocked(state, i);
  }
}

std::uint32_t ClampApiVersion(std::uint32_t version) {
  if (VK_API_VERSION_VARIANT(version) != 0) return version;
  const std::uint32_t major = VK_API_VERSION_MAJOR(version);
  const std::uint32_t minor = VK_API_VERSION_MINOR(version);
  const std::uint32_t max_major = VK_API_VERSION_MAJOR(gen::kMaxApiVersion);
  const std::uint32_t max_minor = VK_API_VERSION_MINOR(gen::kMaxApiVersion);
  if (major > max_major || (major == max_major && minor > max_minor)) return gen::kMaxApiVersion;
  return version;
}

template <typename T, typename Query>
VkResult EnumerateAll(Query query, std::vector<T>* values) {
  for (;;) {
    std::uint32_t count = 0;
    VkResult result = query(&count, nullptr);
    if (result != VK_SUCCESS) return result;
    values->resize(count);
    if (count == 0) return VK_SUCCESS;
    result = query(&count, values->data());
    if (result == VK_INCOMPLETE) continue;
    values->resize(count);
    return result;
  }
}

std::vector<std::string> Names(std::uint32_t count, const char* const* names) {
  std::vector<std::string> result;
  for (std::uint32_t i = 0; i < count; ++i) {
    if (names != nullptr && names[i] != nullptr) result.emplace_back(names[i]);
  }
  return result;
}

std::vector<const char*> Pointers(const std::vector<std::string>& names) {
  std::vector<const char*> result;
  result.reserve(names.size());
  for (const std::string& name : names) result.push_back(name.c_str());
  return result;
}

void RecordPhysicalDevices(VkInstance instance, std::uint32_t count, const VkPhysicalDevice* devices) {
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  for (std::uint32_t i = 0; i < count; ++i) state.physical_devices[devices[i]] = instance;
}

void RecordPhysicalDeviceGroups(VkInstance instance, std::uint32_t count,
                                const VkPhysicalDeviceGroupProperties* groups) {
  for (std::uint32_t i = 0; i < count; ++i) {
    RecordPhysicalDevices(instance, groups[i].physicalDeviceCount, groups[i].physicalDevices);
  }
}

std::atomic<std::uint64_t> g_announced_device_order{0};

void OrderPhysicalDevicesForGuest(std::vector<VkPhysicalDevice>* devices, bool announce) {
  if (devices->size() < 2) return;
  const DeviceOrderPolicy policy = CachedDeviceOrderPolicy();
  DeviceQueries queries;
  queries.properties = ELIXIR_VK_HOST(vkGetPhysicalDeviceProperties);
  queries.queue_families = ELIXIR_VK_HOST(vkGetPhysicalDeviceQueueFamilyProperties);
  queries.can_present = ELIXIR_VK_HOST(vkGetPhysicalDeviceWin32PresentationSupportKHR);
  std::vector<DeviceCandidate> candidates;
  std::vector<std::string> names(devices->size());
  candidates.reserve(devices->size());
  for (std::size_t index = 0; index < devices->size(); ++index) {
    candidates.push_back(DescribeDevice((*devices)[index], queries, &names[index]));
  }
  const std::size_t preferred = PreferredDevice(policy, candidates);
  if (announce) {
    const std::uint64_t signature = (reinterpret_cast<std::uintptr_t>((*devices)[preferred]) << 8) ^
                                    (devices->size() << 4) ^ (static_cast<std::uint64_t>(policy) + 1);
    if (g_announced_device_order.exchange(signature, std::memory_order_relaxed) != signature) {
      std::string listed;
      for (std::size_t index = 0; index < devices->size(); ++index) {
        if (!listed.empty()) listed += ", ";
        listed += names[index] + " (" + PhysicalDeviceTypeName(candidates[index].type) +
                  (candidates[index].can_present ? ")" : ", cannot show a window)");
      }
      Log("graphics cards: %s; Roblox is offered %s first (order: %s)", listed.c_str(), names[preferred].c_str(),
          DeviceOrderPolicyName(policy));
    }
  }
  if (preferred != 0) {
    std::rotate(devices->begin(), devices->begin() + static_cast<std::ptrdiff_t>(preferred),
                devices->begin() + static_cast<std::ptrdiff_t>(preferred) + 1);
  }
}

void ClampProperties(VkPhysicalDeviceProperties* properties) {
  if (properties != nullptr) properties->apiVersion = ClampApiVersion(properties->apiVersion);
}

[[gnu::always_inline]] inline VulkanEtc2Emulation* ActiveEtc2() {
  return Etc2EmulationActive() ? &Etc2Emulation() : nullptr;
}

bool Etc2EmulatedPhysicalDevice(VkPhysicalDevice physical_device) {
  return Etc2Emulation().PhysicalDeviceNeedsEmulation(physical_device, ELIXIR_VK_HOST(vkGetPhysicalDeviceFeatures));
}

bool Etc2Advertised(VkPhysicalDevice physical_device) {
  return Etc2EmulatedPhysicalDevice(physical_device) && Etc2SupportAdvertised();
}

bool HostHasSwapchainMaintenance(VkPhysicalDevice physical_device) {
  auto query = ELIXIR_VK_HOST(vkGetPhysicalDeviceFeatures2);
  if (query == nullptr) query = ELIXIR_VK_HOST(vkGetPhysicalDeviceFeatures2KHR);
  if (query == nullptr) return false;
  VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maintenance{};
  maintenance.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &maintenance;
  query(physical_device, &features);
  return maintenance.swapchainMaintenance1 != VK_FALSE;
}

const void* FindInChain(const void* head, VkStructureType type) {
  for (auto* node = static_cast<const VkBaseInStructure*>(head); node != nullptr; node = node->pNext) {
    if (node->sType == type) return node;
  }
  return nullptr;
}

void ApplyEmulatedFormatProperties(VkFormatProperties* properties) {
  properties->linearTilingFeatures = 0;
  properties->optimalTilingFeatures = EmulatedEtc2FormatFeatures(properties->optimalTilingFeatures);
  properties->bufferFeatures = 0;
}

template <typename Host>
void GetFormatProperties2(Host host, VkPhysicalDevice physical_device, VkFormat format,
                          VkFormatProperties2* properties) {
  Etc2EmulatedFormat mapped;
  if (!LookupEmulatedEtc2Format(format, &mapped) || !Etc2EmulatedPhysicalDevice(physical_device)) {
    host(physical_device, format, properties);
    return;
  }
  host(physical_device, mapped.host_format, properties);
  ApplyEmulatedFormatProperties(&properties->formatProperties);
  if (auto* extended = const_cast<VkFormatProperties3*>(static_cast<const VkFormatProperties3*>(
          FindInChain(properties->pNext, VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3)))) {
    extended->linearTilingFeatures = 0;
    extended->optimalTilingFeatures =
        EmulatedEtc2FormatFeatures(static_cast<VkFormatFeatureFlags>(extended->optimalTilingFeatures));
    extended->bufferFeatures = 0;
  }
}

const void* MapViewFormatList(const void* head, std::vector<VkFormat>* view_formats, ChainCopy* chain) {
  const auto* list = static_cast<const VkImageFormatListCreateInfo*>(
      FindInChain(head, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO));
  if (list == nullptr || list->pViewFormats == nullptr) return head;
  bool any = false;
  for (std::uint32_t i = 0; i < list->viewFormatCount; ++i) {
    Etc2EmulatedFormat mapped;
    VkFormat format = list->pViewFormats[i];
    if (LookupEmulatedEtc2Format(format, &mapped)) {
      format = mapped.host_format;
      any = true;
    }
    if (std::find(view_formats->begin(), view_formats->end(), format) == view_formats->end()) {
      view_formats->push_back(format);
    }
  }
  if (!any) return head;
  const VkResult result = CopyChain(
      head, [](VkStructureType type) { return type == VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO; },
      [&](VkBaseOutStructure* node) {
        auto* copy = reinterpret_cast<VkImageFormatListCreateInfo*>(node);
        copy->viewFormatCount = static_cast<std::uint32_t>(view_formats->size());
        copy->pViewFormats = view_formats->data();
        return true;
      },
      chain);
  return result == VK_SUCCESS ? chain->head : head;
}

constexpr VkImageUsageFlags kEmulatedEtc2Usage =
    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
constexpr VkImageCreateFlags kSparseImageFlags =
    VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;

bool EmulatedImageFormatSupported(VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage,
                                  VkImageCreateFlags flags) {
  return tiling == VK_IMAGE_TILING_OPTIMAL && type == VK_IMAGE_TYPE_2D && (usage & ~kEmulatedEtc2Usage) == 0 &&
         (flags & kSparseImageFlags) == 0;
}

template <typename Host>
VkResult GetImageFormatProperties2(Host host, VkPhysicalDevice physical_device,
                                   const VkPhysicalDeviceImageFormatInfo2* info,
                                   VkImageFormatProperties2* properties) {
  Etc2EmulatedFormat mapped;
  if (info == nullptr || !LookupEmulatedEtc2Format(info->format, &mapped) ||
      !Etc2EmulatedPhysicalDevice(physical_device)) {
    return host(physical_device, info, properties);
  }
  if (!EmulatedImageFormatSupported(info->type, info->tiling, info->usage, info->flags)) {
    if (properties != nullptr) properties->imageFormatProperties = {};
    return VK_ERROR_FORMAT_NOT_SUPPORTED;
  }
  VkPhysicalDeviceImageFormatInfo2 host_info = *info;
  host_info.format = mapped.host_format;
  host_info.flags &= ~static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT);
  std::vector<VkFormat> view_formats;
  ChainCopy chain;
  host_info.pNext = MapViewFormatList(info->pNext, &view_formats, &chain);
  const VkResult result = host(physical_device, &host_info, properties);
  if (result == VK_SUCCESS && properties != nullptr) {
    properties->imageFormatProperties.sampleCounts = VK_SAMPLE_COUNT_1_BIT;
  }
  return result;
}

struct EmulatedImageInfo {
  VkImageCreateInfo create_info{};
  ChainCopy chain;
  std::vector<VkFormat> view_formats;
};

void PrepareEmulatedImageInfo(const VkImageCreateInfo& guest, EmulatedImageInfo* out) {
  out->create_info = guest;
  out->create_info.pNext = MapViewFormatList(guest.pNext, &out->view_formats, &out->chain);
}

bool HostQueryImageInfo(VkDevice device, const VkImageCreateInfo* guest, EmulatedImageInfo* prepared,
                        VkImageCreateInfo* host) {
  VulkanEtc2Emulation* etc2 = ActiveEtc2();
  if (guest == nullptr || etc2 == nullptr || !LookupEmulatedEtc2Format(guest->format, nullptr) ||
      !etc2->Emulates(device)) {
    return false;
  }
  PrepareEmulatedImageInfo(*guest, prepared);
  return etc2->HostImageCreateInfo(device, prepared->create_info, host);
}

void PrepareEtc2Submit(std::uint32_t submit_count, const VkSubmitInfo* submits) {
  VulkanEtc2Emulation* etc2 = ActiveEtc2();
  if (submits == nullptr || etc2 == nullptr || !etc2->HasPendingUploads()) return;
  if (submit_count == 1) {
    etc2->PrepareSubmit(submits->pCommandBuffers, submits->commandBufferCount);
    return;
  }
  std::vector<VkCommandBuffer> command_buffers;
  for (std::uint32_t index = 0; index < submit_count; ++index) {
    if (submits[index].commandBufferCount != 0 && submits[index].pCommandBuffers != nullptr) {
      command_buffers.insert(command_buffers.end(), submits[index].pCommandBuffers,
                             submits[index].pCommandBuffers + submits[index].commandBufferCount);
    }
  }
  etc2->PrepareSubmit(command_buffers.data(), static_cast<std::uint32_t>(command_buffers.size()));
}

void PrepareEtc2Submit2(std::uint32_t submit_count, const VkSubmitInfo2* submits) {
  VulkanEtc2Emulation* etc2 = ActiveEtc2();
  if (submits == nullptr || etc2 == nullptr || !etc2->HasPendingUploads()) return;
  std::vector<VkCommandBuffer> command_buffers;
  for (std::uint32_t index = 0; index < submit_count; ++index) {
    if (submits[index].pCommandBufferInfos == nullptr) continue;
    for (std::uint32_t info = 0; info < submits[index].commandBufferInfoCount; ++info) {
      command_buffers.push_back(submits[index].pCommandBufferInfos[info].commandBuffer);
    }
  }
  etc2->PrepareSubmit(command_buffers.data(), static_cast<std::uint32_t>(command_buffers.size()));
}

}

std::atomic<PresentHook> g_present_hook{nullptr};
std::atomic<std::uint64_t> g_presented_frames{0};

void SetPresentHook(PresentHook hook) { g_present_hook.store(hook, std::memory_order_release); }

std::uint64_t PresentedFrameCount() { return g_presented_frames.load(std::memory_order_relaxed); }

void RegisterNativeWindow(const void* anative_window, HWND hwnd) {
  std::lock_guard<std::mutex> lock(WindowMutex());
  Windows()[anative_window].hwnd = hwnd;
}

void UnregisterNativeWindow(const void* anative_window) {
  std::lock_guard<std::mutex> lock(WindowMutex());
  Windows().erase(anative_window);
}

HWND FindNativeWindow(const void* anative_window) {
  std::lock_guard<std::mutex> lock(WindowMutex());
  const auto it = Windows().find(anative_window);
  return it != Windows().end() ? it->second.hwnd : nullptr;
}

void SetNativeWindowRenderScale(const void* anative_window, const RenderScale& scale) {
  std::lock_guard<std::mutex> lock(WindowMutex());
  const auto it = Windows().find(anative_window);
  if (it != Windows().end()) it->second.render_scale = Normalized(scale);
}

RenderScale NativeWindowRenderScale(const void* anative_window) {
  std::lock_guard<std::mutex> lock(WindowMutex());
  const auto it = Windows().find(anative_window);
  return it != Windows().end() ? it->second.render_scale : RenderScale{};
}

bool InitializeHostVulkan(std::string* error) {
  const HostLoader& host = Host();
  if (host.get_instance_proc_addr != nullptr) return true;
  if (error != nullptr) *error = host.error;
  return false;
}

void RegisterVulkanExports(std::unordered_map<std::string, void*>& exports) {
  for (std::size_t i = 0; i < gen::kLibvulkanExportCount; ++i) {
    const CommandInfo* info = gen::FindCommand(gen::kLibvulkanExports[i]);
    if (info != nullptr && info->guest_proc != nullptr) exports[info->name] = info->guest_proc;
  }
}

GuestGetInstanceProcAddrFn GuestGetInstanceProcAddr() { return &elixir_vk_intercept_vkGetInstanceProcAddr; }

DispatchState GetDispatchState() {
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  DispatchState result;
  result.live_instances = state.instances.size();
  result.live_devices = state.devices.size();
  result.specialized_device = state.specialized;
  result.specialized_commands = state.specialized_count;
  result.live_debug_callbacks = state.messengers.size() + state.report_callbacks.size();
  return result;
}

PFN_vkVoidFunction CurrentHostEntryPoint(const char* name) {
  const CommandInfo* info = gen::FindCommand(name);
  if (info == nullptr) return nullptr;
  return gen::HostSlot(gen::CommandIndex(info));
}

}

using elixir::vulkan::ChainCopy;
using elixir::vulkan::CopyChain;
using elixir::vulkan::CopyEnumeration;
using elixir::vulkan::EnumerateAll;
using elixir::vulkan::g_present_hook;
using elixir::vulkan::g_presented_frames;
using elixir::vulkan::GetState;
using elixir::vulkan::GuestProc;
using elixir::vulkan::HostGetInstanceProcAddr;
using elixir::vulkan::HostReady;
using elixir::vulkan::Log;
using elixir::vulkan::State;
namespace gen = elixir::vulkan::generated;
namespace vk = elixir::vulkan;

extern "C" {

ELIXIR_SYSV PFN_vkVoidFunction elixir_vk_intercept_vkGetInstanceProcAddr(VkInstance instance,
                                                                         const char* pName) {
  const gen::CommandInfo* info = gen::FindCommand(pName);
  if (info == nullptr || info->guest_proc == nullptr || (info->flags & gen::kCommandHostOnly) != 0) {
    return nullptr;
  }
  if (info->level == gen::CommandLevel::kGlobal) return GuestProc(*info);
  if (instance == VK_NULL_HANDLE || !HostReady()) return nullptr;
  if ((info->flags & gen::kCommandAndroid) != 0) {
    if (std::strcmp(info->name, "vkCreateAndroidSurfaceKHR") != 0) return nullptr;
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto it = state.instances.find(instance);
    return it != state.instances.end() && it->second->android_surface ? GuestProc(*info) : nullptr;
  }
  const PFN_vkGetInstanceProcAddr gipa = HostGetInstanceProcAddr();
  const PFN_vkVoidFunction host = gipa != nullptr ? gipa(instance, info->name) : nullptr;
  if (host == nullptr) return nullptr;
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  const std::size_t index = gen::CommandIndex(info);
  if (state.trampolines[index] != host) {
    state.trampolines[index] = host;
    vk::PublishLocked(state, index);
  }
  return GuestProc(*info);
}

ELIXIR_SYSV PFN_vkVoidFunction elixir_vk_intercept_vkGetDeviceProcAddr(VkDevice device,
                                                                       const char* pName) {
  const gen::CommandInfo* info = gen::FindCommand(pName);
  if (info == nullptr || info->guest_proc == nullptr ||
      (info->flags & (gen::kCommandHostOnly | gen::kCommandAndroid)) != 0) {
    return nullptr;
  }
  if (info->level != gen::CommandLevel::kDevice || device == VK_NULL_HANDLE || !HostReady()) {
    return nullptr;
  }
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  const auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      state.trampolines[gen::kCmd_vkGetDeviceProcAddr]);
  if (gdpa == nullptr) return nullptr;
  const PFN_vkVoidFunction host = gdpa(device, info->name);
  if (host == nullptr) return nullptr;
  const std::size_t index = gen::CommandIndex(info);
  if (device == state.specialized && state.device_procs[index] == nullptr) {
    state.device_procs[index] = host;
    ++state.specialized_count;
  }
  if (state.trampolines[index] == nullptr) {
    const auto record = state.devices.find(device);
    const PFN_vkGetInstanceProcAddr gipa = HostGetInstanceProcAddr();
    if (record != state.devices.end() && gipa != nullptr) {
      state.trampolines[index] = gipa(record->second->instance, info->name);
    }
    if (state.trampolines[index] == nullptr) state.fallback_procs[index] = host;
  }
  vk::PublishLocked(state, index);
  return GuestProc(*info);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkEnumerateInstanceVersion(uint32_t* pApiVersion) {
  if (pApiVersion == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  if (!HostReady()) {
    *pApiVersion = VK_API_VERSION_1_0;
    return VK_SUCCESS;
  }
  const auto host = ELIXIR_VK_HOST(vkEnumerateInstanceVersion);
  if (host == nullptr) {
    *pApiVersion = VK_API_VERSION_1_0;
    return VK_SUCCESS;
  }
  const VkResult result = host(pApiVersion);
  if (result == VK_SUCCESS) *pApiVersion = vk::ClampApiVersion(*pApiVersion);
  return result;
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkEnumerateInstanceExtensionProperties(
    const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
  std::vector<VkExtensionProperties> host;
  if (HostReady()) {
    const auto enumerate = ELIXIR_VK_HOST(vkEnumerateInstanceExtensionProperties);
    if (enumerate == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    const VkResult result = EnumerateAll<VkExtensionProperties>(
        [&](uint32_t* count, VkExtensionProperties* out) { return enumerate(pLayerName, count, out); },
        &host);
    if (result != VK_SUCCESS) return result;
  } else if (pLayerName != nullptr) {
    return VK_ERROR_LAYER_NOT_PRESENT;
  }
  return CopyEnumeration(vk::GuestInstanceExtensions(host, pLayerName != nullptr), pPropertyCount,
                         pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkEnumerateInstanceLayerProperties(
    uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
  if (!HostReady()) return CopyEnumeration(std::vector<VkLayerProperties>{}, pPropertyCount, pProperties);
  const auto enumerate = ELIXIR_VK_HOST(vkEnumerateInstanceLayerProperties);
  if (enumerate == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  return enumerate(pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                                          const VkAllocationCallbacks* ,
                                                          VkInstance* pInstance) {
  if (pCreateInfo == nullptr || pInstance == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  if (!HostReady()) return VK_ERROR_INCOMPATIBLE_DRIVER;
  const auto create = ELIXIR_VK_HOST(vkCreateInstance);
  const auto enumerate = ELIXIR_VK_HOST(vkEnumerateInstanceExtensionProperties);
  if (create == nullptr || enumerate == nullptr) return VK_ERROR_INITIALIZATION_FAILED;

  std::vector<VkExtensionProperties> available;
  VkResult result = EnumerateAll<VkExtensionProperties>(
      [&](uint32_t* count, VkExtensionProperties* out) { return enumerate(nullptr, count, out); },
      &available);
  if (result != VK_SUCCESS) return result;

  vk::InstanceExtensionPlan plan;
  result = vk::PlanInstanceExtensions(
      vk::Names(pCreateInfo->enabledExtensionCount, pCreateInfo->ppEnabledExtensionNames), available,
      &plan);
  if (result != VK_SUCCESS) {
    Log("vkCreateInstance: VK_KHR_android_surface needs VK_KHR_surface + VK_KHR_win32_surface");
    return result;
  }
  for (const std::string& name : plan.dropped) {
    Log("vkCreateInstance: dropping hidden extension %s", name.c_str());
  }

  auto record = std::make_unique<vk::InstanceRecord>();
  record->host_extensions = plan.host_extensions;
  record->android_surface = plan.android_surface;
  record->surface_maintenance = plan.surface_maintenance;

  ChainCopy chain;
  result = CopyChain(
      pCreateInfo->pNext,
      [](VkStructureType type) {
        return type == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT ||
               type == VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT ||
               type == VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_LIST_LUNARG;
      },
      [&](VkBaseOutStructure* node) {
        if (node->sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT) {
          auto* info = reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(node);
          info->pUserData = vk::NewCallback(&record->create_callbacks,
                                            reinterpret_cast<const void*>(info->pfnUserCallback),
                                            info->pUserData);
          info->pfnUserCallback = &vk::HostDebugUtilsCallback;
          return true;
        }
        if (node->sType == VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT) {
          auto* info = reinterpret_cast<VkDebugReportCallbackCreateInfoEXT*>(node);
          info->pUserData = vk::NewCallback(&record->create_callbacks,
                                            reinterpret_cast<const void*>(info->pfnCallback),
                                            info->pUserData);
          info->pfnCallback = &vk::HostDebugReportCallback;
          return true;
        }
        Log("vkCreateInstance: dropping VkDirectDriverLoadingListLUNARG");
        return false;
      },
      &chain);
  if (result != VK_SUCCESS) return result;

  const std::vector<const char*> names = vk::Pointers(plan.host_extensions);
  VkInstanceCreateInfo host_info = *pCreateInfo;
  host_info.pNext = chain.head;
  host_info.enabledExtensionCount = static_cast<uint32_t>(names.size());
  host_info.ppEnabledExtensionNames = names.empty() ? nullptr : names.data();
  result = create(&host_info, nullptr, pInstance);
  if (result != VK_SUCCESS) return result;

  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  vk::FillInstanceLocked(state, *pInstance, record->host_extensions);
  state.instances[*pInstance] = std::move(record);
  return VK_SUCCESS;
}

ELIXIR_SYSV void elixir_vk_intercept_vkDestroyInstance(VkInstance instance,
                                                       const VkAllocationCallbacks* ) {
  if (instance == VK_NULL_HANDLE) return;
  State& state = GetState();
  std::unique_ptr<vk::InstanceRecord> record;
  std::vector<vk::CallbackObject> callbacks;
  std::vector<VkPhysicalDevice> physical_devices;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto it = state.instances.find(instance);
    if (it != state.instances.end()) {
      record = std::move(it->second);
      state.instances.erase(it);
    }
    for (auto pd = state.physical_devices.begin(); pd != state.physical_devices.end();) {
      if (pd->second == instance) {
        physical_devices.push_back(pd->first);
        pd = state.physical_devices.erase(pd);
      } else {
        ++pd;
      }
    }
    for (auto* objects : {&state.messengers, &state.report_callbacks}) {
      for (auto object = objects->begin(); object != objects->end();) {
        if (object->second.instance == instance) {
          callbacks.push_back(std::move(object->second));
          object = objects->erase(object);
        } else {
          ++object;
        }
      }
    }
  }
  if (const auto destroy = ELIXIR_VK_HOST(vkDestroyInstance)) destroy(instance, nullptr);
  vk::Etc2Emulation().ForgetPhysicalDevices(physical_devices.data(), physical_devices.size());
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkEnumeratePhysicalDevices(VkInstance instance,
                                                                    uint32_t* pPhysicalDeviceCount,
                                                                    VkPhysicalDevice* pPhysicalDevices) {
  const auto host = ELIXIR_VK_HOST(vkEnumeratePhysicalDevices);
  if (host == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  if (pPhysicalDeviceCount == nullptr) return host(instance, pPhysicalDeviceCount, pPhysicalDevices);
  std::vector<VkPhysicalDevice> devices;
  const VkResult result = EnumerateAll<VkPhysicalDevice>(
      [&](uint32_t* count, VkPhysicalDevice* out) { return host(instance, count, out); }, &devices);
  if (result != VK_SUCCESS) return result;
  vk::RecordPhysicalDevices(instance, static_cast<std::uint32_t>(devices.size()), devices.data());
  vk::OrderPhysicalDevicesForGuest(&devices, pPhysicalDevices != nullptr);
  return CopyEnumeration(devices, pPhysicalDeviceCount, pPhysicalDevices);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkEnumeratePhysicalDeviceGroups(
    VkInstance instance, uint32_t* pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
  const auto host = ELIXIR_VK_HOST(vkEnumeratePhysicalDeviceGroups);
  if (host == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const VkResult result = host(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
  if (pPhysicalDeviceGroupProperties != nullptr && pPhysicalDeviceGroupCount != nullptr &&
      (result == VK_SUCCESS || result == VK_INCOMPLETE)) {
    vk::RecordPhysicalDeviceGroups(instance, *pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
  }
  return result;
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkEnumeratePhysicalDeviceGroupsKHR(
    VkInstance instance, uint32_t* pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
  const auto host = ELIXIR_VK_HOST(vkEnumeratePhysicalDeviceGroupsKHR);
  if (host == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const VkResult result = host(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
  if (pPhysicalDeviceGroupProperties != nullptr && pPhysicalDeviceGroupCount != nullptr &&
      (result == VK_SUCCESS || result == VK_INCOMPLETE)) {
    vk::RecordPhysicalDeviceGroups(instance, *pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
  }
  return result;
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetPhysicalDeviceProperties(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
  if (const auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceProperties)) {
    host(physicalDevice, pProperties);
    vk::ClampProperties(pProperties);
  }
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetPhysicalDeviceProperties2(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
  if (const auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceProperties2)) {
    host(physicalDevice, pProperties);
    if (pProperties != nullptr) vk::ClampProperties(&pProperties->properties);
  }
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetPhysicalDeviceProperties2KHR(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
  if (const auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceProperties2KHR)) {
    host(physicalDevice, pProperties);
    if (pProperties != nullptr) vk::ClampProperties(&pProperties->properties);
  }
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties) {
  const auto enumerate = ELIXIR_VK_HOST(vkEnumerateDeviceExtensionProperties);
  if (enumerate == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  std::vector<VkExtensionProperties> host;
  const VkResult result = EnumerateAll<VkExtensionProperties>(
      [&](uint32_t* count, VkExtensionProperties* out) {
        return enumerate(physicalDevice, pLayerName, count, out);
      },
      &host);
  if (result != VK_SUCCESS) return result;
  return CopyEnumeration(vk::GuestDeviceExtensions(host), pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkCreateDevice(VkPhysicalDevice physicalDevice,
                                                        const VkDeviceCreateInfo* pCreateInfo,
                                                        const VkAllocationCallbacks* ,
                                                        VkDevice* pDevice) {
  if (pCreateInfo == nullptr || pDevice == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const auto create = ELIXIR_VK_HOST(vkCreateDevice);
  if (create == nullptr) return VK_ERROR_INITIALIZATION_FAILED;

  std::vector<std::string> dropped;
  std::vector<std::string> extensions = vk::PlanDeviceExtensions(
      vk::Names(pCreateInfo->enabledExtensionCount, pCreateInfo->ppEnabledExtensionNames), &dropped);
  for (const std::string& name : dropped) {
    Log("vkCreateDevice: dropping hidden extension %s", name.c_str());
  }

  auto record = std::make_unique<vk::DeviceRecord>();
  record->physical_device = physicalDevice;
  const char* maintenance = nullptr;
  {
    constexpr char kExclusive[] = "VK_EXT_full_screen_exclusive";
    bool instance_ready = false;
    std::vector<std::string> instance_extensions;
    {
      State& state = GetState();
      std::lock_guard<std::mutex> lock(state.mutex);
      const auto owner = state.physical_devices.find(physicalDevice);
      const auto instance = owner != state.physical_devices.end() ? state.instances.find(owner->second)
                            : state.instances.size() == 1         ? state.instances.begin()
                                                                  : state.instances.end();
      if (instance != state.instances.end()) {
        instance_extensions = instance->second->host_extensions;
        instance_ready = std::find(instance_extensions.begin(), instance_extensions.end(),
                                   "VK_KHR_get_surface_capabilities2") != instance_extensions.end();
      }
    }
    const bool swapchains = std::find(extensions.begin(), extensions.end(), "VK_KHR_swapchain") != extensions.end();
    const auto enumerate_host = ELIXIR_VK_HOST(vkEnumerateDeviceExtensionProperties);
    if (instance_ready && swapchains && enumerate_host != nullptr) {
      std::vector<VkExtensionProperties> host_extensions;
      const VkResult enumerated = EnumerateAll<VkExtensionProperties>(
          [&](uint32_t* count, VkExtensionProperties* out) {
            return enumerate_host(physicalDevice, nullptr, count, out);
          },
          &host_extensions);
      bool supported = false;
      for (const VkExtensionProperties& extension : host_extensions) {
        supported = supported || std::strcmp(extension.extensionName, kExclusive) == 0;
      }
      if (enumerated == VK_SUCCESS && supported) {
        if (std::find(extensions.begin(), extensions.end(), kExclusive) == extensions.end()) {
          extensions.emplace_back(kExclusive);
        }
        record->exclusive_fullscreen_control = true;
      }
      if (enumerated == VK_SUCCESS && vk::PresentStretchWanted()) {
        maintenance = vk::SwapchainMaintenanceExtension(instance_extensions, host_extensions);
      }
    }
  }
  VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maintenance_features{};
  bool maintenance_added = false;
  if (maintenance != nullptr) {
    const auto* guest_maintenance = static_cast<const VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR*>(
        vk::FindInChain(pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR));
    if (guest_maintenance != nullptr) {
      record->present_scaling = guest_maintenance->swapchainMaintenance1 != VK_FALSE;
    } else if (vk::HostHasSwapchainMaintenance(physicalDevice)) {
      maintenance_added = true;
      if (std::find(extensions.begin(), extensions.end(), maintenance) == extensions.end()) {
        extensions.emplace_back(maintenance);
      }
    }
  }
  const bool etc2_emulated = vk::Etc2EmulatedPhysicalDevice(physicalDevice);
  const auto* guest_features2 = static_cast<const VkPhysicalDeviceFeatures2*>(
      vk::FindInChain(pCreateInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2));
  const bool clear_features2 =
      etc2_emulated && guest_features2 != nullptr && guest_features2->features.textureCompressionETC2 != VK_FALSE;
  ChainCopy chain;
  VkResult result = CopyChain(
      pCreateInfo->pNext,
      [clear_features2](VkStructureType type) {
        return type == VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT ||
               (clear_features2 && type == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
      },
      [&](VkBaseOutStructure* node) {
        if (node->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
          reinterpret_cast<VkPhysicalDeviceFeatures2*>(node)->features.textureCompressionETC2 = VK_FALSE;
          return true;
        }
        auto* info = reinterpret_cast<VkDeviceDeviceMemoryReportCreateInfoEXT*>(node);
        info->pUserData = vk::NewCallback(&record->memory_report_callbacks,
                                          reinterpret_cast<const void*>(info->pfnUserCallback),
                                          info->pUserData);
        info->pfnUserCallback = &vk::HostMemoryReportCallback;
        return true;
      },
      &chain);
  if (result != VK_SUCCESS) return result;

  std::vector<const char*> names = vk::Pointers(extensions);
  VkDeviceCreateInfo host_info = *pCreateInfo;
  host_info.pNext = chain.head;
  host_info.enabledExtensionCount = static_cast<uint32_t>(names.size());
  host_info.ppEnabledExtensionNames = names.empty() ? nullptr : names.data();
  VkPhysicalDeviceFeatures host_features{};
  if (etc2_emulated && pCreateInfo->pEnabledFeatures != nullptr) {
    host_features = *pCreateInfo->pEnabledFeatures;
    host_features.textureCompressionETC2 = VK_FALSE;
    host_info.pEnabledFeatures = &host_features;
  }
  if (maintenance_added) {
    maintenance_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
    maintenance_features.pNext = const_cast<void*>(chain.head);
    maintenance_features.swapchainMaintenance1 = VK_TRUE;
    host_info.pNext = &maintenance_features;
  }
  result = create(physicalDevice, &host_info, nullptr, pDevice);
  if (result != VK_SUCCESS && maintenance_added) {
    Log("vkCreateDevice: the driver refused %s (%d); creating the device without it", maintenance,
        static_cast<int>(result));
    maintenance_added = false;
    extensions.erase(std::remove(extensions.begin(), extensions.end(), std::string(maintenance)), extensions.end());
    names = vk::Pointers(extensions);
    host_info.pNext = chain.head;
    host_info.enabledExtensionCount = static_cast<uint32_t>(names.size());
    host_info.ppEnabledExtensionNames = names.empty() ? nullptr : names.data();
    result = create(physicalDevice, &host_info, nullptr, pDevice);
  }
  if (result != VK_SUCCESS) return result;
  record->present_scaling = record->present_scaling || maintenance_added;
  if (vk::PresentStretchWanted()) {
    if (record->present_scaling) {
      Log("vkCreateDevice: %s is on, so a smaller picture can be stretched to the window", maintenance);
    } else {
      vk::NotePresentStretch(false);
      Log("vkCreateDevice: this graphics driver cannot stretch a smaller picture to the window");
    }
  }

  VkPhysicalDeviceMemoryProperties memory_properties{};
  if (const auto memory_query = ELIXIR_VK_HOST(vkGetPhysicalDeviceMemoryProperties)) {
    memory_query(physicalDevice, &memory_properties);
  }
  PFN_vkGetDeviceProcAddr device_proc_addr = nullptr;
  {
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    device_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(state.trampolines[gen::kCmd_vkGetDeviceProcAddr]);
  }
  vk::Etc2Emulation().RegisterDevice(*pDevice, physicalDevice, etc2_emulated, memory_properties, device_proc_addr);

  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  const auto owner = state.physical_devices.find(physicalDevice);
  if (owner != state.physical_devices.end()) {
    record->instance = owner->second;
  } else if (state.instances.size() == 1) {
    record->instance = state.instances.begin()->first;
  }
  state.devices[*pDevice] = std::move(record);
  vk::RespecializeLocked(state);
  return VK_SUCCESS;
}

ELIXIR_SYSV void elixir_vk_intercept_vkDestroyDevice(VkDevice device,
                                                     const VkAllocationCallbacks* ) {
  if (device == VK_NULL_HANDLE) return;
  State& state = GetState();
  PFN_vkDestroyDevice destroy = nullptr;
  std::unique_ptr<vk::DeviceRecord> record;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    destroy = reinterpret_cast<PFN_vkDestroyDevice>(state.trampolines[gen::kCmd_vkDestroyDevice]);
    const auto it = state.devices.find(device);
    if (it != state.devices.end()) {
      record = std::move(it->second);
      state.devices.erase(it);
    }
    vk::RespecializeLocked(state);
  }
  vk::Etc2Emulation().DestroyDevice(device);
  if (destroy != nullptr) destroy(device, nullptr);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkCreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* , VkDebugUtilsMessengerEXT* pMessenger) {
  const auto create = ELIXIR_VK_HOST(vkCreateDebugUtilsMessengerEXT);
  if (create == nullptr || pCreateInfo == nullptr || pMessenger == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  vk::CallbackObject object;
  object.instance = instance;
  VkDebugUtilsMessengerCreateInfoEXT host_info = *pCreateInfo;
  host_info.pUserData = vk::NewCallback(&object.callback,
                                        reinterpret_cast<const void*>(pCreateInfo->pfnUserCallback),
                                        pCreateInfo->pUserData);
  host_info.pfnUserCallback = &vk::HostDebugUtilsCallback;
  const VkResult result = create(instance, &host_info, nullptr, pMessenger);
  if (result == VK_SUCCESS) {
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.messengers[reinterpret_cast<std::uint64_t>(*pMessenger)] = std::move(object);
  }
  return result;
}

ELIXIR_SYSV void elixir_vk_intercept_vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* ) {
  vk::CallbackObject object = vk::TakeCallbackObject(&GetState().messengers,
                                                     reinterpret_cast<std::uint64_t>(messenger));
  if (const auto destroy = ELIXIR_VK_HOST(vkDestroyDebugUtilsMessengerEXT)) {
    destroy(instance, messenger, nullptr);
  }
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkCreateDebugReportCallbackEXT(
    VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* , VkDebugReportCallbackEXT* pCallback) {
  const auto create = ELIXIR_VK_HOST(vkCreateDebugReportCallbackEXT);
  if (create == nullptr || pCreateInfo == nullptr || pCallback == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  vk::CallbackObject object;
  object.instance = instance;
  VkDebugReportCallbackCreateInfoEXT host_info = *pCreateInfo;
  host_info.pUserData = vk::NewCallback(&object.callback,
                                        reinterpret_cast<const void*>(pCreateInfo->pfnCallback),
                                        pCreateInfo->pUserData);
  host_info.pfnCallback = &vk::HostDebugReportCallback;
  const VkResult result = create(instance, &host_info, nullptr, pCallback);
  if (result == VK_SUCCESS) {
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.report_callbacks[reinterpret_cast<std::uint64_t>(*pCallback)] = std::move(object);
  }
  return result;
}

ELIXIR_SYSV void elixir_vk_intercept_vkDestroyDebugReportCallbackEXT(
    VkInstance instance, VkDebugReportCallbackEXT callback,
    const VkAllocationCallbacks* ) {
  vk::CallbackObject object = vk::TakeCallbackObject(&GetState().report_callbacks,
                                                     reinterpret_cast<std::uint64_t>(callback));
  if (const auto destroy = ELIXIR_VK_HOST(vkDestroyDebugReportCallbackEXT)) {
    destroy(instance, callback, nullptr);
  }
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkCreateAndroidSurfaceKHR(
    VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* , VkSurfaceKHR* pSurface) {
  if (pSurface != nullptr) *pSurface = VK_NULL_HANDLE;
  if (pCreateInfo == nullptr || pSurface == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const auto create = ELIXIR_VK_HOST(vkCreateWin32SurfaceKHR);
  if (create == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const HWND hwnd = vk::FindNativeWindow(pCreateInfo->window);
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    Log("vkCreateAndroidSurfaceKHR: ANativeWindow %p has no live HWND registered",
        static_cast<const void*>(pCreateInfo->window));
    return VK_ERROR_UNKNOWN;
  }
  VkWin32SurfaceCreateInfoKHR host_info{};
  host_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  host_info.hinstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
  if (host_info.hinstance == nullptr) host_info.hinstance = GetModuleHandleW(nullptr);
  host_info.hwnd = hwnd;
  const VkResult created = create(instance, &host_info, nullptr, pSurface);
  if (created == VK_SUCCESS) vk::RememberSurfaceWindow(*pSurface, pCreateInfo->window);
  return created;
}

ELIXIR_SYSV void elixir_vk_intercept_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                                                         const VkAllocationCallbacks* ) {
  if (surface != VK_NULL_HANDLE) vk::ForgetSurfaceWindow(surface);
  if (const auto destroy = ELIXIR_VK_HOST(vkDestroySurfaceKHR)) destroy(instance, surface, nullptr);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
  const auto query = ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
  if (query == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const VkResult result = query(physicalDevice, surface, pSurfaceCapabilities);
  if (result == VK_SUCCESS) vk::ScaleSurfaceCapabilitiesForGuest(surface, pSurfaceCapabilities);
  return result;
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkGetPhysicalDeviceSurfaceCapabilities2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
    VkSurfaceCapabilities2KHR* pSurfaceCapabilities) {
  const auto query = ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceCapabilities2KHR);
  if (query == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const VkResult result = query(physicalDevice, pSurfaceInfo, pSurfaceCapabilities);
  if (result == VK_SUCCESS && pSurfaceInfo != nullptr && pSurfaceCapabilities != nullptr) {
    vk::ScaleSurfaceCapabilitiesForGuest(pSurfaceInfo->surface, &pSurfaceCapabilities->surfaceCapabilities);
  }
  return result;
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkGetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount,
    VkPresentModeKHR* pPresentModes) {
  const auto query = ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfacePresentModesKHR);
  if (query == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  std::vector<VkPresentModeKHR> host;
  const VkResult result = EnumerateAll<VkPresentModeKHR>(
      [&](uint32_t* count, VkPresentModeKHR* out) { return query(physicalDevice, surface, count, out); },
      &host);
  if (result != VK_SUCCESS) return result;
  return CopyEnumeration(vk::FilterPresentModes(vk::CachedPresentModePolicy(), host),
                         pPresentModeCount, pPresentModes);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkCreateSwapchainKHR(VkDevice device,
                                                              const VkSwapchainCreateInfoKHR* pCreateInfo,
                                                              const VkAllocationCallbacks* ,
                                                              VkSwapchainKHR* pSwapchain) {
  const auto create = ELIXIR_VK_HOST(vkCreateSwapchainKHR);
  if (create == nullptr || pCreateInfo == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  VkSwapchainCreateInfoKHR host_info = *pCreateInfo;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  bool surface_maintenance = false;
  bool exclusive_fullscreen_control = false;
  bool present_scaling = false;
  {
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto device_record = state.devices.find(device);
    if (device_record != state.devices.end()) {
      physical_device = device_record->second->physical_device;
      exclusive_fullscreen_control = device_record->second->exclusive_fullscreen_control;
      present_scaling = device_record->second->present_scaling;
      const auto instance_record = state.instances.find(device_record->second->instance);
      surface_maintenance =
          instance_record != state.instances.end() && instance_record->second->surface_maintenance;
    }
  }
  if (physical_device != VK_NULL_HANDLE) {
    vk::VulkanSurfaceQueries queries;
    queries.present_modes = ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfacePresentModesKHR);
    queries.capabilities = ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    if (surface_maintenance) {
      queries.mode_capabilities = ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceCapabilities2KHR);
    }
    const VkResult policy = vk::ApplySwapchainPresentPolicy(vk::CachedPresentModePolicy(),
                                                            physical_device, queries, &host_info);
    if (policy != VK_SUCCESS) {
      Log("vkCreateSwapchainKHR: present policy query failed (%d); using the guest's request",
          static_cast<int>(policy));
      host_info = *pCreateInfo;
    }
  }
  VkSwapchainPresentScalingCreateInfoKHR scaling{};
  VkExtent2D window_extent{};
  bool stretched = false;
  if (present_scaling && physical_device != VK_NULL_HANDLE &&
      vk::FindInChain(host_info.pNext, VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_KHR) == nullptr) {
    vk::PresentScaling surface_scaling;
    const VkResult queried =
        vk::QueryPresentScaling(ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceCapabilities2KHR), physical_device,
                                host_info.surface, host_info.presentMode, &surface_scaling);
    if (queried == VK_SUCCESS) {
      vk::NotePresentStretch(surface_scaling.stretches);
      window_extent = surface_scaling.current_extent;
      stretched = surface_scaling.stretches && vk::PresentStretchState() == vk::PresentStretch::kSupported &&
                  vk::NeedsStretch(host_info.imageExtent, window_extent);
    } else {
      Log("vkCreateSwapchainKHR: the surface's scaling query failed (%d)", static_cast<int>(queried));
    }
  }
  if (stretched) {
    scaling.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_KHR;
    scaling.pNext = host_info.pNext;
    scaling.scalingBehavior = VK_PRESENT_SCALING_STRETCH_BIT_KHR;
    host_info.pNext = &scaling;
  }
  VkSurfaceFullScreenExclusiveInfoEXT exclusive{};
  const bool composed =
      exclusive_fullscreen_control &&
      vk::FindInChain(host_info.pNext, VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT) == nullptr;
  if (composed) {
    exclusive.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
    exclusive.pNext = const_cast<void*>(host_info.pNext);
    exclusive.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;
    host_info.pNext = &exclusive;
  }
  VkResult created = create(device, &host_info, nullptr, pSwapchain);
  if (created != VK_SUCCESS && stretched) {
    Log("vkCreateSwapchainKHR: the driver refused a stretched %ux%u swapchain (%d); creating it without stretching",
        host_info.imageExtent.width, host_info.imageExtent.height, static_cast<int>(created));
    vk::NotePresentStretchRefused();
    stretched = false;
    if (composed) {
      exclusive.pNext = const_cast<void*>(scaling.pNext);
    } else {
      host_info.pNext = scaling.pNext;
    }
    created = create(device, &host_info, nullptr, pSwapchain);
  }
  char stretch_note[64] = "";
  if (stretched) {
    std::snprintf(stretch_note, sizeof(stretch_note), ", stretched to the window's %ux%u", window_extent.width,
                  window_extent.height);
  }
  Log("swapchain %ux%u, %u images, present mode %s (the app asked for %s; policy %s)%s%s: %d",
      host_info.imageExtent.width, host_info.imageExtent.height, host_info.minImageCount,
      vk::PresentModeKhrName(host_info.presentMode), vk::PresentModeKhrName(pCreateInfo->presentMode),
      vk::PresentModePolicyName(vk::CachedPresentModePolicy()), composed ? ", exclusive fullscreen declined" : "",
      stretch_note, static_cast<int>(created));
  return created;
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                               uint64_t timeout, VkSemaphore semaphore,
                                                               VkFence fence, uint32_t* pImageIndex) {
  const auto acquire = ELIXIR_VK_HOST(vkAcquireNextImageKHR);
  if (acquire == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const elixir::platform::GuestThreadResume resume;
  return vk::NormalizeAndroidSwapchainResult(
      acquire(device, swapchain, timeout, semaphore, fence, pImageIndex));
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkAcquireNextImage2KHR(VkDevice device,
                                                                const VkAcquireNextImageInfoKHR* pAcquireInfo,
                                                                uint32_t* pImageIndex) {
  const auto acquire = ELIXIR_VK_HOST(vkAcquireNextImage2KHR);
  if (acquire == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const elixir::platform::GuestThreadResume resume;
  return vk::NormalizeAndroidSwapchainResult(acquire(device, pAcquireInfo, pImageIndex));
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkQueuePresentKHR(VkQueue queue,
                                                           const VkPresentInfoKHR* pPresentInfo) {
  const auto present = ELIXIR_VK_HOST(vkQueuePresentKHR);
  if (present == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  const elixir::platform::GuestThreadResume resume;
  const VkResult result = present(queue, pPresentInfo);
  const elixir::vulkan::PresentHook hook = g_present_hook.load(std::memory_order_acquire);
  if (pPresentInfo != nullptr && pPresentInfo->pResults != nullptr) {
    for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) {
      if (pPresentInfo->pResults[i] == VK_SUCCESS || pPresentInfo->pResults[i] == VK_SUBOPTIMAL_KHR) {
        g_presented_frames.fetch_add(1, std::memory_order_relaxed);
      }
      if (hook != nullptr) hook(pPresentInfo->pResults[i]);
      pPresentInfo->pResults[i] = vk::NormalizeAndroidSwapchainResult(pPresentInfo->pResults[i]);
    }
  } else {
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) g_presented_frames.fetch_add(1, std::memory_order_relaxed);
    if (hook != nullptr) hook(result);
  }
  return vk::NormalizeAndroidSwapchainResult(result);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkGetAndroidHardwareBufferPropertiesANDROID(
    VkDevice , const struct AHardwareBuffer* ,
    VkAndroidHardwareBufferPropertiesANDROID* ) {
  return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkGetMemoryAndroidHardwareBufferANDROID(
    VkDevice , const VkMemoryGetAndroidHardwareBufferInfoANDROID* ,
    struct AHardwareBuffer** pBuffer) {
  if (pBuffer != nullptr) *pBuffer = nullptr;
  return VK_ERROR_UNKNOWN;
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice,
                                                                 VkPhysicalDeviceFeatures* pFeatures) {
  const auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFeatures);
  if (host == nullptr || pFeatures == nullptr) return;
  host(physicalDevice, pFeatures);
  if (vk::Etc2Advertised(physicalDevice)) pFeatures->textureCompressionETC2 = VK_TRUE;
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                                                                  VkPhysicalDeviceFeatures2* pFeatures) {
  auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFeatures2);
  if (host == nullptr) host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFeatures2KHR);
  if (host == nullptr || pFeatures == nullptr) return;
  host(physicalDevice, pFeatures);
  if (vk::Etc2Advertised(physicalDevice)) pFeatures->features.textureCompressionETC2 = VK_TRUE;
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice,
                                                                     VkPhysicalDeviceFeatures2* pFeatures) {
  auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFeatures2KHR);
  if (host == nullptr) host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFeatures2);
  if (host == nullptr || pFeatures == nullptr) return;
  host(physicalDevice, pFeatures);
  if (vk::Etc2Advertised(physicalDevice)) pFeatures->features.textureCompressionETC2 = VK_TRUE;
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice,
                                                                         VkFormat format,
                                                                         VkFormatProperties* pFormatProperties) {
  const auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFormatProperties);
  if (host == nullptr || pFormatProperties == nullptr) return;
  vk::Etc2EmulatedFormat mapped;
  if (!vk::LookupEmulatedEtc2Format(format, &mapped) || !vk::Etc2EmulatedPhysicalDevice(physicalDevice)) {
    host(physicalDevice, format, pFormatProperties);
    return;
  }
  host(physicalDevice, mapped.host_format, pFormatProperties);
  vk::ApplyEmulatedFormatProperties(pFormatProperties);
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice,
                                                                          VkFormat format,
                                                                          VkFormatProperties2* pFormatProperties) {
  auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFormatProperties2);
  if (host == nullptr) host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFormatProperties2KHR);
  if (host == nullptr || pFormatProperties == nullptr) return;
  vk::GetFormatProperties2(host, physicalDevice, format, pFormatProperties);
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice physicalDevice,
                                                                             VkFormat format,
                                                                             VkFormatProperties2* pFormatProperties) {
  auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFormatProperties2KHR);
  if (host == nullptr) host = ELIXIR_VK_HOST(vkGetPhysicalDeviceFormatProperties2);
  if (host == nullptr || pFormatProperties == nullptr) return;
  vk::GetFormatProperties2(host, physicalDevice, format, pFormatProperties);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling,
    VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
  const auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceImageFormatProperties);
  if (host == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  vk::Etc2EmulatedFormat mapped;
  if (!vk::LookupEmulatedEtc2Format(format, &mapped) || !vk::Etc2EmulatedPhysicalDevice(physicalDevice)) {
    return host(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
  }
  if (!vk::EmulatedImageFormatSupported(type, tiling, usage, flags)) {
    if (pImageFormatProperties != nullptr) *pImageFormatProperties = {};
    return VK_ERROR_FORMAT_NOT_SUPPORTED;
  }
  const VkResult result =
      host(physicalDevice, mapped.host_format, type, tiling, usage,
           flags & ~static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT),
           pImageFormatProperties);
  if (result == VK_SUCCESS && pImageFormatProperties != nullptr) {
    pImageFormatProperties->sampleCounts = VK_SAMPLE_COUNT_1_BIT;
  }
  return result;
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkGetPhysicalDeviceImageFormatProperties2(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo,
    VkImageFormatProperties2* pImageFormatProperties) {
  auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceImageFormatProperties2);
  if (host == nullptr) host = ELIXIR_VK_HOST(vkGetPhysicalDeviceImageFormatProperties2KHR);
  if (host == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  return vk::GetImageFormatProperties2(host, physicalDevice, pImageFormatInfo, pImageFormatProperties);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkGetPhysicalDeviceImageFormatProperties2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo,
    VkImageFormatProperties2* pImageFormatProperties) {
  auto host = ELIXIR_VK_HOST(vkGetPhysicalDeviceImageFormatProperties2KHR);
  if (host == nullptr) host = ELIXIR_VK_HOST(vkGetPhysicalDeviceImageFormatProperties2);
  if (host == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
  return vk::GetImageFormatProperties2(host, physicalDevice, pImageFormatInfo, pImageFormatProperties);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo,
                                                       const VkAllocationCallbacks* ,
                                                       VkImage* pImage) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && pCreateInfo != nullptr && vk::LookupEmulatedEtc2Format(pCreateInfo->format, nullptr) &&
      etc2->Emulates(device)) {
    vk::EmulatedImageInfo info;
    vk::PrepareEmulatedImageInfo(*pCreateInfo, &info);
    return etc2->CreateImage(device, &info.create_info, nullptr, pImage);
  }
  return ELIXIR_VK_HOST(vkCreateImage)(device, pCreateInfo, nullptr, pImage);
}

ELIXIR_SYSV void elixir_vk_intercept_vkDestroyImage(VkDevice device, VkImage image,
                                                    const VkAllocationCallbacks* ) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) {
    etc2->DestroyImage(device, image, nullptr);
    return;
  }
  ELIXIR_VK_HOST(vkDestroyImage)(device, image, nullptr);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo,
                                                           const VkAllocationCallbacks* ,
                                                           VkImageView* pView) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && pCreateInfo != nullptr && vk::LookupEmulatedEtc2Format(pCreateInfo->format, nullptr) &&
      etc2->Emulates(device)) {
    return etc2->CreateImageView(device, pCreateInfo, nullptr, pView);
  }
  return ELIXIR_VK_HOST(vkCreateImageView)(device, pCreateInfo, nullptr, pView);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
                                                            VkDeviceSize memoryOffset) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) return etc2->BindBufferMemory(device, buffer, memory, memoryOffset);
  return ELIXIR_VK_HOST(vkBindBufferMemory)(device, buffer, memory, memoryOffset);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount,
                                                             const VkBindBufferMemoryInfo* pBindInfos) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) return etc2->BindBufferMemory2(device, bindInfoCount, pBindInfos);
  return ELIXIR_VK_HOST(vkBindBufferMemory2)(device, bindInfoCount, pBindInfos);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount,
                                                                const VkBindBufferMemoryInfo* pBindInfos) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) return etc2->BindBufferMemory2(device, bindInfoCount, pBindInfos);
  return ELIXIR_VK_HOST(vkBindBufferMemory2KHR)(device, bindInfoCount, pBindInfos);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                                                     VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) return etc2->MapMemory(device, memory, offset, size, flags, ppData);
  return ELIXIR_VK_HOST(vkMapMemory)(device, memory, offset, size, flags, ppData);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkMapMemory2(VkDevice device, const VkMemoryMapInfo* pMemoryMapInfo,
                                                      void** ppData) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) return etc2->MapMemory2(device, pMemoryMapInfo, ppData);
  return ELIXIR_VK_HOST(vkMapMemory2)(device, pMemoryMapInfo, ppData);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkMapMemory2KHR(VkDevice device, const VkMemoryMapInfo* pMemoryMapInfo,
                                                         void** ppData) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) return etc2->MapMemory2(device, pMemoryMapInfo, ppData);
  return ELIXIR_VK_HOST(vkMapMemory2KHR)(device, pMemoryMapInfo, ppData);
}

ELIXIR_SYSV void elixir_vk_intercept_vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) {
    etc2->UnmapMemory(device, memory);
    return;
  }
  ELIXIR_VK_HOST(vkUnmapMemory)(device, memory);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkUnmapMemory2(VkDevice device, const VkMemoryUnmapInfo* pMemoryUnmapInfo) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) return etc2->UnmapMemory2(device, pMemoryUnmapInfo);
  return ELIXIR_VK_HOST(vkUnmapMemory2)(device, pMemoryUnmapInfo);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkUnmapMemory2KHR(VkDevice device,
                                                           const VkMemoryUnmapInfo* pMemoryUnmapInfo) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) return etc2->UnmapMemory2(device, pMemoryUnmapInfo);
  return ELIXIR_VK_HOST(vkUnmapMemory2KHR)(device, pMemoryUnmapInfo);
}

ELIXIR_SYSV void elixir_vk_intercept_vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                                                  const VkAllocationCallbacks* ) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) {
    etc2->FreeMemory(device, memory, nullptr);
    return;
  }
  ELIXIR_VK_HOST(vkFreeMemory)(device, memory, nullptr);
}

ELIXIR_SYSV void elixir_vk_intercept_vkDestroyBuffer(VkDevice device, VkBuffer buffer,
                                                     const VkAllocationCallbacks* ) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->Emulates(device)) {
    etc2->DestroyBuffer(device, buffer, nullptr);
    return;
  }
  ELIXIR_VK_HOST(vkDestroyBuffer)(device, buffer, nullptr);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
                                                            VkImage dstImage, VkImageLayout dstImageLayout,
                                                            uint32_t regionCount, const VkBufferImageCopy* pRegions) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr &&
      etc2->CmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions)) {
    return;
  }
  ELIXIR_VK_HOST(vkCmdCopyBufferToImage)(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer,
                                                             const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->CmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo)) return;
  ELIXIR_VK_HOST(vkCmdCopyBufferToImage2)(commandBuffer, pCopyBufferToImageInfo);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdCopyBufferToImage2KHR(
    VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->CmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo)) return;
  ELIXIR_VK_HOST(vkCmdCopyBufferToImage2KHR)(commandBuffer, pCopyBufferToImageInfo);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                                                    VkImageLayout srcImageLayout, VkImage dstImage,
                                                    VkImageLayout dstImageLayout, uint32_t regionCount,
                                                    const VkImageCopy* pRegions) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->CmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout,
                                         regionCount, pRegions)) {
    return;
  }
  ELIXIR_VK_HOST(vkCmdCopyImage)(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount,
                                 pRegions);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdCopyImage2(VkCommandBuffer commandBuffer,
                                                     const VkCopyImageInfo2* pCopyImageInfo) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->CmdCopyImage2(commandBuffer, pCopyImageInfo)) return;
  ELIXIR_VK_HOST(vkCmdCopyImage2)(commandBuffer, pCopyImageInfo);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdCopyImage2KHR(VkCommandBuffer commandBuffer,
                                                        const VkCopyImageInfo2* pCopyImageInfo) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->CmdCopyImage2(commandBuffer, pCopyImageInfo)) return;
  ELIXIR_VK_HOST(vkCmdCopyImage2KHR)(commandBuffer, pCopyImageInfo);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage,
                                                            VkImageLayout srcImageLayout, VkBuffer dstBuffer,
                                                            uint32_t regionCount, const VkBufferImageCopy* pRegions) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && etc2->DropImageToBufferCopy(srcImage)) return;
  ELIXIR_VK_HOST(vkCmdCopyImageToBuffer)(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer,
                                                             const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && pCopyImageToBufferInfo != nullptr &&
      etc2->DropImageToBufferCopy(pCopyImageToBufferInfo->srcImage)) {
    return;
  }
  ELIXIR_VK_HOST(vkCmdCopyImageToBuffer2)(commandBuffer, pCopyImageToBufferInfo);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdCopyImageToBuffer2KHR(
    VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr && pCopyImageToBufferInfo != nullptr &&
      etc2->DropImageToBufferCopy(pCopyImageToBufferInfo->srcImage)) {
    return;
  }
  ELIXIR_VK_HOST(vkCmdCopyImageToBuffer2KHR)(commandBuffer, pCopyImageToBufferInfo);
}

ELIXIR_SYSV void elixir_vk_intercept_vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount,
                                                          const VkCommandBuffer* pCommandBuffers) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr) etc2->CmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
  ELIXIR_VK_HOST(vkCmdExecuteCommands)(commandBuffer, commandBufferCount, pCommandBuffers);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkAllocateCommandBuffers(VkDevice device,
                                                                  const VkCommandBufferAllocateInfo* pAllocateInfo,
                                                                  VkCommandBuffer* pCommandBuffers) {
  const VkResult result = ELIXIR_VK_HOST(vkAllocateCommandBuffers)(device, pAllocateInfo, pCommandBuffers);
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (result == VK_SUCCESS && pAllocateInfo != nullptr && etc2 != nullptr) {
    etc2->AllocateCommandBuffers(device, pAllocateInfo->commandPool, pAllocateInfo->commandBufferCount,
                                pCommandBuffers);
  }
  return result;
}

ELIXIR_SYSV void elixir_vk_intercept_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
                                                          uint32_t commandBufferCount,
                                                          const VkCommandBuffer* pCommandBuffers) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr) etc2->FreeCommandBuffers(commandBufferCount, pCommandBuffers);
  ELIXIR_VK_HOST(vkFreeCommandBuffers)(device, commandPool, commandBufferCount, pCommandBuffers);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkResetCommandPool(VkDevice device, VkCommandPool commandPool,
                                                            VkCommandPoolResetFlags flags) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr) etc2->ReleaseCommandPool(device, commandPool, false);
  return ELIXIR_VK_HOST(vkResetCommandPool)(device, commandPool, flags);
}

ELIXIR_SYSV void elixir_vk_intercept_vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool,
                                                          const VkAllocationCallbacks* ) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr) etc2->ReleaseCommandPool(device, commandPool, true);
  ELIXIR_VK_HOST(vkDestroyCommandPool)(device, commandPool, nullptr);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                              const VkCommandBufferBeginInfo* pBeginInfo) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr) etc2->ReleaseCommandBuffer(commandBuffer);
  return ELIXIR_VK_HOST(vkBeginCommandBuffer)(commandBuffer, pBeginInfo);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkResetCommandBuffer(VkCommandBuffer commandBuffer,
                                                              VkCommandBufferResetFlags flags) {
  vk::VulkanEtc2Emulation* etc2 = vk::ActiveEtc2();
  if (etc2 != nullptr) etc2->ReleaseCommandBuffer(commandBuffer);
  return ELIXIR_VK_HOST(vkResetCommandBuffer)(commandBuffer, flags);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkQueueSubmit(VkQueue queue, uint32_t submitCount,
                                                       const VkSubmitInfo* pSubmits, VkFence fence) {
  vk::PrepareEtc2Submit(submitCount, pSubmits);
  const elixir::platform::GuestThreadResume resume;
  return ELIXIR_VK_HOST(vkQueueSubmit)(queue, submitCount, pSubmits, fence);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkQueueSubmit2(VkQueue queue, uint32_t submitCount,
                                                        const VkSubmitInfo2* pSubmits, VkFence fence) {
  vk::PrepareEtc2Submit2(submitCount, pSubmits);
  const elixir::platform::GuestThreadResume resume;
  return ELIXIR_VK_HOST(vkQueueSubmit2)(queue, submitCount, pSubmits, fence);
}

ELIXIR_SYSV VkResult elixir_vk_intercept_vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount,
                                                           const VkSubmitInfo2* pSubmits, VkFence fence) {
  vk::PrepareEtc2Submit2(submitCount, pSubmits);
  const elixir::platform::GuestThreadResume resume;
  return ELIXIR_VK_HOST(vkQueueSubmit2KHR)(queue, submitCount, pSubmits, fence);
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetDeviceImageMemoryRequirements(
    VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  const auto host = ELIXIR_VK_HOST(vkGetDeviceImageMemoryRequirements);
  vk::EmulatedImageInfo prepared;
  VkImageCreateInfo image{};
  if (pInfo != nullptr && vk::HostQueryImageInfo(device, pInfo->pCreateInfo, &prepared, &image)) {
    VkDeviceImageMemoryRequirements info = *pInfo;
    info.pCreateInfo = &image;
    host(device, &info, pMemoryRequirements);
    return;
  }
  host(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetDeviceImageMemoryRequirementsKHR(
    VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  const auto host = ELIXIR_VK_HOST(vkGetDeviceImageMemoryRequirementsKHR);
  vk::EmulatedImageInfo prepared;
  VkImageCreateInfo image{};
  if (pInfo != nullptr && vk::HostQueryImageInfo(device, pInfo->pCreateInfo, &prepared, &image)) {
    VkDeviceImageMemoryRequirements info = *pInfo;
    info.pCreateInfo = &image;
    host(device, &info, pMemoryRequirements);
    return;
  }
  host(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetDeviceImageSparseMemoryRequirements(
    VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
  const auto host = ELIXIR_VK_HOST(vkGetDeviceImageSparseMemoryRequirements);
  vk::EmulatedImageInfo prepared;
  VkImageCreateInfo image{};
  if (pInfo != nullptr && vk::HostQueryImageInfo(device, pInfo->pCreateInfo, &prepared, &image)) {
    VkDeviceImageMemoryRequirements info = *pInfo;
    info.pCreateInfo = &image;
    host(device, &info, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    return;
  }
  host(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetDeviceImageSparseMemoryRequirementsKHR(
    VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
  const auto host = ELIXIR_VK_HOST(vkGetDeviceImageSparseMemoryRequirementsKHR);
  vk::EmulatedImageInfo prepared;
  VkImageCreateInfo image{};
  if (pInfo != nullptr && vk::HostQueryImageInfo(device, pInfo->pCreateInfo, &prepared, &image)) {
    VkDeviceImageMemoryRequirements info = *pInfo;
    info.pCreateInfo = &image;
    host(device, &info, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    return;
  }
  host(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetDeviceImageSubresourceLayout(VkDevice device,
                                                                       const VkDeviceImageSubresourceInfo* pInfo,
                                                                       VkSubresourceLayout2* pLayout) {
  const auto host = ELIXIR_VK_HOST(vkGetDeviceImageSubresourceLayout);
  vk::EmulatedImageInfo prepared;
  VkImageCreateInfo image{};
  if (pInfo != nullptr && vk::HostQueryImageInfo(device, pInfo->pCreateInfo, &prepared, &image)) {
    VkDeviceImageSubresourceInfo info = *pInfo;
    info.pCreateInfo = &image;
    host(device, &info, pLayout);
    return;
  }
  host(device, pInfo, pLayout);
}

ELIXIR_SYSV void elixir_vk_intercept_vkGetDeviceImageSubresourceLayoutKHR(VkDevice device,
                                                                          const VkDeviceImageSubresourceInfo* pInfo,
                                                                          VkSubresourceLayout2* pLayout) {
  const auto host = ELIXIR_VK_HOST(vkGetDeviceImageSubresourceLayoutKHR);
  vk::EmulatedImageInfo prepared;
  VkImageCreateInfo image{};
  if (pInfo != nullptr && vk::HostQueryImageInfo(device, pInfo->pCreateInfo, &prepared, &image)) {
    VkDeviceImageSubresourceInfo info = *pInfo;
    info.pCreateInfo = &image;
    host(device, &info, pLayout);
    return;
  }
  host(device, pInfo, pLayout);
}

}
