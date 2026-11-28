// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "graphics/vulkan/frame_chain.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include "graphics/vulkan/present_scaling.h"

namespace elixir::vulkan {

std::uint32_t g_live_frame_chains = 0;

namespace {

#define ELIXIR_CHAIN_FUNCTIONS(X)                                                        \
  X(create_image, PFN_vkCreateImage, "vkCreateImage")                                    \
  X(destroy_image, PFN_vkDestroyImage, "vkDestroyImage")                                 \
  X(image_requirements, PFN_vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements") \
  X(allocate_memory, PFN_vkAllocateMemory, "vkAllocateMemory")                           \
  X(free_memory, PFN_vkFreeMemory, "vkFreeMemory")                                       \
  X(bind_image_memory, PFN_vkBindImageMemory, "vkBindImageMemory")                       \
  X(create_image_view, PFN_vkCreateImageView, "vkCreateImageView")                       \
  X(destroy_image_view, PFN_vkDestroyImageView, "vkDestroyImageView")                    \
  X(create_sampler, PFN_vkCreateSampler, "vkCreateSampler")                              \
  X(destroy_sampler, PFN_vkDestroySampler, "vkDestroySampler")                           \
  X(create_render_pass, PFN_vkCreateRenderPass, "vkCreateRenderPass")                    \
  X(destroy_render_pass, PFN_vkDestroyRenderPass, "vkDestroyRenderPass")                 \
  X(create_framebuffer, PFN_vkCreateFramebuffer, "vkCreateFramebuffer")                  \
  X(destroy_framebuffer, PFN_vkDestroyFramebuffer, "vkDestroyFramebuffer")               \
  X(create_shader_module, PFN_vkCreateShaderModule, "vkCreateShaderModule")              \
  X(destroy_shader_module, PFN_vkDestroyShaderModule, "vkDestroyShaderModule")           \
  X(create_pipeline_layout, PFN_vkCreatePipelineLayout, "vkCreatePipelineLayout")        \
  X(destroy_pipeline_layout, PFN_vkDestroyPipelineLayout, "vkDestroyPipelineLayout")     \
  X(create_pipelines, PFN_vkCreateGraphicsPipelines, "vkCreateGraphicsPipelines")        \
  X(destroy_pipeline, PFN_vkDestroyPipeline, "vkDestroyPipeline")                        \
  X(create_set_layout, PFN_vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout")   \
  X(destroy_set_layout, PFN_vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout") \
  X(create_descriptor_pool, PFN_vkCreateDescriptorPool, "vkCreateDescriptorPool")        \
  X(destroy_descriptor_pool, PFN_vkDestroyDescriptorPool, "vkDestroyDescriptorPool")     \
  X(allocate_sets, PFN_vkAllocateDescriptorSets, "vkAllocateDescriptorSets")             \
  X(update_sets, PFN_vkUpdateDescriptorSets, "vkUpdateDescriptorSets")                   \
  X(create_buffer, PFN_vkCreateBuffer, "vkCreateBuffer")                                 \
  X(destroy_buffer, PFN_vkDestroyBuffer, "vkDestroyBuffer")                              \
  X(buffer_requirements, PFN_vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements") \
  X(bind_buffer_memory, PFN_vkBindBufferMemory, "vkBindBufferMemory")                    \
  X(map_memory, PFN_vkMapMemory, "vkMapMemory")                                          \
  X(create_command_pool, PFN_vkCreateCommandPool, "vkCreateCommandPool")                 \
  X(destroy_command_pool, PFN_vkDestroyCommandPool, "vkDestroyCommandPool")              \
  X(allocate_commands, PFN_vkAllocateCommandBuffers, "vkAllocateCommandBuffers")         \
  X(begin_commands, PFN_vkBeginCommandBuffer, "vkBeginCommandBuffer")                    \
  X(end_commands, PFN_vkEndCommandBuffer, "vkEndCommandBuffer")                          \
  X(reset_commands, PFN_vkResetCommandBuffer, "vkResetCommandBuffer")                    \
  X(barrier, PFN_vkCmdPipelineBarrier, "vkCmdPipelineBarrier")                           \
  X(begin_render_pass, PFN_vkCmdBeginRenderPass, "vkCmdBeginRenderPass")                 \
  X(end_render_pass, PFN_vkCmdEndRenderPass, "vkCmdEndRenderPass")                       \
  X(bind_pipeline, PFN_vkCmdBindPipeline, "vkCmdBindPipeline")                           \
  X(bind_sets, PFN_vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets")                   \
  X(set_viewport, PFN_vkCmdSetViewport, "vkCmdSetViewport")                              \
  X(set_scissor, PFN_vkCmdSetScissor, "vkCmdSetScissor")                                 \
  X(draw, PFN_vkCmdDraw, "vkCmdDraw")                                                    \
  X(blit, PFN_vkCmdBlitImage, "vkCmdBlitImage")                                          \
  X(create_semaphore, PFN_vkCreateSemaphore, "vkCreateSemaphore")                        \
  X(destroy_semaphore, PFN_vkDestroySemaphore, "vkDestroySemaphore")                     \
  X(create_fence, PFN_vkCreateFence, "vkCreateFence")                                    \
  X(destroy_fence, PFN_vkDestroyFence, "vkDestroyFence")                                 \
  X(wait_fences, PFN_vkWaitForFences, "vkWaitForFences")                                 \
  X(reset_fences, PFN_vkResetFences, "vkResetFences")                                    \
  X(submit, PFN_vkQueueSubmit, "vkQueueSubmit")                                          \
  X(swapchain_images, PFN_vkGetSwapchainImagesKHR, "vkGetSwapchainImagesKHR")            \
  X(destroy_swapchain, PFN_vkDestroySwapchainKHR, "vkDestroySwapchainKHR")               \
  X(create_query_pool, PFN_vkCreateQueryPool, "vkCreateQueryPool")                       \
  X(destroy_query_pool, PFN_vkDestroyQueryPool, "vkDestroyQueryPool")                    \
  X(reset_queries, PFN_vkCmdResetQueryPool, "vkCmdResetQueryPool")                       \
  X(write_timestamp, PFN_vkCmdWriteTimestamp, "vkCmdWriteTimestamp")                     \
  X(query_results, PFN_vkGetQueryPoolResults, "vkGetQueryPoolResults")

void Log(const char* format, ...) {
  char text[512];
  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(text, sizeof(text), format, arguments);
  va_end(arguments);
  std::fprintf(stderr, "[vulkan] %s\n", text);
}

struct HostDevice {
  VkDevice device = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkPhysicalDeviceMemoryProperties memory{};
  float timestamp_period = 0.0f;
  std::unordered_map<VkQueue, std::uint32_t> queues;
#define ELIXIR_CHAIN_MEMBER(member, type, name) type member = nullptr;
  ELIXIR_CHAIN_FUNCTIONS(ELIXIR_CHAIN_MEMBER)
#undef ELIXIR_CHAIN_MEMBER

  bool complete() const {
#define ELIXIR_CHAIN_CHECK(member, type, name) \
  if (member == nullptr) return false;
    ELIXIR_CHAIN_FUNCTIONS(ELIXIR_CHAIN_CHECK)
#undef ELIXIR_CHAIN_CHECK
    return true;
  }
};

struct Target {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  VkExtent2D extent{};
};

struct BuiltPass {
  const ChainPass* spec = nullptr;
  VkShaderModule module = VK_NULL_HANDLE;
  VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> sets;
  int target = -1;
  VkExtent2D extent{};
  VkExtent2D source_extent{};
  VkExtent2D original_extent{};
  std::uint32_t block_offset = 0;
};

struct Chain {
  HostDevice* host = nullptr;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkExtent2D render_extent{};
  VkExtent2D window_extent{};
  std::vector<VkImage> real_images;
  std::vector<VkImageView> real_views;
  std::vector<VkFramebuffer> real_framebuffers;
  std::vector<VkImage> proxy_images;
  std::vector<VkDeviceMemory> proxy_memory;
  std::vector<VkImageView> proxy_views;
  std::vector<VkSemaphore> done;
  VkCommandPool pool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commands;
  VkFence fence = VK_NULL_HANDLE;
  bool fence_pending = false;
  std::uint32_t family = UINT32_MAX;
  VkQueryPool queries = VK_NULL_HANDLE;
  std::vector<bool> timed;

  std::shared_ptr<const FrameChainSpec> spec;
  bool built = false;
  bool broken = false;
  VkRenderPass final_pass = VK_NULL_HANDLE;
  VkRenderPass rgba8_pass = VK_NULL_HANDLE;
  VkRenderPass float_pass = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VkShaderModule triangle = VK_NULL_HANDLE;
  VkDescriptorPool descriptors = VK_NULL_HANDLE;
  std::vector<Target> targets;
  std::vector<BuiltPass> passes;
  std::vector<VkBuffer> blocks;
  std::vector<VkDeviceMemory> block_memory;
  std::vector<void*> block_map;
  std::uint32_t block_bytes = 0;

  std::uint64_t frame = 0;
  LARGE_INTEGER started{};
  LARGE_INTEGER previous{};
  double delta_seconds = 0.0;
};

struct State {
  std::mutex mutex;
  std::unordered_map<VkDevice, std::unique_ptr<HostDevice>> devices;
  std::unordered_map<VkSwapchainKHR, std::unique_ptr<Chain>> chains;
  std::shared_ptr<const FrameChainSpec> spec;
  FrameChainStatus status;
};

State& GetState() {
  static State* state = new State;
  return *state;
}

std::atomic<bool> g_rebuild_requested{false};
std::atomic<bool> g_timing{false};

constexpr VkFormat kRgba8 = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat kRgba16f = VK_FORMAT_R16G16B16A16_SFLOAT;

bool MemoryType(const VkPhysicalDeviceMemoryProperties& memory, std::uint32_t bits, VkMemoryPropertyFlags wanted,
                std::uint32_t* index) {
  for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
    if ((bits & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & wanted) == wanted) {
      *index = i;
      return true;
    }
  }
  return false;
}

bool CreateImage(HostDevice& host, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkImage* image,
                 VkDeviceMemory* memory) {
  VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = format;
  info.extent = VkExtent3D{extent.width, extent.height, 1};
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usage;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (host.create_image(host.device, &info, nullptr, image) != VK_SUCCESS) return false;
  VkMemoryRequirements requirements{};
  host.image_requirements(host.device, *image, &requirements);
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocation.allocationSize = requirements.size;
  if (!MemoryType(host.memory, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  &allocation.memoryTypeIndex) &&
      !MemoryType(host.memory, requirements.memoryTypeBits, 0, &allocation.memoryTypeIndex)) {
    return false;
  }
  if (host.allocate_memory(host.device, &allocation, nullptr, memory) != VK_SUCCESS) return false;
  return host.bind_image_memory(host.device, *image, *memory, 0) == VK_SUCCESS;
}

bool CreateView(HostDevice& host, VkImage image, VkFormat format, VkImageView* view) {
  VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  info.image = image;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = format;
  info.subresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  return host.create_image_view(host.device, &info, nullptr, view) == VK_SUCCESS;
}

bool CreateRenderPass(HostDevice& host, VkFormat format, VkImageLayout final_layout, VkRenderPass* pass) {
  VkAttachmentDescription attachment{};
  attachment.format = format;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = final_layout;
  const VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color;
  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  info.attachmentCount = 1;
  info.pAttachments = &attachment;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 2;
  info.pDependencies = dependencies;
  return host.create_render_pass(host.device, &info, nullptr, pass) == VK_SUCCESS;
}

bool CreateFramebuffer(HostDevice& host, VkRenderPass pass, VkImageView view, VkExtent2D extent,
                       VkFramebuffer* framebuffer) {
  VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  info.renderPass = pass;
  info.attachmentCount = 1;
  info.pAttachments = &view;
  info.width = extent.width;
  info.height = extent.height;
  info.layers = 1;
  return host.create_framebuffer(host.device, &info, nullptr, framebuffer) == VK_SUCCESS;
}

VkExtent2D Scaled(VkExtent2D extent, float scale) {
  return VkExtent2D{std::max(1u, static_cast<std::uint32_t>(std::lround(extent.width * scale))),
                    std::max(1u, static_cast<std::uint32_t>(std::lround(extent.height * scale)))};
}

bool SameExtent(VkExtent2D a, VkExtent2D b) { return a.width == b.width && a.height == b.height; }

void WaitForLastFrame(Chain& chain) {
  if (!chain.fence_pending || chain.fence == VK_NULL_HANDLE) return;
  chain.host->wait_fences(chain.host->device, 1, &chain.fence, VK_TRUE, 2000000000ull);
  chain.fence_pending = false;
}

void DestroyBuilt(Chain& chain) {
  HostDevice& host = *chain.host;
  WaitForLastFrame(chain);
  for (BuiltPass& pass : chain.passes) {
    if (pass.pipeline != VK_NULL_HANDLE) host.destroy_pipeline(host.device, pass.pipeline, nullptr);
    if (pass.layout != VK_NULL_HANDLE) host.destroy_pipeline_layout(host.device, pass.layout, nullptr);
    if (pass.set_layout != VK_NULL_HANDLE) host.destroy_set_layout(host.device, pass.set_layout, nullptr);
    if (pass.module != VK_NULL_HANDLE) host.destroy_shader_module(host.device, pass.module, nullptr);
  }
  chain.passes.clear();
  for (Target& target : chain.targets) {
    if (target.framebuffer != VK_NULL_HANDLE) host.destroy_framebuffer(host.device, target.framebuffer, nullptr);
    if (target.view != VK_NULL_HANDLE) host.destroy_image_view(host.device, target.view, nullptr);
    if (target.image != VK_NULL_HANDLE) host.destroy_image(host.device, target.image, nullptr);
    if (target.memory != VK_NULL_HANDLE) host.free_memory(host.device, target.memory, nullptr);
  }
  chain.targets.clear();
  if (chain.descriptors != VK_NULL_HANDLE) host.destroy_descriptor_pool(host.device, chain.descriptors, nullptr);
  chain.descriptors = VK_NULL_HANDLE;
  for (std::size_t i = 0; i < chain.blocks.size(); ++i) {
    if (chain.blocks[i] != VK_NULL_HANDLE) host.destroy_buffer(host.device, chain.blocks[i], nullptr);
    if (chain.block_memory[i] != VK_NULL_HANDLE) host.free_memory(host.device, chain.block_memory[i], nullptr);
  }
  chain.blocks.clear();
  chain.block_memory.clear();
  chain.block_map.clear();
  chain.block_bytes = 0;
  if (chain.triangle != VK_NULL_HANDLE) host.destroy_shader_module(host.device, chain.triangle, nullptr);
  chain.triangle = VK_NULL_HANDLE;
  chain.built = false;
}

void DestroyChain(Chain& chain) {
  HostDevice& host = *chain.host;
  DestroyBuilt(chain);
  for (VkFramebuffer framebuffer : chain.real_framebuffers) {
    if (framebuffer != VK_NULL_HANDLE) host.destroy_framebuffer(host.device, framebuffer, nullptr);
  }
  for (VkImageView view : chain.real_views) {
    if (view != VK_NULL_HANDLE) host.destroy_image_view(host.device, view, nullptr);
  }
  for (VkImageView view : chain.proxy_views) {
    if (view != VK_NULL_HANDLE) host.destroy_image_view(host.device, view, nullptr);
  }
  for (VkImage image : chain.proxy_images) {
    if (image != VK_NULL_HANDLE) host.destroy_image(host.device, image, nullptr);
  }
  for (VkDeviceMemory memory : chain.proxy_memory) {
    if (memory != VK_NULL_HANDLE) host.free_memory(host.device, memory, nullptr);
  }
  for (VkSemaphore semaphore : chain.done) {
    if (semaphore != VK_NULL_HANDLE) host.destroy_semaphore(host.device, semaphore, nullptr);
  }
  if (chain.pool != VK_NULL_HANDLE) host.destroy_command_pool(host.device, chain.pool, nullptr);
  if (chain.queries != VK_NULL_HANDLE) host.destroy_query_pool(host.device, chain.queries, nullptr);
  if (chain.fence != VK_NULL_HANDLE) host.destroy_fence(host.device, chain.fence, nullptr);
  if (chain.sampler != VK_NULL_HANDLE) host.destroy_sampler(host.device, chain.sampler, nullptr);
  if (chain.final_pass != VK_NULL_HANDLE) host.destroy_render_pass(host.device, chain.final_pass, nullptr);
  if (chain.rgba8_pass != VK_NULL_HANDLE) host.destroy_render_pass(host.device, chain.rgba8_pass, nullptr);
  if (chain.float_pass != VK_NULL_HANDLE) host.destroy_render_pass(host.device, chain.float_pass, nullptr);
}

bool Problem(State& state, Chain& chain, const std::string& text) {
  Log("shaders: %s", text.c_str());
  state.status.last_problem = text;
  ++state.status.failures;
  chain.broken = true;
  return false;
}

VkImageView ViewOf(const Chain& chain, int target, std::size_t image) {
  return target < 0 ? chain.proxy_views[image] : chain.targets[static_cast<std::size_t>(target)].view;
}

bool BuildPasses(State& state, Chain& chain) {
  HostDevice& host = *chain.host;
  const FrameChainSpec& spec = *chain.spec;
  const bool scaled = !SameExtent(chain.render_extent, chain.window_extent);
  std::vector<const ChainPass*> wanted;
  for (const ChainPass& pass : spec.passes) {
    if (pass.scaler && !scaled) continue;
    if (pass.fragment.empty()) continue;
    wanted.push_back(&pass);
  }
  if (wanted.empty()) return true;
  if (spec.triangle.empty()) return Problem(state, chain, "the built-in vertex shader is missing");

  VkShaderModuleCreateInfo triangle{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  triangle.codeSize = spec.triangle.size() * sizeof(std::uint32_t);
  triangle.pCode = spec.triangle.data();
  if (host.create_shader_module(host.device, &triangle, nullptr, &chain.triangle) != VK_SUCCESS) {
    return Problem(state, chain, "the built-in vertex shader was refused by the graphics driver");
  }

  const int count = static_cast<int>(wanted.size());
  std::vector<int> source_of(wanted.size(), -1);
  std::vector<int> original_of(wanted.size(), -1);
  std::vector<std::vector<int>> reads(wanted.size());
  std::vector<int> last_use(wanted.size(), -1);
  {
    int group = wanted.front()->group;
    int group_first = 0;
    int group_input = -1;
    for (int index = 0; index < count; ++index) {
      const ChainPass& pass_spec = *wanted[static_cast<std::size_t>(index)];
      if (pass_spec.group != group) {
        group = pass_spec.group;
        group_first = index;
        group_input = index - 1;
      }
      source_of[static_cast<std::size_t>(index)] = index - 1;
      original_of[static_cast<std::size_t>(index)] = group_input;
      for (const ChainSampler& sampler : pass_spec.samplers) {
        int from = -1;
        bool known = true;
        if (sampler.name == "source") {
          from = index - 1;
        } else if (sampler.name == "original") {
          from = group_input;
        } else if (sampler.name == "game") {
          from = -1;
        } else if (sampler.name.size() > 4 && sampler.name.compare(0, 4, "pass") == 0) {
          const int number = std::atoi(sampler.name.c_str() + 4);
          known = number >= 1 && group_first + number - 1 < index;
          if (known) from = group_first + number - 1;
        } else {
          known = false;
        }
        if (!known) {
          return Problem(state, chain,
                         pass_spec.label + ": no picture is called " + sampler.name +
                             " (use source, original, game or pass1, pass2 of the same shader)");
        }
        reads[static_cast<std::size_t>(index)].push_back(from);
        if (from >= 0) last_use[static_cast<std::size_t>(from)] = std::max(last_use[static_cast<std::size_t>(from)], index);
      }
    }
  }

  struct Slot {
    VkFormat format = VK_FORMAT_UNDEFINED;
    int owner = -1;
  };
  std::vector<Slot> slots;
  std::vector<VkExtent2D> extent_of(wanted.size());
  bool reached_window = !scaled;
  std::uint32_t block_bytes = 0;
  std::uint32_t sampler_count = 0;
  for (int index = 0; index < count; ++index) {
    const std::size_t at = static_cast<std::size_t>(index);
    const ChainPass& pass_spec = *wanted[at];
    const bool last = index + 1 == count;
    BuiltPass pass;
    pass.spec = &pass_spec;
    const auto extent_from = [&](int from) {
      return from < 0 ? chain.render_extent : extent_of[static_cast<std::size_t>(from)];
    };
    pass.source_extent = extent_from(source_of[at]);
    pass.original_extent = extent_from(original_of[at]);
    if (pass_spec.scaler) reached_window = true;
    const VkExtent2D base = reached_window || last ? chain.window_extent : chain.render_extent;
    pass.extent = last ? chain.window_extent : Scaled(base, pass_spec.scale);
    extent_of[at] = pass.extent;
    if (!last) {
      const VkFormat format = pass_spec.float_target ? kRgba16f : kRgba8;
      int chosen = -1;
      for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        const int owner = slots[slot].owner;
        const bool unused = owner < 0 || std::max(last_use[static_cast<std::size_t>(owner)], owner) < index;
        if (unused && slots[slot].format == format && SameExtent(chain.targets[slot].extent, pass.extent)) {
          chosen = static_cast<int>(slot);
          break;
        }
      }
      if (chosen < 0) {
        Target target;
        target.extent = pass.extent;
        const VkRenderPass render_pass = pass_spec.float_target ? chain.float_pass : chain.rgba8_pass;
        const bool made = CreateImage(host, format, target.extent,
                                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                      &target.image, &target.memory) &&
                          CreateView(host, target.image, format, &target.view) &&
                          CreateFramebuffer(host, render_pass, target.view, target.extent, &target.framebuffer);
        chain.targets.push_back(target);
        slots.push_back(Slot{format, -1});
        if (!made) return Problem(state, chain, pass_spec.label + ": a render target could not be made");
        chosen = static_cast<int>(slots.size()) - 1;
      }
      slots[static_cast<std::size_t>(chosen)].owner = index;
      pass.target = chosen;
    }

    VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    module.codeSize = pass_spec.fragment.size() * sizeof(std::uint32_t);
    module.pCode = pass_spec.fragment.data();
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    if (pass_spec.has_block) {
      bindings.push_back(VkDescriptorSetLayoutBinding{pass_spec.block_binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
      pass.block_offset = block_bytes;
      block_bytes += (pass_spec.block_size + 255u) & ~255u;
    }
    for (const ChainSampler& sampler : pass_spec.samplers) {
      bindings.push_back(VkDescriptorSetLayoutBinding{sampler.binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
      ++sampler_count;
    }
    VkDescriptorSetLayoutCreateInfo set_layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    set_layout.bindingCount = static_cast<std::uint32_t>(bindings.size());
    set_layout.pBindings = bindings.data();
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout.setLayoutCount = 1;
    const bool shaped =
        host.create_shader_module(host.device, &module, nullptr, &pass.module) == VK_SUCCESS &&
        host.create_set_layout(host.device, &set_layout, nullptr, &pass.set_layout) == VK_SUCCESS &&
        (layout.pSetLayouts = &pass.set_layout,
         host.create_pipeline_layout(host.device, &layout, nullptr, &pass.layout) == VK_SUCCESS);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = chain.triangle;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = pass.module;
    stages[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo vertex_input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_attachment;
    const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;
    VkGraphicsPipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipeline.stageCount = 2;
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &vertex_input;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &multisample;
    pipeline.pColorBlendState = &blend;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = pass.layout;
    pipeline.renderPass = last ? chain.final_pass : pass_spec.float_target ? chain.float_pass : chain.rgba8_pass;
    const bool piped = shaped && host.create_pipelines(host.device, VK_NULL_HANDLE, 1, &pipeline, nullptr,
                                                       &pass.pipeline) == VK_SUCCESS;
    chain.passes.push_back(std::move(pass));
    if (!piped) return Problem(state, chain, pass_spec.label + ": the graphics driver refused the shader");
  }

  const std::size_t images = chain.real_images.size();
  chain.block_bytes = std::max(block_bytes, 256u);
  for (std::size_t image = 0; image < images; ++image) {
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size = chain.block_bytes;
    buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    bool made = host.create_buffer(host.device, &buffer, nullptr, &handle) == VK_SUCCESS;
    if (made) {
      VkMemoryRequirements requirements{};
      host.buffer_requirements(host.device, handle, &requirements);
      VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
      allocation.allocationSize = requirements.size;
      made = MemoryType(host.memory, requirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        &allocation.memoryTypeIndex) &&
             host.allocate_memory(host.device, &allocation, nullptr, &memory) == VK_SUCCESS &&
             host.bind_buffer_memory(host.device, handle, memory, 0) == VK_SUCCESS &&
             host.map_memory(host.device, memory, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS;
    }
    chain.blocks.push_back(handle);
    chain.block_memory.push_back(memory);
    chain.block_map.push_back(mapped);
    if (!made) return Problem(state, chain, "the shaders' values could not be given a buffer");
    std::memset(mapped, 0, chain.block_bytes);
  }

  const std::uint32_t set_count = static_cast<std::uint32_t>(chain.passes.size() * images);
  VkDescriptorPoolSize sizes[2] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, std::max(1u, set_count)},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::max(1u, sampler_count * static_cast<std::uint32_t>(images))},
  };
  VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pool.maxSets = set_count;
  pool.poolSizeCount = 2;
  pool.pPoolSizes = sizes;
  if (host.create_descriptor_pool(host.device, &pool, nullptr, &chain.descriptors) != VK_SUCCESS) {
    return Problem(state, chain, "the shaders' inputs could not be allocated");
  }
  for (std::size_t index = 0; index < chain.passes.size(); ++index) {
    BuiltPass& pass = chain.passes[index];
    pass.sets.resize(images, VK_NULL_HANDLE);
    for (std::size_t image = 0; image < images; ++image) {
      VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
      allocation.descriptorPool = chain.descriptors;
      allocation.descriptorSetCount = 1;
      allocation.pSetLayouts = &pass.set_layout;
      if (host.allocate_sets(host.device, &allocation, &pass.sets[image]) != VK_SUCCESS) {
        return Problem(state, chain, "the shaders' inputs could not be allocated");
      }
      std::vector<VkWriteDescriptorSet> writes;
      std::vector<VkDescriptorImageInfo> image_infos(pass.spec->samplers.size());
      VkDescriptorBufferInfo block{chain.blocks[image], pass.block_offset, pass.spec->block_size};
      if (pass.spec->has_block) {
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = pass.sets[image];
        write.dstBinding = pass.spec->block_binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &block;
        writes.push_back(write);
      }
      for (std::size_t s = 0; s < pass.spec->samplers.size(); ++s) {
        const int from = reads[index][s];
        const int target = from < 0 ? -1 : chain.passes[static_cast<std::size_t>(from)].target;
        image_infos[s] = VkDescriptorImageInfo{chain.sampler, ViewOf(chain, target, image),
                                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = pass.sets[image];
        write.dstBinding = pass.spec->samplers[s].binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &image_infos[s];
        writes.push_back(write);
      }
      if (!writes.empty()) {
        host.update_sets(host.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
      }
    }
  }
  return true;
}

void Rebuild(State& state, Chain& chain) {
  DestroyBuilt(chain);
  chain.broken = false;
  chain.spec = state.spec;
  ++state.status.rebuilds;
  if (chain.spec == nullptr) {
    chain.built = true;
    return;
  }
  if (!BuildPasses(state, chain)) {
    DestroyBuilt(chain);
    chain.broken = true;
  }
  chain.built = true;
  state.status.passes = static_cast<std::uint32_t>(chain.passes.size());
}

void Put(void* base, const ChainUniform& uniform, const float value[4]) {
  auto* at = static_cast<unsigned char*>(base) + uniform.offset;
  switch (uniform.kind) {
    case ChainValueKind::kFloat:
      std::memcpy(at, value, 4);
      break;
    case ChainValueKind::kInt: {
      const std::int32_t number = static_cast<std::int32_t>(value[0]);
      std::memcpy(at, &number, 4);
      break;
    }
    case ChainValueKind::kUint: {
      const std::uint32_t number = static_cast<std::uint32_t>(std::max(0.0f, value[0]));
      std::memcpy(at, &number, 4);
      break;
    }
    case ChainValueKind::kVec2:
      std::memcpy(at, value, 8);
      break;
    case ChainValueKind::kVec3:
      std::memcpy(at, value, 12);
      break;
    case ChainValueKind::kVec4:
      std::memcpy(at, value, 16);
      break;
    case ChainValueKind::kIvec2:
    case ChainValueKind::kIvec4: {
      std::int32_t numbers[4];
      for (int i = 0; i < 4; ++i) numbers[i] = static_cast<std::int32_t>(value[i]);
      std::memcpy(at, numbers, uniform.kind == ChainValueKind::kIvec2 ? 8 : 16);
      break;
    }
  }
}

void SizeValue(VkExtent2D extent, float out[4]) {
  out[0] = static_cast<float>(extent.width);
  out[1] = static_cast<float>(extent.height);
  out[2] = 1.0f / static_cast<float>(std::max(1u, extent.width));
  out[3] = 1.0f / static_cast<float>(std::max(1u, extent.height));
}

void FillBlocks(Chain& chain, std::size_t image, double seconds) {
  void* base = chain.block_map[image];
  for (const BuiltPass& pass : chain.passes) {
    if (!pass.spec->has_block) continue;
    void* block = static_cast<unsigned char*>(base) + pass.block_offset;
    for (const ChainUniform& uniform : pass.spec->uniforms) {
      float value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      if (uniform.name == "output_size") {
        SizeValue(pass.extent, value);
      } else if (uniform.name == "source_size") {
        SizeValue(pass.source_extent, value);
      } else if (uniform.name == "original_size") {
        SizeValue(pass.original_extent, value);
      } else if (uniform.name == "game_size") {
        SizeValue(chain.render_extent, value);
      } else if (uniform.name == "window_size") {
        SizeValue(chain.window_extent, value);
      } else if (uniform.name == "time") {
        value[0] = static_cast<float>(std::fmod(seconds, 3600.0));
      } else if (uniform.name == "delta") {
        value[0] = static_cast<float>(chain.delta_seconds);
      } else if (uniform.name == "frame") {
        value[0] = static_cast<float>(chain.frame % 1048576u);
      } else {
        for (const auto& [name, number] : pass.spec->values) {
          if (name == uniform.name) {
            value[0] = number;
            break;
          }
        }
      }
      Put(block, uniform, value);
    }
  }
}

void ImageBarrier(HostDevice& host, VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to,
                  VkAccessFlags source_access, VkAccessFlags target_access, VkPipelineStageFlags source_stage,
                  VkPipelineStageFlags target_stage) {
  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.srcAccessMask = source_access;
  barrier.dstAccessMask = target_access;
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  host.barrier(commands, source_stage, target_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool Record(Chain& chain, std::uint32_t image) {
  HostDevice& host = *chain.host;
  const VkCommandBuffer commands = chain.commands[image];
  if (host.reset_commands(commands, 0) != VK_SUCCESS) return false;
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (host.begin_commands(commands, &begin) != VK_SUCCESS) return false;
  const VkImage proxy = chain.proxy_images[image];
  const VkImage real = chain.real_images[image];
  const bool timing = chain.queries != VK_NULL_HANDLE;
  if (timing) {
    host.reset_queries(commands, chain.queries, image * 2, 2);
    host.write_timestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, chain.queries, image * 2);
    chain.timed[image] = true;
  }
  if (chain.passes.empty()) {
    ImageBarrier(host, commands, proxy, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0,
                 VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    ImageBarrier(host, commands, real, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkImageBlit region{};
    region.srcSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = region.srcSubresource;
    region.srcOffsets[1] = VkOffset3D{static_cast<std::int32_t>(chain.render_extent.width),
                                      static_cast<std::int32_t>(chain.render_extent.height), 1};
    region.dstOffsets[1] = VkOffset3D{static_cast<std::int32_t>(chain.window_extent.width),
                                      static_cast<std::int32_t>(chain.window_extent.height), 1};
    host.blit(commands, proxy, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, real, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
              &region, VK_FILTER_LINEAR);
    ImageBarrier(host, commands, real, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                 VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    ImageBarrier(host, commands, proxy, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                 VK_ACCESS_TRANSFER_READ_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    if (timing) host.write_timestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, chain.queries, image * 2 + 1);
    return host.end_commands(commands) == VK_SUCCESS;
  }
  ImageBarrier(host, commands, proxy, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0,
               VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  for (const BuiltPass& pass : chain.passes) {
    const bool last = pass.target < 0;
    VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    info.renderPass = last ? chain.final_pass : pass.spec->float_target ? chain.float_pass : chain.rgba8_pass;
    info.framebuffer =
        last ? chain.real_framebuffers[image] : chain.targets[static_cast<std::size_t>(pass.target)].framebuffer;
    info.renderArea.extent = pass.extent;
    host.begin_render_pass(commands, &info, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(pass.extent.width),
                              static_cast<float>(pass.extent.height), 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, pass.extent};
    host.set_viewport(commands, 0, 1, &viewport);
    host.set_scissor(commands, 0, 1, &scissor);
    host.bind_pipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);
    host.bind_sets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.layout, 0, 1, &pass.sets[image], 0, nullptr);
    host.draw(commands, 3, 1, 0, 0);
    host.end_render_pass(commands);
  }
  ImageBarrier(host, commands, proxy, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               VK_ACCESS_SHADER_READ_BIT, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
  if (timing) host.write_timestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, chain.queries, image * 2 + 1);
  return host.end_commands(commands) == VK_SUCCESS;
}

bool EnsureCommands(Chain& chain, VkQueue queue) {
  if (chain.pool != VK_NULL_HANDLE) return true;
  HostDevice& host = *chain.host;
  const auto family = host.queues.find(queue);
  if (family == host.queues.end()) return false;
  chain.family = family->second;
  VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool.queueFamilyIndex = chain.family;
  if (host.create_command_pool(host.device, &pool, nullptr, &chain.pool) != VK_SUCCESS) return false;
  chain.commands.resize(chain.real_images.size(), VK_NULL_HANDLE);
  VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocation.commandPool = chain.pool;
  allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocation.commandBufferCount = static_cast<std::uint32_t>(chain.commands.size());
  if (host.allocate_commands(host.device, &allocation, chain.commands.data()) != VK_SUCCESS) return false;
  if (g_timing.load(std::memory_order_relaxed) && host.timestamp_period > 0.0f) {
    VkQueryPoolCreateInfo queries{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    queries.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queries.queryCount = static_cast<std::uint32_t>(chain.commands.size() * 2);
    if (host.create_query_pool(host.device, &queries, nullptr, &chain.queries) != VK_SUCCESS) {
      chain.queries = VK_NULL_HANDLE;
    }
    chain.timed.assign(chain.commands.size(), false);
  }
  return true;
}

}

void SetFrameChainSpec(std::shared_ptr<const FrameChainSpec> spec) {
  SetOwnScaler(spec != nullptr && std::any_of(spec->passes.begin(), spec->passes.end(), [](const ChainPass& pass) {
                 return pass.scaler && !pass.fragment.empty();
               }));
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.spec = std::move(spec);
}

std::shared_ptr<const FrameChainSpec> CurrentFrameChainSpec() {
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.spec;
}

bool FrameChainWantedFor(VkExtent2D render_extent, VkExtent2D window_extent) {
  const std::shared_ptr<const FrameChainSpec> spec = CurrentFrameChainSpec();
  if (spec == nullptr) return false;
  const bool scaled = !SameExtent(render_extent, window_extent);
  return std::any_of(spec->passes.begin(), spec->passes.end(),
                     [scaled](const ChainPass& pass) { return !pass.fragment.empty() && (!pass.scaler || scaled); });
}

void SetFrameChainTiming(bool measure) { g_timing.store(measure, std::memory_order_release); }

void RequestSwapchainRebuild() { g_rebuild_requested.store(true, std::memory_order_release); }

bool SwapchainRebuildRequested() { return g_rebuild_requested.load(std::memory_order_relaxed); }

void NudgeSurfaceCapabilities(VkSurfaceCapabilitiesKHR* capabilities) {
  if (capabilities == nullptr || !SwapchainRebuildRequested()) return;
  if (capabilities->currentExtent.width == UINT32_MAX || capabilities->currentExtent.height < 16) return;
  capabilities->currentExtent.height -= 1;
  capabilities->minImageExtent.height = std::min(capabilities->minImageExtent.height, capabilities->currentExtent.height);
}

bool TakeSwapchainRebuildRequest() {
  return g_rebuild_requested.load(std::memory_order_relaxed) &&
         g_rebuild_requested.exchange(false, std::memory_order_acq_rel);
}

FrameChains& Chains() {
  static FrameChains chains;
  return chains;
}

void FrameChains::RegisterDevice(VkDevice device, VkPhysicalDevice physical_device,
                                 const VkPhysicalDeviceMemoryProperties& memory,
                                 PFN_vkGetDeviceProcAddr get_device_proc_addr, float timestamp_period) {
  if (device == VK_NULL_HANDLE || get_device_proc_addr == nullptr) return;
  auto host = std::make_unique<HostDevice>();
  host->device = device;
  host->physical_device = physical_device;
  host->memory = memory;
  host->timestamp_period = timestamp_period;
#define ELIXIR_CHAIN_RESOLVE(member, type, name) \
  host->member = reinterpret_cast<type>(get_device_proc_addr(device, name));
  ELIXIR_CHAIN_FUNCTIONS(ELIXIR_CHAIN_RESOLVE)
#undef ELIXIR_CHAIN_RESOLVE
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.devices[device] = std::move(host);
}

void FrameChains::DestroyDevice(VkDevice device) {
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  for (auto it = state.chains.begin(); it != state.chains.end();) {
    if (it->second->host->device == device) {
      DestroyChain(*it->second);
      it = state.chains.erase(it);
      __atomic_fetch_sub(&g_live_frame_chains, 1u, __ATOMIC_RELAXED);
    } else {
      ++it;
    }
  }
  state.devices.erase(device);
}

void FrameChains::NoteQueue(VkDevice device, VkQueue queue, std::uint32_t family) {
  if (queue == VK_NULL_HANDLE) return;
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  const auto found = state.devices.find(device);
  if (found != state.devices.end()) found->second->queues[queue] = family;
}

bool FrameChains::CreateSwapchain(const ChainCreateRequest& request, VkSwapchainKHR* swapchain, VkResult* result) {
  if (request.guest == nullptr || request.create == nullptr || swapchain == nullptr) return false;
  if (request.window_extent.width == 0 || request.window_extent.height == 0) return false;
  if (!FrameChainWantedFor(request.guest->imageExtent, request.window_extent)) return false;
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  const auto device = state.devices.find(request.device);
  if (device == state.devices.end() || !device->second->complete()) return false;
  HostDevice& host = *device->second;
  if (request.guest->imageArrayLayers != 1) return false;

  VkSwapchainCreateInfoKHR info = request.host;
  info.imageExtent = request.window_extent;
  info.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  VkSwapchainKHR created = VK_NULL_HANDLE;
  VkResult made = request.create(request.device, &info, nullptr, &created);
  if (made != VK_SUCCESS) {
    info.imageUsage = request.host.imageUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    made = request.create(request.device, &info, nullptr, &created);
  }
  if (made != VK_SUCCESS) {
    Log("shaders: the window's swapchain could not be made for the shader passes (%d)", static_cast<int>(made));
    return false;
  }
  auto chain = std::make_unique<Chain>();
  chain->host = &host;
  chain->swapchain = created;
  chain->format = info.imageFormat;
  chain->render_extent = request.guest->imageExtent;
  chain->window_extent = request.window_extent;
  QueryPerformanceCounter(&chain->started);
  chain->previous = chain->started;
  const bool can_blit = (info.imageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;

  std::uint32_t count = 0;
  bool ok = host.swapchain_images(host.device, created, &count, nullptr) == VK_SUCCESS && count > 0 && count <= 16;
  if (ok) {
    chain->real_images.resize(count);
    ok = host.swapchain_images(host.device, created, &count, chain->real_images.data()) == VK_SUCCESS;
  }
  ok = ok && CreateRenderPass(host, chain->format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &chain->final_pass) &&
       CreateRenderPass(host, kRgba8, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &chain->rgba8_pass) &&
       CreateRenderPass(host, kRgba16f, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &chain->float_pass);
  if (ok) {
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.maxLod = 0.0f;
    ok = host.create_sampler(host.device, &sampler, nullptr, &chain->sampler) == VK_SUCCESS;
  }
  const VkImageUsageFlags proxy_usage = request.guest->imageUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT |
                                        (can_blit ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u);
  for (std::uint32_t i = 0; ok && i < count; ++i) {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkImageView real_view = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    const VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    ok = CreateImage(host, chain->format, chain->render_extent, proxy_usage, &image, &memory);
    chain->proxy_images.push_back(image);
    chain->proxy_memory.push_back(memory);
    ok = ok && CreateView(host, image, chain->format, &view);
    chain->proxy_views.push_back(view);
    ok = ok && CreateView(host, chain->real_images[i], chain->format, &real_view);
    chain->real_views.push_back(real_view);
    ok = ok && CreateFramebuffer(host, chain->final_pass, real_view, chain->window_extent, &framebuffer);
    chain->real_framebuffers.push_back(framebuffer);
    ok = ok && host.create_semaphore(host.device, &semaphore_info, nullptr, &semaphore) == VK_SUCCESS;
    chain->done.push_back(semaphore);
  }
  if (ok) {
    const VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    ok = host.create_fence(host.device, &fence, nullptr, &chain->fence) == VK_SUCCESS;
  }
  if (!ok) {
    Log("shaders: the game's render targets could not be made; the game draws straight to the window");
    DestroyChain(*chain);
    host.destroy_swapchain(host.device, created, nullptr);
    ++state.status.failures;
    return false;
  }
  if (!can_blit) Log("shaders: this driver's window images cannot be copied to; a failed shader shows nothing");
  Log("shaders: the game renders %ux%u and Elixir draws it to the window's %ux%u (%u images)",
      chain->render_extent.width, chain->render_extent.height, chain->window_extent.width,
      chain->window_extent.height, count);
  state.status.render_width = chain->render_extent.width;
  state.status.render_height = chain->render_extent.height;
  state.status.window_width = chain->window_extent.width;
  state.status.window_height = chain->window_extent.height;
  ++state.status.chains_created;
  state.status.gpu_ms_total = 0.0;
  state.status.gpu_frames = 0;
  state.chains[created] = std::move(chain);
  __atomic_fetch_add(&g_live_frame_chains, 1u, __ATOMIC_RELAXED);
  *swapchain = created;
  *result = VK_SUCCESS;
  return true;
}

bool FrameChains::GetImages(VkSwapchainKHR swapchain, std::uint32_t* count, VkImage* images, VkResult* result) {
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  const auto found = state.chains.find(swapchain);
  if (found == state.chains.end() || count == nullptr) return false;
  const std::vector<VkImage>& proxies = found->second->proxy_images;
  const std::uint32_t available = static_cast<std::uint32_t>(proxies.size());
  if (images == nullptr) {
    *count = available;
    *result = VK_SUCCESS;
    return true;
  }
  const std::uint32_t copied = std::min(*count, available);
  std::copy_n(proxies.begin(), copied, images);
  *count = copied;
  *result = copied < available ? VK_INCOMPLETE : VK_SUCCESS;
  return true;
}

void FrameChains::DestroySwapchain(VkSwapchainKHR swapchain) {
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  const auto found = state.chains.find(swapchain);
  if (found == state.chains.end()) return;
  DestroyChain(*found->second);
  state.chains.erase(found);
  __atomic_fetch_sub(&g_live_frame_chains, 1u, __ATOMIC_RELAXED);
}

bool FrameChains::Present(VkQueue queue, const VkPresentInfoKHR* guest, PFN_vkQueuePresentKHR present,
                          VkResult* result) {
  if (guest == nullptr || present == nullptr || guest->swapchainCount != 1 || guest->pSwapchains == nullptr ||
      guest->pImageIndices == nullptr) {
    return false;
  }
  State& state = GetState();
  std::unique_lock<std::mutex> lock(state.mutex);
  const auto found = state.chains.find(guest->pSwapchains[0]);
  if (found == state.chains.end()) return false;
  Chain& chain = *found->second;
  HostDevice& host = *chain.host;
  const std::uint32_t image = guest->pImageIndices[0];
  if (image >= chain.real_images.size() || !EnsureCommands(chain, queue)) {
    ++state.status.failures;
    *result = VK_ERROR_OUT_OF_DATE_KHR;
    if (guest->pResults != nullptr) guest->pResults[0] = *result;
    return true;
  }
  if (!chain.built || chain.spec != state.spec) Rebuild(state, chain);

  LARGE_INTEGER now{};
  LARGE_INTEGER frequency{};
  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&frequency);
  const double seconds = static_cast<double>(now.QuadPart - chain.started.QuadPart) / frequency.QuadPart;
  chain.delta_seconds = static_cast<double>(now.QuadPart - chain.previous.QuadPart) / frequency.QuadPart;
  chain.previous = now;
  if (chain.frame > 0) {
    const double milliseconds = chain.delta_seconds * 1000.0;
    state.status.frame_ms = state.status.frame_ms <= 0.0 ? milliseconds
                                                          : state.status.frame_ms * 0.9 + milliseconds * 0.1;
  }
  if (chain.queries != VK_NULL_HANDLE && chain.timed[image]) {
    std::uint64_t ticks[2] = {0, 0};
    if (host.query_results(host.device, chain.queries, image * 2, 2, sizeof(ticks), ticks, sizeof(std::uint64_t),
                           VK_QUERY_RESULT_64_BIT) == VK_SUCCESS &&
        ticks[1] >= ticks[0]) {
      state.status.gpu_ms_total += static_cast<double>(ticks[1] - ticks[0]) * host.timestamp_period / 1.0e6;
      ++state.status.gpu_frames;
    }
  }
  if (!chain.passes.empty()) FillBlocks(chain, image, seconds);
  ++chain.frame;
  ++state.status.frames;

  bool recorded = Record(chain, image);
  if (recorded) {
    if (chain.fence_pending) {
      host.wait_fences(host.device, 1, &chain.fence, VK_TRUE, 2000000000ull);
      chain.fence_pending = false;
    }
    host.reset_fences(host.device, 1, &chain.fence);
    std::vector<VkPipelineStageFlags> stages(guest->waitSemaphoreCount, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = guest->waitSemaphoreCount;
    submit.pWaitSemaphores = guest->pWaitSemaphores;
    submit.pWaitDstStageMask = stages.data();
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &chain.commands[image];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &chain.done[image];
    recorded = host.submit(queue, 1, &submit, chain.fence) == VK_SUCCESS;
    chain.fence_pending = recorded;
  }
  if (!recorded) {
    ++state.status.failures;
    state.status.last_problem = "a frame could not be drawn through the shaders";
    *result = VK_ERROR_DEVICE_LOST;
    if (guest->pResults != nullptr) guest->pResults[0] = *result;
    return true;
  }
  VkPresentInfoKHR info = *guest;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &chain.done[image];
  lock.unlock();
  *result = present(queue, &info);
  return true;
}

FrameChainStatus FrameChains::Status() {
  State& state = GetState();
  std::lock_guard<std::mutex> lock(state.mutex);
  FrameChainStatus status = state.status;
  status.live_chains = static_cast<std::uint32_t>(state.chains.size());
  return status;
}

}
