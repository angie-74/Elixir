// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// GENERATED - do not edit. Produced by tools/gen_vulkan_thunks.py from
// third_party/Vulkan-Headers/registry/vk.xml (VK_HEADER_VERSION 356).
// Regenerate with:  python tools/gen_vulkan_thunks.py
#include "graphics/vulkan/generated/vk_sysv_thunks.h"

#include <cstring>
#include <type_traits>

namespace elixir::vulkan::generated {

PFN_vkVoidFunction g_host_dispatch[kCommandCount] = {};

}

extern "C" {
ELIXIR_SYSV VkResult elixir_vk_sysv_vkAcquireDrmDisplayEXT(VkPhysicalDevice physicalDevice, int32_t drmFd, VkDisplayKHR display) {
  return ELIXIR_VK_HOST(vkAcquireDrmDisplayEXT)(physicalDevice, drmFd, display);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkAcquirePerformanceConfigurationINTEL(VkDevice device, const VkPerformanceConfigurationAcquireInfoINTEL* pAcquireInfo, VkPerformanceConfigurationINTEL* pConfiguration) {
  return ELIXIR_VK_HOST(vkAcquirePerformanceConfigurationINTEL)(device, pAcquireInfo, pConfiguration);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkAcquireProfilingLockKHR(VkDevice device, const VkAcquireProfilingLockInfoKHR* pInfo) {
  return ELIXIR_VK_HOST(vkAcquireProfilingLockKHR)(device, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
  return ELIXIR_VK_HOST(vkAllocateDescriptorSets)(device, pAllocateInfo, pDescriptorSets);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks*, VkDeviceMemory* pMemory) {
  return ELIXIR_VK_HOST(vkAllocateMemory)(device, pAllocateInfo, nullptr, pMemory);
}

ELIXIR_SYSV void elixir_vk_sysv_vkAntiLagUpdateAMD(VkDevice device, const VkAntiLagDataAMD* pData) {
  ELIXIR_VK_HOST(vkAntiLagUpdateAMD)(device, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBindAccelerationStructureMemoryNV(VkDevice device, uint32_t bindInfoCount, const VkBindAccelerationStructureMemoryInfoNV* pBindInfos) {
  return ELIXIR_VK_HOST(vkBindAccelerationStructureMemoryNV)(device, bindInfoCount, pBindInfos);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBindDataGraphPipelineSessionMemoryARM(VkDevice device, uint32_t bindInfoCount, const VkBindDataGraphPipelineSessionMemoryInfoARM* pBindInfos) {
  return ELIXIR_VK_HOST(vkBindDataGraphPipelineSessionMemoryARM)(device, bindInfoCount, pBindInfos);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
  return ELIXIR_VK_HOST(vkBindImageMemory)(device, image, memory, memoryOffset);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
  return ELIXIR_VK_HOST(vkBindImageMemory2)(device, bindInfoCount, pBindInfos);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
  return ELIXIR_VK_HOST(vkBindImageMemory2KHR)(device, bindInfoCount, pBindInfos);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBindOpticalFlowSessionImageNV(VkDevice device, VkOpticalFlowSessionNV session, VkOpticalFlowSessionBindingPointNV bindingPoint, VkImageView view, VkImageLayout layout) {
  return ELIXIR_VK_HOST(vkBindOpticalFlowSessionImageNV)(device, session, bindingPoint, view, layout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBindTensorMemoryARM(VkDevice device, uint32_t bindInfoCount, const VkBindTensorMemoryInfoARM* pBindInfos) {
  return ELIXIR_VK_HOST(vkBindTensorMemoryARM)(device, bindInfoCount, pBindInfos);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBindVideoSessionMemoryKHR(VkDevice device, VkVideoSessionKHR videoSession, uint32_t bindSessionMemoryInfoCount, const VkBindVideoSessionMemoryInfoKHR* pBindSessionMemoryInfos) {
  return ELIXIR_VK_HOST(vkBindVideoSessionMemoryKHR)(device, videoSession, bindSessionMemoryInfoCount, pBindSessionMemoryInfos);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBuildAccelerationStructuresKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos) {
  return ELIXIR_VK_HOST(vkBuildAccelerationStructuresKHR)(device, deferredOperation, infoCount, pInfos, ppBuildRangeInfos);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkBuildMicromapsEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkMicromapBuildInfoEXT* pInfos) {
  return ELIXIR_VK_HOST(vkBuildMicromapsEXT)(device, deferredOperation, infoCount, pInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkClearShaderInstrumentationMetricsARM(VkDevice device, VkShaderInstrumentationARM instrumentation) {
  ELIXIR_VK_HOST(vkClearShaderInstrumentationMetricsARM)(device, instrumentation);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginConditionalRendering2EXT(VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfo2EXT* pConditionalRenderingBegin) {
  ELIXIR_VK_HOST(vkCmdBeginConditionalRendering2EXT)(commandBuffer, pConditionalRenderingBegin);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginConditionalRenderingEXT(VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) {
  ELIXIR_VK_HOST(vkCmdBeginConditionalRenderingEXT)(commandBuffer, pConditionalRenderingBegin);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginCustomResolveEXT(VkCommandBuffer commandBuffer, const VkBeginCustomResolveInfoEXT* pBeginCustomResolveInfo) {
  ELIXIR_VK_HOST(vkCmdBeginCustomResolveEXT)(commandBuffer, pBeginCustomResolveInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) {
  ELIXIR_VK_HOST(vkCmdBeginDebugUtilsLabelEXT)(commandBuffer, pLabelInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCmdBeginGpaSampleAMD(VkCommandBuffer commandBuffer, VkGpaSessionAMD gpaSession, const VkGpaSampleBeginInfoAMD* pGpaSampleBeginInfo, uint32_t* pSampleID) {
  return ELIXIR_VK_HOST(vkCmdBeginGpaSampleAMD)(commandBuffer, gpaSession, pGpaSampleBeginInfo, pSampleID);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCmdBeginGpaSessionAMD(VkCommandBuffer commandBuffer, VkGpaSessionAMD gpaSession) {
  return ELIXIR_VK_HOST(vkCmdBeginGpaSessionAMD)(commandBuffer, gpaSession);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginPerTileExecutionQCOM(VkCommandBuffer commandBuffer, const VkPerTileBeginInfoQCOM* pPerTileBeginInfo) {
  ELIXIR_VK_HOST(vkCmdBeginPerTileExecutionQCOM)(commandBuffer, pPerTileBeginInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
  ELIXIR_VK_HOST(vkCmdBeginQuery)(commandBuffer, queryPool, query, flags);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) {
  ELIXIR_VK_HOST(vkCmdBeginQueryIndexedEXT)(commandBuffer, queryPool, query, flags, index);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
  ELIXIR_VK_HOST(vkCmdBeginRenderPass)(commandBuffer, pRenderPassBegin, contents);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
  ELIXIR_VK_HOST(vkCmdBeginRenderPass2)(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginRenderPass2KHR(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
  ELIXIR_VK_HOST(vkCmdBeginRenderPass2KHR)(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
  ELIXIR_VK_HOST(vkCmdBeginRendering)(commandBuffer, pRenderingInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
  ELIXIR_VK_HOST(vkCmdBeginRenderingKHR)(commandBuffer, pRenderingInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginShaderInstrumentationARM(VkCommandBuffer commandBuffer, VkShaderInstrumentationARM instrumentation) {
  ELIXIR_VK_HOST(vkCmdBeginShaderInstrumentationARM)(commandBuffer, instrumentation);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginTransformFeedback2EXT(VkCommandBuffer commandBuffer, uint32_t firstCounterRange, uint32_t counterRangeCount, const VkBindTransformFeedbackBuffer2InfoEXT* pCounterInfos) {
  ELIXIR_VK_HOST(vkCmdBeginTransformFeedback2EXT)(commandBuffer, firstCounterRange, counterRangeCount, pCounterInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
  ELIXIR_VK_HOST(vkCmdBeginTransformFeedbackEXT)(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBeginVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoBeginCodingInfoKHR* pBeginInfo) {
  ELIXIR_VK_HOST(vkCmdBeginVideoCodingKHR)(commandBuffer, pBeginInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(VkCommandBuffer commandBuffer, const VkBindDescriptorBufferEmbeddedSamplersInfoEXT* pBindDescriptorBufferEmbeddedSamplersInfo) {
  ELIXIR_VK_HOST(vkCmdBindDescriptorBufferEmbeddedSamplers2EXT)(commandBuffer, pBindDescriptorBufferEmbeddedSamplersInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindDescriptorBufferEmbeddedSamplersEXT(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set) {
  ELIXIR_VK_HOST(vkCmdBindDescriptorBufferEmbeddedSamplersEXT)(commandBuffer, pipelineBindPoint, layout, set);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindDescriptorBuffersEXT(VkCommandBuffer commandBuffer, uint32_t bufferCount, const VkDescriptorBufferBindingInfoEXT* pBindingInfos) {
  ELIXIR_VK_HOST(vkCmdBindDescriptorBuffersEXT)(commandBuffer, bufferCount, pBindingInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
  ELIXIR_VK_HOST(vkCmdBindDescriptorSets)(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindDescriptorSets2(VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfo* pBindDescriptorSetsInfo) {
  ELIXIR_VK_HOST(vkCmdBindDescriptorSets2)(commandBuffer, pBindDescriptorSetsInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindDescriptorSets2KHR(VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfo* pBindDescriptorSetsInfo) {
  ELIXIR_VK_HOST(vkCmdBindDescriptorSets2KHR)(commandBuffer, pBindDescriptorSetsInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
  ELIXIR_VK_HOST(vkCmdBindIndexBuffer)(commandBuffer, buffer, offset, indexType);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindIndexBuffer2(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType) {
  ELIXIR_VK_HOST(vkCmdBindIndexBuffer2)(commandBuffer, buffer, offset, size, indexType);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindIndexBuffer2KHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType) {
  ELIXIR_VK_HOST(vkCmdBindIndexBuffer2KHR)(commandBuffer, buffer, offset, size, indexType);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindIndexBuffer3KHR(VkCommandBuffer commandBuffer, const VkBindIndexBuffer3InfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdBindIndexBuffer3KHR)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindInvocationMaskHUAWEI(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout) {
  ELIXIR_VK_HOST(vkCmdBindInvocationMaskHUAWEI)(commandBuffer, imageView, imageLayout);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
  ELIXIR_VK_HOST(vkCmdBindPipeline)(commandBuffer, pipelineBindPoint, pipeline);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindPipelineShaderGroupNV(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline, uint32_t groupIndex) {
  ELIXIR_VK_HOST(vkCmdBindPipelineShaderGroupNV)(commandBuffer, pipelineBindPoint, pipeline, groupIndex);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindResourceHeapEXT(VkCommandBuffer commandBuffer, const VkBindHeapInfoEXT* pBindInfo) {
  ELIXIR_VK_HOST(vkCmdBindResourceHeapEXT)(commandBuffer, pBindInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindSamplerHeapEXT(VkCommandBuffer commandBuffer, const VkBindHeapInfoEXT* pBindInfo) {
  ELIXIR_VK_HOST(vkCmdBindSamplerHeapEXT)(commandBuffer, pBindInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindShadersEXT(VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits* pStages, const VkShaderEXT* pShaders) {
  ELIXIR_VK_HOST(vkCmdBindShadersEXT)(commandBuffer, stageCount, pStages, pShaders);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindShadingRateImageNV(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout) {
  ELIXIR_VK_HOST(vkCmdBindShadingRateImageNV)(commandBuffer, imageView, imageLayout);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindTileMemoryQCOM(VkCommandBuffer commandBuffer, const VkTileMemoryBindInfoQCOM* pTileMemoryBindInfo) {
  ELIXIR_VK_HOST(vkCmdBindTileMemoryQCOM)(commandBuffer, pTileMemoryBindInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindTransformFeedbackBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBindTransformFeedbackBuffer2InfoEXT* pBindingInfos) {
  ELIXIR_VK_HOST(vkCmdBindTransformFeedbackBuffers2EXT)(commandBuffer, firstBinding, bindingCount, pBindingInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) {
  ELIXIR_VK_HOST(vkCmdBindTransformFeedbackBuffersEXT)(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
  ELIXIR_VK_HOST(vkCmdBindVertexBuffers)(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
  ELIXIR_VK_HOST(vkCmdBindVertexBuffers2)(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
  ELIXIR_VK_HOST(vkCmdBindVertexBuffers2EXT)(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBindVertexBuffers3KHR(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBindVertexBuffer3InfoKHR* pBindingInfos) {
  ELIXIR_VK_HOST(vkCmdBindVertexBuffers3KHR)(commandBuffer, firstBinding, bindingCount, pBindingInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
  ELIXIR_VK_HOST(vkCmdBlitImage)(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
  ELIXIR_VK_HOST(vkCmdBlitImage2)(commandBuffer, pBlitImageInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBlitImage2KHR(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
  ELIXIR_VK_HOST(vkCmdBlitImage2KHR)(commandBuffer, pBlitImageInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBuildAccelerationStructureNV(VkCommandBuffer commandBuffer, const VkAccelerationStructureInfoNV* pInfo, VkBuffer instanceData, VkDeviceSize instanceOffset, VkBool32 update, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkBuffer scratch, VkDeviceSize scratchOffset) {
  ELIXIR_VK_HOST(vkCmdBuildAccelerationStructureNV)(commandBuffer, pInfo, instanceData, instanceOffset, update, dst, src, scratch, scratchOffset);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBuildAccelerationStructuresIndirectKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t* const* ppMaxPrimitiveCounts) {
  ELIXIR_VK_HOST(vkCmdBuildAccelerationStructuresIndirectKHR)(commandBuffer, infoCount, pInfos, pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos) {
  ELIXIR_VK_HOST(vkCmdBuildAccelerationStructuresKHR)(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBuildClusterAccelerationStructureIndirectNV(VkCommandBuffer commandBuffer, const VkClusterAccelerationStructureCommandsInfoNV* pCommandInfos) {
  ELIXIR_VK_HOST(vkCmdBuildClusterAccelerationStructureIndirectNV)(commandBuffer, pCommandInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBuildMicromapsEXT(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkMicromapBuildInfoEXT* pInfos) {
  ELIXIR_VK_HOST(vkCmdBuildMicromapsEXT)(commandBuffer, infoCount, pInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdBuildPartitionedAccelerationStructuresNV(VkCommandBuffer commandBuffer, const VkBuildPartitionedAccelerationStructureInfoNV* pBuildInfo) {
  ELIXIR_VK_HOST(vkCmdBuildPartitionedAccelerationStructuresNV)(commandBuffer, pBuildInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
  ELIXIR_VK_HOST(vkCmdClearAttachments)(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
  ELIXIR_VK_HOST(vkCmdClearColorImage)(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
  ELIXIR_VK_HOST(vkCmdClearDepthStencilImage)(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdControlVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoCodingControlInfoKHR* pCodingControlInfo) {
  ELIXIR_VK_HOST(vkCmdControlVideoCodingKHR)(commandBuffer, pCodingControlInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdConvertCooperativeVectorMatrixNV(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkConvertCooperativeVectorMatrixInfoNV* pInfos) {
  ELIXIR_VK_HOST(vkCmdConvertCooperativeVectorMatrixNV)(commandBuffer, infoCount, pInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdCopyAccelerationStructureKHR)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyAccelerationStructureNV(VkCommandBuffer commandBuffer, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkCopyAccelerationStructureModeKHR mode) {
  ELIXIR_VK_HOST(vkCmdCopyAccelerationStructureNV)(commandBuffer, dst, src, mode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyAccelerationStructureToMemoryKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdCopyAccelerationStructureToMemoryKHR)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
  ELIXIR_VK_HOST(vkCmdCopyBuffer)(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
  ELIXIR_VK_HOST(vkCmdCopyBuffer2)(commandBuffer, pCopyBufferInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
  ELIXIR_VK_HOST(vkCmdCopyBuffer2KHR)(commandBuffer, pCopyBufferInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyGpaSessionResultsAMD(VkCommandBuffer commandBuffer, VkGpaSessionAMD gpaSession) {
  ELIXIR_VK_HOST(vkCmdCopyGpaSessionResultsAMD)(commandBuffer, gpaSession);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyImageToMemoryKHR(VkCommandBuffer commandBuffer, const VkCopyDeviceMemoryImageInfoKHR* pCopyMemoryInfo) {
  ELIXIR_VK_HOST(vkCmdCopyImageToMemoryKHR)(commandBuffer, pCopyMemoryInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMemoryIndirectKHR(VkCommandBuffer commandBuffer, const VkCopyMemoryIndirectInfoKHR* pCopyMemoryIndirectInfo) {
  ELIXIR_VK_HOST(vkCmdCopyMemoryIndirectKHR)(commandBuffer, pCopyMemoryIndirectInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMemoryIndirectNV(VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdCopyMemoryIndirectNV)(commandBuffer, copyBufferAddress, copyCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMemoryKHR(VkCommandBuffer commandBuffer, const VkCopyDeviceMemoryInfoKHR* pCopyMemoryInfo) {
  ELIXIR_VK_HOST(vkCmdCopyMemoryKHR)(commandBuffer, pCopyMemoryInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMemoryToAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdCopyMemoryToAccelerationStructureKHR)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMemoryToImageIndirectKHR(VkCommandBuffer commandBuffer, const VkCopyMemoryToImageIndirectInfoKHR* pCopyMemoryToImageIndirectInfo) {
  ELIXIR_VK_HOST(vkCmdCopyMemoryToImageIndirectKHR)(commandBuffer, pCopyMemoryToImageIndirectInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMemoryToImageIndirectNV(VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride, VkImage dstImage, VkImageLayout dstImageLayout, const VkImageSubresourceLayers* pImageSubresources) {
  ELIXIR_VK_HOST(vkCmdCopyMemoryToImageIndirectNV)(commandBuffer, copyBufferAddress, copyCount, stride, dstImage, dstImageLayout, pImageSubresources);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMemoryToImageKHR(VkCommandBuffer commandBuffer, const VkCopyDeviceMemoryImageInfoKHR* pCopyMemoryInfo) {
  ELIXIR_VK_HOST(vkCmdCopyMemoryToImageKHR)(commandBuffer, pCopyMemoryInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMemoryToMicromapEXT(VkCommandBuffer commandBuffer, const VkCopyMemoryToMicromapInfoEXT* pInfo) {
  ELIXIR_VK_HOST(vkCmdCopyMemoryToMicromapEXT)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMicromapEXT(VkCommandBuffer commandBuffer, const VkCopyMicromapInfoEXT* pInfo) {
  ELIXIR_VK_HOST(vkCmdCopyMicromapEXT)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyMicromapToMemoryEXT(VkCommandBuffer commandBuffer, const VkCopyMicromapToMemoryInfoEXT* pInfo) {
  ELIXIR_VK_HOST(vkCmdCopyMicromapToMemoryEXT)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
  ELIXIR_VK_HOST(vkCmdCopyQueryPoolResults)(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyQueryPoolResultsToMemoryKHR(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, const VkStridedDeviceAddressRangeKHR* pDstRange, VkAddressCommandFlagsKHR dstFlags, VkQueryResultFlags queryResultFlags) {
  ELIXIR_VK_HOST(vkCmdCopyQueryPoolResultsToMemoryKHR)(commandBuffer, queryPool, firstQuery, queryCount, pDstRange, dstFlags, queryResultFlags);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCopyTensorARM(VkCommandBuffer commandBuffer, const VkCopyTensorInfoARM* pCopyTensorInfo) {
  ELIXIR_VK_HOST(vkCmdCopyTensorARM)(commandBuffer, pCopyTensorInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCuLaunchKernelNVX(VkCommandBuffer commandBuffer, const VkCuLaunchInfoNVX* pLaunchInfo) {
  ELIXIR_VK_HOST(vkCmdCuLaunchKernelNVX)(commandBuffer, pLaunchInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdCudaLaunchKernelNV(VkCommandBuffer commandBuffer, const VkCudaLaunchInfoNV* pLaunchInfo) {
  ELIXIR_VK_HOST(vkCmdCudaLaunchKernelNV)(commandBuffer, pLaunchInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDebugMarkerBeginEXT(VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo) {
  ELIXIR_VK_HOST(vkCmdDebugMarkerBeginEXT)(commandBuffer, pMarkerInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDebugMarkerEndEXT(VkCommandBuffer commandBuffer) {
  ELIXIR_VK_HOST(vkCmdDebugMarkerEndEXT)(commandBuffer);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDebugMarkerInsertEXT(VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo) {
  ELIXIR_VK_HOST(vkCmdDebugMarkerInsertEXT)(commandBuffer, pMarkerInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDecodeVideoKHR(VkCommandBuffer commandBuffer, const VkVideoDecodeInfoKHR* pDecodeInfo) {
  ELIXIR_VK_HOST(vkCmdDecodeVideoKHR)(commandBuffer, pDecodeInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDecompressMemoryEXT(VkCommandBuffer commandBuffer, const VkDecompressMemoryInfoEXT* pDecompressMemoryInfoEXT) {
  ELIXIR_VK_HOST(vkCmdDecompressMemoryEXT)(commandBuffer, pDecompressMemoryInfoEXT);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDecompressMemoryIndirectCountEXT(VkCommandBuffer commandBuffer, VkMemoryDecompressionMethodFlagsEXT decompressionMethod, VkDeviceAddress indirectCommandsAddress, VkDeviceAddress indirectCommandsCountAddress, uint32_t maxDecompressionCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDecompressMemoryIndirectCountEXT)(commandBuffer, decompressionMethod, indirectCommandsAddress, indirectCommandsCountAddress, maxDecompressionCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDecompressMemoryIndirectCountNV(VkCommandBuffer commandBuffer, VkDeviceAddress indirectCommandsAddress, VkDeviceAddress indirectCommandsCountAddress, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDecompressMemoryIndirectCountNV)(commandBuffer, indirectCommandsAddress, indirectCommandsCountAddress, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDecompressMemoryNV(VkCommandBuffer commandBuffer, uint32_t decompressRegionCount, const VkDecompressMemoryRegionNV* pDecompressMemoryRegions) {
  ELIXIR_VK_HOST(vkCmdDecompressMemoryNV)(commandBuffer, decompressRegionCount, pDecompressMemoryRegions);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
  ELIXIR_VK_HOST(vkCmdDispatch)(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
  ELIXIR_VK_HOST(vkCmdDispatchBase)(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatchBaseKHR(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
  ELIXIR_VK_HOST(vkCmdDispatchBaseKHR)(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatchDataGraphARM(VkCommandBuffer commandBuffer, VkDataGraphPipelineSessionARM session, const VkDataGraphPipelineDispatchInfoARM* pInfo) {
  ELIXIR_VK_HOST(vkCmdDispatchDataGraphARM)(commandBuffer, session, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatchGraphAMDX(VkCommandBuffer commandBuffer, VkDeviceAddress scratch, VkDeviceSize scratchSize, const VkDispatchGraphCountInfoAMDX* pCountInfo) {
  ELIXIR_VK_HOST(vkCmdDispatchGraphAMDX)(commandBuffer, scratch, scratchSize, pCountInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatchGraphIndirectAMDX(VkCommandBuffer commandBuffer, VkDeviceAddress scratch, VkDeviceSize scratchSize, const VkDispatchGraphCountInfoAMDX* pCountInfo) {
  ELIXIR_VK_HOST(vkCmdDispatchGraphIndirectAMDX)(commandBuffer, scratch, scratchSize, pCountInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatchGraphIndirectCountAMDX(VkCommandBuffer commandBuffer, VkDeviceAddress scratch, VkDeviceSize scratchSize, VkDeviceAddress countInfo) {
  ELIXIR_VK_HOST(vkCmdDispatchGraphIndirectCountAMDX)(commandBuffer, scratch, scratchSize, countInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
  ELIXIR_VK_HOST(vkCmdDispatchIndirect)(commandBuffer, buffer, offset);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatchIndirect2KHR(VkCommandBuffer commandBuffer, const VkDispatchIndirect2InfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdDispatchIndirect2KHR)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDispatchTileQCOM(VkCommandBuffer commandBuffer, const VkDispatchTileInfoQCOM* pDispatchTileInfo) {
  ELIXIR_VK_HOST(vkCmdDispatchTileQCOM)(commandBuffer, pDispatchTileInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
  ELIXIR_VK_HOST(vkCmdDraw)(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawClusterHUAWEI(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
  ELIXIR_VK_HOST(vkCmdDrawClusterHUAWEI)(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawClusterIndirectHUAWEI(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
  ELIXIR_VK_HOST(vkCmdDrawClusterIndirectHUAWEI)(commandBuffer, buffer, offset);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
  ELIXIR_VK_HOST(vkCmdDrawIndexed)(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawIndexedIndirect)(commandBuffer, buffer, offset, drawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndexedIndirect2KHR(VkCommandBuffer commandBuffer, const VkDrawIndirect2InfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdDrawIndexedIndirect2KHR)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawIndexedIndirectCount)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndexedIndirectCount2KHR(VkCommandBuffer commandBuffer, const VkDrawIndirectCount2InfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdDrawIndexedIndirectCount2KHR)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndexedIndirectCountAMD(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawIndexedIndirectCountAMD)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawIndexedIndirectCountKHR)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawIndirect)(commandBuffer, buffer, offset, drawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndirect2KHR(VkCommandBuffer commandBuffer, const VkDrawIndirect2InfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdDrawIndirect2KHR)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndirectByteCount2EXT(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, const VkBindTransformFeedbackBuffer2InfoEXT* pCounterInfo, uint32_t counterOffset, uint32_t vertexStride) {
  ELIXIR_VK_HOST(vkCmdDrawIndirectByteCount2EXT)(commandBuffer, instanceCount, firstInstance, pCounterInfo, counterOffset, vertexStride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) {
  ELIXIR_VK_HOST(vkCmdDrawIndirectByteCountEXT)(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawIndirectCount)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndirectCount2KHR(VkCommandBuffer commandBuffer, const VkDrawIndirectCount2InfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdDrawIndirectCount2KHR)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndirectCountAMD(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawIndirectCountAMD)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawIndirectCountKHR)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMeshTasksEXT(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
  ELIXIR_VK_HOST(vkCmdDrawMeshTasksEXT)(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMeshTasksIndirect2EXT(VkCommandBuffer commandBuffer, const VkDrawIndirect2InfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdDrawMeshTasksIndirect2EXT)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMeshTasksIndirectCount2EXT(VkCommandBuffer commandBuffer, const VkDrawIndirectCount2InfoKHR* pInfo) {
  ELIXIR_VK_HOST(vkCmdDrawMeshTasksIndirectCount2EXT)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMeshTasksIndirectCountEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawMeshTasksIndirectCountEXT)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMeshTasksIndirectCountNV(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawMeshTasksIndirectCountNV)(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMeshTasksIndirectEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawMeshTasksIndirectEXT)(commandBuffer, buffer, offset, drawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMeshTasksIndirectNV(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawMeshTasksIndirectNV)(commandBuffer, buffer, offset, drawCount, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMeshTasksNV(VkCommandBuffer commandBuffer, uint32_t taskCount, uint32_t firstTask) {
  ELIXIR_VK_HOST(vkCmdDrawMeshTasksNV)(commandBuffer, taskCount, firstTask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMultiEXT(VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawInfoEXT* pVertexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride) {
  ELIXIR_VK_HOST(vkCmdDrawMultiEXT)(commandBuffer, drawCount, pVertexInfo, instanceCount, firstInstance, stride);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdDrawMultiIndexedEXT(VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawIndexedInfoEXT* pIndexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride, const int32_t* pVertexOffset) {
  ELIXIR_VK_HOST(vkCmdDrawMultiIndexedEXT)(commandBuffer, drawCount, pIndexInfo, instanceCount, firstInstance, stride, pVertexOffset);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEncodeVideoKHR(VkCommandBuffer commandBuffer, const VkVideoEncodeInfoKHR* pEncodeInfo) {
  ELIXIR_VK_HOST(vkCmdEncodeVideoKHR)(commandBuffer, pEncodeInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer) {
  ELIXIR_VK_HOST(vkCmdEndConditionalRenderingEXT)(commandBuffer);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer) {
  ELIXIR_VK_HOST(vkCmdEndDebugUtilsLabelEXT)(commandBuffer);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndGpaSampleAMD(VkCommandBuffer commandBuffer, VkGpaSessionAMD gpaSession, uint32_t sampleID) {
  ELIXIR_VK_HOST(vkCmdEndGpaSampleAMD)(commandBuffer, gpaSession, sampleID);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCmdEndGpaSessionAMD(VkCommandBuffer commandBuffer, VkGpaSessionAMD gpaSession) {
  return ELIXIR_VK_HOST(vkCmdEndGpaSessionAMD)(commandBuffer, gpaSession);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndPerTileExecutionQCOM(VkCommandBuffer commandBuffer, const VkPerTileEndInfoQCOM* pPerTileEndInfo) {
  ELIXIR_VK_HOST(vkCmdEndPerTileExecutionQCOM)(commandBuffer, pPerTileEndInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
  ELIXIR_VK_HOST(vkCmdEndQuery)(commandBuffer, queryPool, query);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) {
  ELIXIR_VK_HOST(vkCmdEndQueryIndexedEXT)(commandBuffer, queryPool, query, index);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
  ELIXIR_VK_HOST(vkCmdEndRenderPass)(commandBuffer);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
  ELIXIR_VK_HOST(vkCmdEndRenderPass2)(commandBuffer, pSubpassEndInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndRenderPass2KHR(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
  ELIXIR_VK_HOST(vkCmdEndRenderPass2KHR)(commandBuffer, pSubpassEndInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndRendering(VkCommandBuffer commandBuffer) {
  ELIXIR_VK_HOST(vkCmdEndRendering)(commandBuffer);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndRendering2EXT(VkCommandBuffer commandBuffer, const VkRenderingEndInfoKHR* pRenderingEndInfo) {
  ELIXIR_VK_HOST(vkCmdEndRendering2EXT)(commandBuffer, pRenderingEndInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndRendering2KHR(VkCommandBuffer commandBuffer, const VkRenderingEndInfoKHR* pRenderingEndInfo) {
  ELIXIR_VK_HOST(vkCmdEndRendering2KHR)(commandBuffer, pRenderingEndInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer) {
  ELIXIR_VK_HOST(vkCmdEndRenderingKHR)(commandBuffer);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndShaderInstrumentationARM(VkCommandBuffer commandBuffer) {
  ELIXIR_VK_HOST(vkCmdEndShaderInstrumentationARM)(commandBuffer);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndTransformFeedback2EXT(VkCommandBuffer commandBuffer, uint32_t firstCounterRange, uint32_t counterRangeCount, const VkBindTransformFeedbackBuffer2InfoEXT* pCounterInfos) {
  ELIXIR_VK_HOST(vkCmdEndTransformFeedback2EXT)(commandBuffer, firstCounterRange, counterRangeCount, pCounterInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
  ELIXIR_VK_HOST(vkCmdEndTransformFeedbackEXT)(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdEndVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoEndCodingInfoKHR* pEndCodingInfo) {
  ELIXIR_VK_HOST(vkCmdEndVideoCodingKHR)(commandBuffer, pEndCodingInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdExecuteGeneratedCommandsEXT(VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoEXT* pGeneratedCommandsInfo) {
  ELIXIR_VK_HOST(vkCmdExecuteGeneratedCommandsEXT)(commandBuffer, isPreprocessed, pGeneratedCommandsInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdExecuteGeneratedCommandsNV(VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo) {
  ELIXIR_VK_HOST(vkCmdExecuteGeneratedCommandsNV)(commandBuffer, isPreprocessed, pGeneratedCommandsInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
  ELIXIR_VK_HOST(vkCmdFillBuffer)(commandBuffer, dstBuffer, dstOffset, size, data);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdFillMemoryKHR(VkCommandBuffer commandBuffer, const VkDeviceAddressRangeKHR* pDstRange, VkAddressCommandFlagsKHR dstFlags, uint32_t data) {
  ELIXIR_VK_HOST(vkCmdFillMemoryKHR)(commandBuffer, pDstRange, dstFlags, data);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdInitializeGraphScratchMemoryAMDX(VkCommandBuffer commandBuffer, VkPipeline executionGraph, VkDeviceAddress scratch, VkDeviceSize scratchSize) {
  ELIXIR_VK_HOST(vkCmdInitializeGraphScratchMemoryAMDX)(commandBuffer, executionGraph, scratch, scratchSize);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) {
  ELIXIR_VK_HOST(vkCmdInsertDebugUtilsLabelEXT)(commandBuffer, pLabelInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) {
  ELIXIR_VK_HOST(vkCmdNextSubpass)(commandBuffer, contents);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
  ELIXIR_VK_HOST(vkCmdNextSubpass2)(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdNextSubpass2KHR(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
  ELIXIR_VK_HOST(vkCmdNextSubpass2KHR)(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdOpticalFlowExecuteNV(VkCommandBuffer commandBuffer, VkOpticalFlowSessionNV session, const VkOpticalFlowExecuteInfoNV* pExecuteInfo) {
  ELIXIR_VK_HOST(vkCmdOpticalFlowExecuteNV)(commandBuffer, session, pExecuteInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
  ELIXIR_VK_HOST(vkCmdPipelineBarrier)(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
  ELIXIR_VK_HOST(vkCmdPipelineBarrier2)(commandBuffer, pDependencyInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPipelineBarrier2KHR(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
  ELIXIR_VK_HOST(vkCmdPipelineBarrier2KHR)(commandBuffer, pDependencyInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPreprocessGeneratedCommandsEXT(VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoEXT* pGeneratedCommandsInfo, VkCommandBuffer stateCommandBuffer) {
  ELIXIR_VK_HOST(vkCmdPreprocessGeneratedCommandsEXT)(commandBuffer, pGeneratedCommandsInfo, stateCommandBuffer);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPreprocessGeneratedCommandsNV(VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo) {
  ELIXIR_VK_HOST(vkCmdPreprocessGeneratedCommandsNV)(commandBuffer, pGeneratedCommandsInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
  ELIXIR_VK_HOST(vkCmdPushConstants)(commandBuffer, layout, stageFlags, offset, size, pValues);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushConstants2(VkCommandBuffer commandBuffer, const VkPushConstantsInfo* pPushConstantsInfo) {
  ELIXIR_VK_HOST(vkCmdPushConstants2)(commandBuffer, pPushConstantsInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushConstants2KHR(VkCommandBuffer commandBuffer, const VkPushConstantsInfo* pPushConstantsInfo) {
  ELIXIR_VK_HOST(vkCmdPushConstants2KHR)(commandBuffer, pPushConstantsInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushDataEXT(VkCommandBuffer commandBuffer, const VkPushDataInfoEXT* pPushDataInfo) {
  ELIXIR_VK_HOST(vkCmdPushDataEXT)(commandBuffer, pPushDataInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
  ELIXIR_VK_HOST(vkCmdPushDescriptorSet)(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushDescriptorSet2(VkCommandBuffer commandBuffer, const VkPushDescriptorSetInfo* pPushDescriptorSetInfo) {
  ELIXIR_VK_HOST(vkCmdPushDescriptorSet2)(commandBuffer, pPushDescriptorSetInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushDescriptorSet2KHR(VkCommandBuffer commandBuffer, const VkPushDescriptorSetInfo* pPushDescriptorSetInfo) {
  ELIXIR_VK_HOST(vkCmdPushDescriptorSet2KHR)(commandBuffer, pPushDescriptorSetInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
  ELIXIR_VK_HOST(vkCmdPushDescriptorSetKHR)(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplate(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) {
  ELIXIR_VK_HOST(vkCmdPushDescriptorSetWithTemplate)(commandBuffer, descriptorUpdateTemplate, layout, set, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplate2(VkCommandBuffer commandBuffer, const VkPushDescriptorSetWithTemplateInfo* pPushDescriptorSetWithTemplateInfo) {
  ELIXIR_VK_HOST(vkCmdPushDescriptorSetWithTemplate2)(commandBuffer, pPushDescriptorSetWithTemplateInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplate2KHR(VkCommandBuffer commandBuffer, const VkPushDescriptorSetWithTemplateInfo* pPushDescriptorSetWithTemplateInfo) {
  ELIXIR_VK_HOST(vkCmdPushDescriptorSetWithTemplate2KHR)(commandBuffer, pPushDescriptorSetWithTemplateInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) {
  ELIXIR_VK_HOST(vkCmdPushDescriptorSetWithTemplateKHR)(commandBuffer, descriptorUpdateTemplate, layout, set, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
  ELIXIR_VK_HOST(vkCmdResetEvent)(commandBuffer, event, stageMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
  ELIXIR_VK_HOST(vkCmdResetEvent2)(commandBuffer, event, stageMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdResetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
  ELIXIR_VK_HOST(vkCmdResetEvent2KHR)(commandBuffer, event, stageMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
  ELIXIR_VK_HOST(vkCmdResetQueryPool)(commandBuffer, queryPool, firstQuery, queryCount);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
  ELIXIR_VK_HOST(vkCmdResolveImage)(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
  ELIXIR_VK_HOST(vkCmdResolveImage2)(commandBuffer, pResolveImageInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdResolveImage2KHR(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
  ELIXIR_VK_HOST(vkCmdResolveImage2KHR)(commandBuffer, pResolveImageInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) {
  ELIXIR_VK_HOST(vkCmdSetAlphaToCoverageEnableEXT)(commandBuffer, alphaToCoverageEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) {
  ELIXIR_VK_HOST(vkCmdSetAlphaToOneEnableEXT)(commandBuffer, alphaToOneEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetAttachmentFeedbackLoopEnableEXT(VkCommandBuffer commandBuffer, VkImageAspectFlags aspectMask) {
  ELIXIR_VK_HOST(vkCmdSetAttachmentFeedbackLoopEnableEXT)(commandBuffer, aspectMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4]) {
  ELIXIR_VK_HOST(vkCmdSetBlendConstants)(commandBuffer, blendConstants);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCheckpointNV(VkCommandBuffer commandBuffer, const void* pCheckpointMarker) {
  ELIXIR_VK_HOST(vkCmdSetCheckpointNV)(commandBuffer, pCheckpointMarker);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCoarseSampleOrderNV(VkCommandBuffer commandBuffer, VkCoarseSampleOrderTypeNV sampleOrderType, uint32_t customSampleOrderCount, const VkCoarseSampleOrderCustomNV* pCustomSampleOrders) {
  ELIXIR_VK_HOST(vkCmdSetCoarseSampleOrderNV)(commandBuffer, sampleOrderType, customSampleOrderCount, pCustomSampleOrders);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) {
  ELIXIR_VK_HOST(vkCmdSetColorBlendAdvancedEXT)(commandBuffer, firstAttachment, attachmentCount, pColorBlendAdvanced);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) {
  ELIXIR_VK_HOST(vkCmdSetColorBlendEnableEXT)(commandBuffer, firstAttachment, attachmentCount, pColorBlendEnables);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) {
  ELIXIR_VK_HOST(vkCmdSetColorBlendEquationEXT)(commandBuffer, firstAttachment, attachmentCount, pColorBlendEquations);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) {
  ELIXIR_VK_HOST(vkCmdSetColorWriteEnableEXT)(commandBuffer, attachmentCount, pColorWriteEnables);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) {
  ELIXIR_VK_HOST(vkCmdSetColorWriteMaskEXT)(commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetComputeOccupancyPriorityNV(VkCommandBuffer commandBuffer, const VkComputeOccupancyPriorityParametersNV* pParameters) {
  ELIXIR_VK_HOST(vkCmdSetComputeOccupancyPriorityNV)(commandBuffer, pParameters);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) {
  ELIXIR_VK_HOST(vkCmdSetConservativeRasterizationModeEXT)(commandBuffer, conservativeRasterizationMode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCoverageModulationModeNV(VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) {
  ELIXIR_VK_HOST(vkCmdSetCoverageModulationModeNV)(commandBuffer, coverageModulationMode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) {
  ELIXIR_VK_HOST(vkCmdSetCoverageModulationTableEnableNV)(commandBuffer, coverageModulationTableEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCoverageModulationTableNV(VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) {
  ELIXIR_VK_HOST(vkCmdSetCoverageModulationTableNV)(commandBuffer, coverageModulationTableCount, pCoverageModulationTable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCoverageReductionModeNV(VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) {
  ELIXIR_VK_HOST(vkCmdSetCoverageReductionModeNV)(commandBuffer, coverageReductionMode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCoverageToColorEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) {
  ELIXIR_VK_HOST(vkCmdSetCoverageToColorEnableNV)(commandBuffer, coverageToColorEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCoverageToColorLocationNV(VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) {
  ELIXIR_VK_HOST(vkCmdSetCoverageToColorLocationNV)(commandBuffer, coverageToColorLocation);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
  ELIXIR_VK_HOST(vkCmdSetCullMode)(commandBuffer, cullMode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
  ELIXIR_VK_HOST(vkCmdSetCullModeEXT)(commandBuffer, cullMode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
  ELIXIR_VK_HOST(vkCmdSetDepthBias)(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthBias2EXT(VkCommandBuffer commandBuffer, const VkDepthBiasInfoEXT* pDepthBiasInfo) {
  ELIXIR_VK_HOST(vkCmdSetDepthBias2EXT)(commandBuffer, pDepthBiasInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthBiasEnable)(commandBuffer, depthBiasEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthBiasEnableEXT)(commandBuffer, depthBiasEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
  ELIXIR_VK_HOST(vkCmdSetDepthBounds)(commandBuffer, minDepthBounds, maxDepthBounds);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthBoundsTestEnable)(commandBuffer, depthBoundsTestEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthBoundsTestEnableEXT)(commandBuffer, depthBoundsTestEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthClampEnableEXT)(commandBuffer, depthClampEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthClampRangeEXT(VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT* pDepthClampRange) {
  ELIXIR_VK_HOST(vkCmdSetDepthClampRangeEXT)(commandBuffer, depthClampMode, pDepthClampRange);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthClipEnableEXT)(commandBuffer, depthClipEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) {
  ELIXIR_VK_HOST(vkCmdSetDepthClipNegativeOneToOneEXT)(commandBuffer, negativeOneToOne);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
  ELIXIR_VK_HOST(vkCmdSetDepthCompareOp)(commandBuffer, depthCompareOp);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
  ELIXIR_VK_HOST(vkCmdSetDepthCompareOpEXT)(commandBuffer, depthCompareOp);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthTestEnable)(commandBuffer, depthTestEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthTestEnableEXT)(commandBuffer, depthTestEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthWriteEnable)(commandBuffer, depthWriteEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
  ELIXIR_VK_HOST(vkCmdSetDepthWriteEnableEXT)(commandBuffer, depthWriteEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDescriptorBufferOffsets2EXT(VkCommandBuffer commandBuffer, const VkSetDescriptorBufferOffsetsInfoEXT* pSetDescriptorBufferOffsetsInfo) {
  ELIXIR_VK_HOST(vkCmdSetDescriptorBufferOffsets2EXT)(commandBuffer, pSetDescriptorBufferOffsetsInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDescriptorBufferOffsetsEXT(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const uint32_t* pBufferIndices, const VkDeviceSize* pOffsets) {
  ELIXIR_VK_HOST(vkCmdSetDescriptorBufferOffsetsEXT)(commandBuffer, pipelineBindPoint, layout, firstSet, setCount, pBufferIndices, pOffsets);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask) {
  ELIXIR_VK_HOST(vkCmdSetDeviceMask)(commandBuffer, deviceMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDeviceMaskKHR(VkCommandBuffer commandBuffer, uint32_t deviceMask) {
  ELIXIR_VK_HOST(vkCmdSetDeviceMaskKHR)(commandBuffer, deviceMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDiscardRectangleEXT(VkCommandBuffer commandBuffer, uint32_t firstDiscardRectangle, uint32_t discardRectangleCount, const VkRect2D* pDiscardRectangles) {
  ELIXIR_VK_HOST(vkCmdSetDiscardRectangleEXT)(commandBuffer, firstDiscardRectangle, discardRectangleCount, pDiscardRectangles);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDiscardRectangleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 discardRectangleEnable) {
  ELIXIR_VK_HOST(vkCmdSetDiscardRectangleEnableEXT)(commandBuffer, discardRectangleEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDiscardRectangleModeEXT(VkCommandBuffer commandBuffer, VkDiscardRectangleModeEXT discardRectangleMode) {
  ELIXIR_VK_HOST(vkCmdSetDiscardRectangleModeEXT)(commandBuffer, discardRectangleMode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetDispatchParametersARM(VkCommandBuffer commandBuffer, const VkDispatchParametersARM* pDispatchParameters) {
  ELIXIR_VK_HOST(vkCmdSetDispatchParametersARM)(commandBuffer, pDispatchParameters);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
  ELIXIR_VK_HOST(vkCmdSetEvent)(commandBuffer, event, stageMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
  ELIXIR_VK_HOST(vkCmdSetEvent2)(commandBuffer, event, pDependencyInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
  ELIXIR_VK_HOST(vkCmdSetEvent2KHR)(commandBuffer, event, pDependencyInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetExclusiveScissorEnableNV(VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkBool32* pExclusiveScissorEnables) {
  ELIXIR_VK_HOST(vkCmdSetExclusiveScissorEnableNV)(commandBuffer, firstExclusiveScissor, exclusiveScissorCount, pExclusiveScissorEnables);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetExclusiveScissorNV(VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkRect2D* pExclusiveScissors) {
  ELIXIR_VK_HOST(vkCmdSetExclusiveScissorNV)(commandBuffer, firstExclusiveScissor, exclusiveScissorCount, pExclusiveScissors);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) {
  ELIXIR_VK_HOST(vkCmdSetExtraPrimitiveOverestimationSizeEXT)(commandBuffer, extraPrimitiveOverestimationSize);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetFragmentShadingRateEnumNV(VkCommandBuffer commandBuffer, VkFragmentShadingRateNV shadingRate, const VkFragmentShadingRateCombinerOpKHR combinerOps[2]) {
  ELIXIR_VK_HOST(vkCmdSetFragmentShadingRateEnumNV)(commandBuffer, shadingRate, combinerOps);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetFragmentShadingRateKHR(VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR combinerOps[2]) {
  ELIXIR_VK_HOST(vkCmdSetFragmentShadingRateKHR)(commandBuffer, pFragmentSize, combinerOps);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
  ELIXIR_VK_HOST(vkCmdSetFrontFace)(commandBuffer, frontFace);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
  ELIXIR_VK_HOST(vkCmdSetFrontFaceEXT)(commandBuffer, frontFace);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) {
  ELIXIR_VK_HOST(vkCmdSetLineRasterizationModeEXT)(commandBuffer, lineRasterizationMode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetLineStipple(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
  ELIXIR_VK_HOST(vkCmdSetLineStipple)(commandBuffer, lineStippleFactor, lineStipplePattern);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
  ELIXIR_VK_HOST(vkCmdSetLineStippleEXT)(commandBuffer, lineStippleFactor, lineStipplePattern);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) {
  ELIXIR_VK_HOST(vkCmdSetLineStippleEnableEXT)(commandBuffer, stippledLineEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetLineStippleKHR(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
  ELIXIR_VK_HOST(vkCmdSetLineStippleKHR)(commandBuffer, lineStippleFactor, lineStipplePattern);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) {
  ELIXIR_VK_HOST(vkCmdSetLineWidth)(commandBuffer, lineWidth);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) {
  ELIXIR_VK_HOST(vkCmdSetLogicOpEXT)(commandBuffer, logicOp);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) {
  ELIXIR_VK_HOST(vkCmdSetLogicOpEnableEXT)(commandBuffer, logicOpEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints) {
  ELIXIR_VK_HOST(vkCmdSetPatchControlPointsEXT)(commandBuffer, patchControlPoints);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCmdSetPerformanceMarkerINTEL(VkCommandBuffer commandBuffer, const VkPerformanceMarkerInfoINTEL* pMarkerInfo) {
  return ELIXIR_VK_HOST(vkCmdSetPerformanceMarkerINTEL)(commandBuffer, pMarkerInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCmdSetPerformanceOverrideINTEL(VkCommandBuffer commandBuffer, const VkPerformanceOverrideInfoINTEL* pOverrideInfo) {
  return ELIXIR_VK_HOST(vkCmdSetPerformanceOverrideINTEL)(commandBuffer, pOverrideInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCmdSetPerformanceStreamMarkerINTEL(VkCommandBuffer commandBuffer, const VkPerformanceStreamMarkerInfoINTEL* pMarkerInfo) {
  return ELIXIR_VK_HOST(vkCmdSetPerformanceStreamMarkerINTEL)(commandBuffer, pMarkerInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) {
  ELIXIR_VK_HOST(vkCmdSetPolygonModeEXT)(commandBuffer, polygonMode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
  ELIXIR_VK_HOST(vkCmdSetPrimitiveRestartEnable)(commandBuffer, primitiveRestartEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
  ELIXIR_VK_HOST(vkCmdSetPrimitiveRestartEnableEXT)(commandBuffer, primitiveRestartEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetPrimitiveRestartIndexEXT(VkCommandBuffer commandBuffer, uint32_t primitiveRestartIndex) {
  ELIXIR_VK_HOST(vkCmdSetPrimitiveRestartIndexEXT)(commandBuffer, primitiveRestartIndex);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
  ELIXIR_VK_HOST(vkCmdSetPrimitiveTopology)(commandBuffer, primitiveTopology);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
  ELIXIR_VK_HOST(vkCmdSetPrimitiveTopologyEXT)(commandBuffer, primitiveTopology);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) {
  ELIXIR_VK_HOST(vkCmdSetProvokingVertexModeEXT)(commandBuffer, provokingVertexMode);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) {
  ELIXIR_VK_HOST(vkCmdSetRasterizationSamplesEXT)(commandBuffer, rasterizationSamples);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer, uint32_t rasterizationStream) {
  ELIXIR_VK_HOST(vkCmdSetRasterizationStreamEXT)(commandBuffer, rasterizationStream);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
  ELIXIR_VK_HOST(vkCmdSetRasterizerDiscardEnable)(commandBuffer, rasterizerDiscardEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
  ELIXIR_VK_HOST(vkCmdSetRasterizerDiscardEnableEXT)(commandBuffer, rasterizerDiscardEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRayTracingPipelineStackSizeKHR(VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) {
  ELIXIR_VK_HOST(vkCmdSetRayTracingPipelineStackSizeKHR)(commandBuffer, pipelineStackSize);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRenderingAttachmentLocations(VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo) {
  ELIXIR_VK_HOST(vkCmdSetRenderingAttachmentLocations)(commandBuffer, pLocationInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRenderingAttachmentLocationsKHR(VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo) {
  ELIXIR_VK_HOST(vkCmdSetRenderingAttachmentLocationsKHR)(commandBuffer, pLocationInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRenderingInputAttachmentIndices(VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo) {
  ELIXIR_VK_HOST(vkCmdSetRenderingInputAttachmentIndices)(commandBuffer, pInputAttachmentIndexInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRenderingInputAttachmentIndicesKHR(VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo) {
  ELIXIR_VK_HOST(vkCmdSetRenderingInputAttachmentIndicesKHR)(commandBuffer, pInputAttachmentIndexInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) {
  ELIXIR_VK_HOST(vkCmdSetRepresentativeFragmentTestEnableNV)(commandBuffer, representativeFragmentTestEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) {
  ELIXIR_VK_HOST(vkCmdSetSampleLocationsEXT)(commandBuffer, pSampleLocationsInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) {
  ELIXIR_VK_HOST(vkCmdSetSampleLocationsEnableEXT)(commandBuffer, sampleLocationsEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) {
  ELIXIR_VK_HOST(vkCmdSetSampleMaskEXT)(commandBuffer, samples, pSampleMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
  ELIXIR_VK_HOST(vkCmdSetScissor)(commandBuffer, firstScissor, scissorCount, pScissors);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
  ELIXIR_VK_HOST(vkCmdSetScissorWithCount)(commandBuffer, scissorCount, pScissors);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
  ELIXIR_VK_HOST(vkCmdSetScissorWithCountEXT)(commandBuffer, scissorCount, pScissors);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetShadingRateImageEnableNV(VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) {
  ELIXIR_VK_HOST(vkCmdSetShadingRateImageEnableNV)(commandBuffer, shadingRateImageEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
  ELIXIR_VK_HOST(vkCmdSetStencilCompareMask)(commandBuffer, faceMask, compareMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
  ELIXIR_VK_HOST(vkCmdSetStencilOp)(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
  ELIXIR_VK_HOST(vkCmdSetStencilOpEXT)(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
  ELIXIR_VK_HOST(vkCmdSetStencilReference)(commandBuffer, faceMask, reference);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
  ELIXIR_VK_HOST(vkCmdSetStencilTestEnable)(commandBuffer, stencilTestEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
  ELIXIR_VK_HOST(vkCmdSetStencilTestEnableEXT)(commandBuffer, stencilTestEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
  ELIXIR_VK_HOST(vkCmdSetStencilWriteMask)(commandBuffer, faceMask, writeMask);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) {
  ELIXIR_VK_HOST(vkCmdSetTessellationDomainOriginEXT)(commandBuffer, domainOrigin);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) {
  ELIXIR_VK_HOST(vkCmdSetVertexInputEXT)(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
  ELIXIR_VK_HOST(vkCmdSetViewport)(commandBuffer, firstViewport, viewportCount, pViewports);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetViewportShadingRatePaletteNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkShadingRatePaletteNV* pShadingRatePalettes) {
  ELIXIR_VK_HOST(vkCmdSetViewportShadingRatePaletteNV)(commandBuffer, firstViewport, viewportCount, pShadingRatePalettes);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetViewportSwizzleNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) {
  ELIXIR_VK_HOST(vkCmdSetViewportSwizzleNV)(commandBuffer, firstViewport, viewportCount, pViewportSwizzles);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetViewportWScalingEnableNV(VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) {
  ELIXIR_VK_HOST(vkCmdSetViewportWScalingEnableNV)(commandBuffer, viewportWScalingEnable);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetViewportWScalingNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportWScalingNV* pViewportWScalings) {
  ELIXIR_VK_HOST(vkCmdSetViewportWScalingNV)(commandBuffer, firstViewport, viewportCount, pViewportWScalings);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
  ELIXIR_VK_HOST(vkCmdSetViewportWithCount)(commandBuffer, viewportCount, pViewports);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
  ELIXIR_VK_HOST(vkCmdSetViewportWithCountEXT)(commandBuffer, viewportCount, pViewports);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdSubpassShadingHUAWEI(VkCommandBuffer commandBuffer) {
  ELIXIR_VK_HOST(vkCmdSubpassShadingHUAWEI)(commandBuffer);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdTraceRaysIndirect2KHR(VkCommandBuffer commandBuffer, VkDeviceAddress indirectDeviceAddress) {
  ELIXIR_VK_HOST(vkCmdTraceRaysIndirect2KHR)(commandBuffer, indirectDeviceAddress);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdTraceRaysIndirectKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) {
  ELIXIR_VK_HOST(vkCmdTraceRaysIndirectKHR)(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, indirectDeviceAddress);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) {
  ELIXIR_VK_HOST(vkCmdTraceRaysKHR)(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdTraceRaysNV(VkCommandBuffer commandBuffer, VkBuffer raygenShaderBindingTableBuffer, VkDeviceSize raygenShaderBindingOffset, VkBuffer missShaderBindingTableBuffer, VkDeviceSize missShaderBindingOffset, VkDeviceSize missShaderBindingStride, VkBuffer hitShaderBindingTableBuffer, VkDeviceSize hitShaderBindingOffset, VkDeviceSize hitShaderBindingStride, VkBuffer callableShaderBindingTableBuffer, VkDeviceSize callableShaderBindingOffset, VkDeviceSize callableShaderBindingStride, uint32_t width, uint32_t height, uint32_t depth) {
  ELIXIR_VK_HOST(vkCmdTraceRaysNV)(commandBuffer, raygenShaderBindingTableBuffer, raygenShaderBindingOffset, missShaderBindingTableBuffer, missShaderBindingOffset, missShaderBindingStride, hitShaderBindingTableBuffer, hitShaderBindingOffset, hitShaderBindingStride, callableShaderBindingTableBuffer, callableShaderBindingOffset, callableShaderBindingStride, width, height, depth);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
  ELIXIR_VK_HOST(vkCmdUpdateBuffer)(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdUpdateMemoryKHR(VkCommandBuffer commandBuffer, const VkDeviceAddressRangeKHR* pDstRange, VkAddressCommandFlagsKHR dstFlags, VkDeviceSize dataSize, const void* pData) {
  ELIXIR_VK_HOST(vkCmdUpdateMemoryKHR)(commandBuffer, pDstRange, dstFlags, dataSize, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdUpdatePipelineIndirectBufferNV(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
  ELIXIR_VK_HOST(vkCmdUpdatePipelineIndirectBufferNV)(commandBuffer, pipelineBindPoint, pipeline);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
  ELIXIR_VK_HOST(vkCmdWaitEvents)(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
  ELIXIR_VK_HOST(vkCmdWaitEvents2)(commandBuffer, eventCount, pEvents, pDependencyInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
  ELIXIR_VK_HOST(vkCmdWaitEvents2KHR)(commandBuffer, eventCount, pEvents, pDependencyInfos);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWriteAccelerationStructuresPropertiesKHR(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) {
  ELIXIR_VK_HOST(vkCmdWriteAccelerationStructuresPropertiesKHR)(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWriteAccelerationStructuresPropertiesNV(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureNV* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) {
  ELIXIR_VK_HOST(vkCmdWriteAccelerationStructuresPropertiesNV)(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWriteBufferMarker2AMD(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) {
  ELIXIR_VK_HOST(vkCmdWriteBufferMarker2AMD)(commandBuffer, stage, dstBuffer, dstOffset, marker);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWriteBufferMarkerAMD(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) {
  ELIXIR_VK_HOST(vkCmdWriteBufferMarkerAMD)(commandBuffer, pipelineStage, dstBuffer, dstOffset, marker);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWriteMarkerToMemoryAMD(VkCommandBuffer commandBuffer, const VkMemoryMarkerInfoAMD* pInfo) {
  ELIXIR_VK_HOST(vkCmdWriteMarkerToMemoryAMD)(commandBuffer, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWriteMicromapsPropertiesEXT(VkCommandBuffer commandBuffer, uint32_t micromapCount, const VkMicromapEXT* pMicromaps, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) {
  ELIXIR_VK_HOST(vkCmdWriteMicromapsPropertiesEXT)(commandBuffer, micromapCount, pMicromaps, queryType, queryPool, firstQuery);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
  ELIXIR_VK_HOST(vkCmdWriteTimestamp)(commandBuffer, pipelineStage, queryPool, query);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
  ELIXIR_VK_HOST(vkCmdWriteTimestamp2)(commandBuffer, stage, queryPool, query);
}

ELIXIR_SYSV void elixir_vk_sysv_vkCmdWriteTimestamp2KHR(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
  ELIXIR_VK_HOST(vkCmdWriteTimestamp2KHR)(commandBuffer, stage, queryPool, query);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCompileDeferredNV(VkDevice device, VkPipeline pipeline, uint32_t shader) {
  return ELIXIR_VK_HOST(vkCompileDeferredNV)(device, pipeline, shader);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkConvertCooperativeVectorMatrixNV(VkDevice device, const VkConvertCooperativeVectorMatrixInfoNV* pInfo) {
  return ELIXIR_VK_HOST(vkConvertCooperativeVectorMatrixNV)(device, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) {
  return ELIXIR_VK_HOST(vkCopyAccelerationStructureKHR)(device, deferredOperation, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyAccelerationStructureToMemoryKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
  return ELIXIR_VK_HOST(vkCopyAccelerationStructureToMemoryKHR)(device, deferredOperation, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyImageToImage(VkDevice device, const VkCopyImageToImageInfo* pCopyImageToImageInfo) {
  return ELIXIR_VK_HOST(vkCopyImageToImage)(device, pCopyImageToImageInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyImageToImageEXT(VkDevice device, const VkCopyImageToImageInfo* pCopyImageToImageInfo) {
  return ELIXIR_VK_HOST(vkCopyImageToImageEXT)(device, pCopyImageToImageInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyImageToMemory(VkDevice device, const VkCopyImageToMemoryInfo* pCopyImageToMemoryInfo) {
  return ELIXIR_VK_HOST(vkCopyImageToMemory)(device, pCopyImageToMemoryInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyImageToMemoryEXT(VkDevice device, const VkCopyImageToMemoryInfo* pCopyImageToMemoryInfo) {
  return ELIXIR_VK_HOST(vkCopyImageToMemoryEXT)(device, pCopyImageToMemoryInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyMemoryToAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
  return ELIXIR_VK_HOST(vkCopyMemoryToAccelerationStructureKHR)(device, deferredOperation, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyMemoryToImage(VkDevice device, const VkCopyMemoryToImageInfo* pCopyMemoryToImageInfo) {
  return ELIXIR_VK_HOST(vkCopyMemoryToImage)(device, pCopyMemoryToImageInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyMemoryToImageEXT(VkDevice device, const VkCopyMemoryToImageInfo* pCopyMemoryToImageInfo) {
  return ELIXIR_VK_HOST(vkCopyMemoryToImageEXT)(device, pCopyMemoryToImageInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyMemoryToMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToMicromapInfoEXT* pInfo) {
  return ELIXIR_VK_HOST(vkCopyMemoryToMicromapEXT)(device, deferredOperation, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMicromapInfoEXT* pInfo) {
  return ELIXIR_VK_HOST(vkCopyMicromapEXT)(device, deferredOperation, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCopyMicromapToMemoryEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMicromapToMemoryInfoEXT* pInfo) {
  return ELIXIR_VK_HOST(vkCopyMicromapToMemoryEXT)(device, deferredOperation, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateAccelerationStructure2KHR(VkDevice device, const VkAccelerationStructureCreateInfo2KHR* pCreateInfo, const VkAllocationCallbacks*, VkAccelerationStructureKHR* pAccelerationStructure) {
  return ELIXIR_VK_HOST(vkCreateAccelerationStructure2KHR)(device, pCreateInfo, nullptr, pAccelerationStructure);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks*, VkAccelerationStructureKHR* pAccelerationStructure) {
  return ELIXIR_VK_HOST(vkCreateAccelerationStructureKHR)(device, pCreateInfo, nullptr, pAccelerationStructure);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateAccelerationStructureNV(VkDevice device, const VkAccelerationStructureCreateInfoNV* pCreateInfo, const VkAllocationCallbacks*, VkAccelerationStructureNV* pAccelerationStructure) {
  return ELIXIR_VK_HOST(vkCreateAccelerationStructureNV)(device, pCreateInfo, nullptr, pAccelerationStructure);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkBuffer* pBuffer) {
  return ELIXIR_VK_HOST(vkCreateBuffer)(device, pCreateInfo, nullptr, pBuffer);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkBufferView* pView) {
  return ELIXIR_VK_HOST(vkCreateBufferView)(device, pCreateInfo, nullptr, pView);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkCommandPool* pCommandPool) {
  return ELIXIR_VK_HOST(vkCreateCommandPool)(device, pCreateInfo, nullptr, pCommandPool);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) {
  return ELIXIR_VK_HOST(vkCreateComputePipelines)(device, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateCuFunctionNVX(VkDevice device, const VkCuFunctionCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks*, VkCuFunctionNVX* pFunction) {
  return ELIXIR_VK_HOST(vkCreateCuFunctionNVX)(device, pCreateInfo, nullptr, pFunction);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateCuModuleNVX(VkDevice device, const VkCuModuleCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks*, VkCuModuleNVX* pModule) {
  return ELIXIR_VK_HOST(vkCreateCuModuleNVX)(device, pCreateInfo, nullptr, pModule);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateCudaFunctionNV(VkDevice device, const VkCudaFunctionCreateInfoNV* pCreateInfo, const VkAllocationCallbacks*, VkCudaFunctionNV* pFunction) {
  return ELIXIR_VK_HOST(vkCreateCudaFunctionNV)(device, pCreateInfo, nullptr, pFunction);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateCudaModuleNV(VkDevice device, const VkCudaModuleCreateInfoNV* pCreateInfo, const VkAllocationCallbacks*, VkCudaModuleNV* pModule) {
  return ELIXIR_VK_HOST(vkCreateCudaModuleNV)(device, pCreateInfo, nullptr, pModule);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateDataGraphPipelineSessionARM(VkDevice device, const VkDataGraphPipelineSessionCreateInfoARM* pCreateInfo, const VkAllocationCallbacks*, VkDataGraphPipelineSessionARM* pSession) {
  return ELIXIR_VK_HOST(vkCreateDataGraphPipelineSessionARM)(device, pCreateInfo, nullptr, pSession);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateDataGraphPipelinesARM(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkDataGraphPipelineCreateInfoARM* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) {
  return ELIXIR_VK_HOST(vkCreateDataGraphPipelinesARM)(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateDeferredOperationKHR(VkDevice device, const VkAllocationCallbacks*, VkDeferredOperationKHR* pDeferredOperation) {
  return ELIXIR_VK_HOST(vkCreateDeferredOperationKHR)(device, nullptr, pDeferredOperation);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDescriptorPool* pDescriptorPool) {
  return ELIXIR_VK_HOST(vkCreateDescriptorPool)(device, pCreateInfo, nullptr, pDescriptorPool);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDescriptorSetLayout* pSetLayout) {
  return ELIXIR_VK_HOST(vkCreateDescriptorSetLayout)(device, pCreateInfo, nullptr, pSetLayout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
  return ELIXIR_VK_HOST(vkCreateDescriptorUpdateTemplate)(device, pCreateInfo, nullptr, pDescriptorUpdateTemplate);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateDescriptorUpdateTemplateKHR(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
  return ELIXIR_VK_HOST(vkCreateDescriptorUpdateTemplateKHR)(device, pCreateInfo, nullptr, pDescriptorUpdateTemplate);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateDisplayModeKHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display, const VkDisplayModeCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks*, VkDisplayModeKHR* pMode) {
  return ELIXIR_VK_HOST(vkCreateDisplayModeKHR)(physicalDevice, display, pCreateInfo, nullptr, pMode);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateDisplayPlaneSurfaceKHR(VkInstance instance, const VkDisplaySurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks*, VkSurfaceKHR* pSurface) {
  return ELIXIR_VK_HOST(vkCreateDisplayPlaneSurfaceKHR)(instance, pCreateInfo, nullptr, pSurface);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkEvent* pEvent) {
  return ELIXIR_VK_HOST(vkCreateEvent)(device, pCreateInfo, nullptr, pEvent);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateExecutionGraphPipelinesAMDX(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkExecutionGraphPipelineCreateInfoAMDX* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) {
  return ELIXIR_VK_HOST(vkCreateExecutionGraphPipelinesAMDX)(device, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateExternalComputeQueueNV(VkDevice device, const VkExternalComputeQueueCreateInfoNV* pCreateInfo, const VkAllocationCallbacks*, VkExternalComputeQueueNV* pExternalQueue) {
  return ELIXIR_VK_HOST(vkCreateExternalComputeQueueNV)(device, pCreateInfo, nullptr, pExternalQueue);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkFence* pFence) {
  return ELIXIR_VK_HOST(vkCreateFence)(device, pCreateInfo, nullptr, pFence);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkFramebuffer* pFramebuffer) {
  return ELIXIR_VK_HOST(vkCreateFramebuffer)(device, pCreateInfo, nullptr, pFramebuffer);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateGpaSessionAMD(VkDevice device, const VkGpaSessionCreateInfoAMD* pCreateInfo, const VkAllocationCallbacks*, VkGpaSessionAMD* pGpaSession) {
  return ELIXIR_VK_HOST(vkCreateGpaSessionAMD)(device, pCreateInfo, nullptr, pGpaSession);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) {
  return ELIXIR_VK_HOST(vkCreateGraphicsPipelines)(device, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateHeadlessSurfaceEXT(VkInstance instance, const VkHeadlessSurfaceCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks*, VkSurfaceKHR* pSurface) {
  return ELIXIR_VK_HOST(vkCreateHeadlessSurfaceEXT)(instance, pCreateInfo, nullptr, pSurface);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateIndirectCommandsLayoutEXT(VkDevice device, const VkIndirectCommandsLayoutCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks*, VkIndirectCommandsLayoutEXT* pIndirectCommandsLayout) {
  return ELIXIR_VK_HOST(vkCreateIndirectCommandsLayoutEXT)(device, pCreateInfo, nullptr, pIndirectCommandsLayout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateIndirectCommandsLayoutNV(VkDevice device, const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo, const VkAllocationCallbacks*, VkIndirectCommandsLayoutNV* pIndirectCommandsLayout) {
  return ELIXIR_VK_HOST(vkCreateIndirectCommandsLayoutNV)(device, pCreateInfo, nullptr, pIndirectCommandsLayout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateIndirectExecutionSetEXT(VkDevice device, const VkIndirectExecutionSetCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks*, VkIndirectExecutionSetEXT* pIndirectExecutionSet) {
  return ELIXIR_VK_HOST(vkCreateIndirectExecutionSetEXT)(device, pCreateInfo, nullptr, pIndirectExecutionSet);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateMicromapEXT(VkDevice device, const VkMicromapCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks*, VkMicromapEXT* pMicromap) {
  return ELIXIR_VK_HOST(vkCreateMicromapEXT)(device, pCreateInfo, nullptr, pMicromap);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateOpticalFlowSessionNV(VkDevice device, const VkOpticalFlowSessionCreateInfoNV* pCreateInfo, const VkAllocationCallbacks*, VkOpticalFlowSessionNV* pSession) {
  return ELIXIR_VK_HOST(vkCreateOpticalFlowSessionNV)(device, pCreateInfo, nullptr, pSession);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreatePipelineBinariesKHR(VkDevice device, const VkPipelineBinaryCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks*, VkPipelineBinaryHandlesInfoKHR* pBinaries) {
  return ELIXIR_VK_HOST(vkCreatePipelineBinariesKHR)(device, pCreateInfo, nullptr, pBinaries);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkPipelineCache* pPipelineCache) {
  return ELIXIR_VK_HOST(vkCreatePipelineCache)(device, pCreateInfo, nullptr, pPipelineCache);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkPipelineLayout* pPipelineLayout) {
  return ELIXIR_VK_HOST(vkCreatePipelineLayout)(device, pCreateInfo, nullptr, pPipelineLayout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkPrivateDataSlot* pPrivateDataSlot) {
  return ELIXIR_VK_HOST(vkCreatePrivateDataSlot)(device, pCreateInfo, nullptr, pPrivateDataSlot);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreatePrivateDataSlotEXT(VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkPrivateDataSlot* pPrivateDataSlot) {
  return ELIXIR_VK_HOST(vkCreatePrivateDataSlotEXT)(device, pCreateInfo, nullptr, pPrivateDataSlot);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkQueryPool* pQueryPool) {
  return ELIXIR_VK_HOST(vkCreateQueryPool)(device, pCreateInfo, nullptr, pQueryPool);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) {
  return ELIXIR_VK_HOST(vkCreateRayTracingPipelinesKHR)(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateRayTracingPipelinesNV(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoNV* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) {
  return ELIXIR_VK_HOST(vkCreateRayTracingPipelinesNV)(device, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkRenderPass* pRenderPass) {
  return ELIXIR_VK_HOST(vkCreateRenderPass)(device, pCreateInfo, nullptr, pRenderPass);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks*, VkRenderPass* pRenderPass) {
  return ELIXIR_VK_HOST(vkCreateRenderPass2)(device, pCreateInfo, nullptr, pRenderPass);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateRenderPass2KHR(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks*, VkRenderPass* pRenderPass) {
  return ELIXIR_VK_HOST(vkCreateRenderPass2KHR)(device, pCreateInfo, nullptr, pRenderPass);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkSampler* pSampler) {
  return ELIXIR_VK_HOST(vkCreateSampler)(device, pCreateInfo, nullptr, pSampler);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateSamplerYcbcrConversion(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkSamplerYcbcrConversion* pYcbcrConversion) {
  return ELIXIR_VK_HOST(vkCreateSamplerYcbcrConversion)(device, pCreateInfo, nullptr, pYcbcrConversion);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateSamplerYcbcrConversionKHR(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkSamplerYcbcrConversion* pYcbcrConversion) {
  return ELIXIR_VK_HOST(vkCreateSamplerYcbcrConversionKHR)(device, pCreateInfo, nullptr, pYcbcrConversion);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkSemaphore* pSemaphore) {
  return ELIXIR_VK_HOST(vkCreateSemaphore)(device, pCreateInfo, nullptr, pSemaphore);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateShaderInstrumentationARM(VkDevice device, const VkShaderInstrumentationCreateInfoARM* pCreateInfo, const VkAllocationCallbacks*, VkShaderInstrumentationARM* pInstrumentation) {
  return ELIXIR_VK_HOST(vkCreateShaderInstrumentationARM)(device, pCreateInfo, nullptr, pInstrumentation);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkShaderModule* pShaderModule) {
  return ELIXIR_VK_HOST(vkCreateShaderModule)(device, pCreateInfo, nullptr, pShaderModule);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateShadersEXT(VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT* pCreateInfos, const VkAllocationCallbacks*, VkShaderEXT* pShaders) {
  return ELIXIR_VK_HOST(vkCreateShadersEXT)(device, createInfoCount, pCreateInfos, nullptr, pShaders);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateSharedSwapchainsKHR(VkDevice device, uint32_t swapchainCount, const VkSwapchainCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks*, VkSwapchainKHR* pSwapchains) {
  return ELIXIR_VK_HOST(vkCreateSharedSwapchainsKHR)(device, swapchainCount, pCreateInfos, nullptr, pSwapchains);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateTensorARM(VkDevice device, const VkTensorCreateInfoARM* pCreateInfo, const VkAllocationCallbacks*, VkTensorARM* pTensor) {
  return ELIXIR_VK_HOST(vkCreateTensorARM)(device, pCreateInfo, nullptr, pTensor);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateTensorViewARM(VkDevice device, const VkTensorViewCreateInfoARM* pCreateInfo, const VkAllocationCallbacks*, VkTensorViewARM* pView) {
  return ELIXIR_VK_HOST(vkCreateTensorViewARM)(device, pCreateInfo, nullptr, pView);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateValidationCacheEXT(VkDevice device, const VkValidationCacheCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks*, VkValidationCacheEXT* pValidationCache) {
  return ELIXIR_VK_HOST(vkCreateValidationCacheEXT)(device, pCreateInfo, nullptr, pValidationCache);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateVideoSessionKHR(VkDevice device, const VkVideoSessionCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks*, VkVideoSessionKHR* pVideoSession) {
  return ELIXIR_VK_HOST(vkCreateVideoSessionKHR)(device, pCreateInfo, nullptr, pVideoSession);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkCreateVideoSessionParametersKHR(VkDevice device, const VkVideoSessionParametersCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks*, VkVideoSessionParametersKHR* pVideoSessionParameters) {
  return ELIXIR_VK_HOST(vkCreateVideoSessionParametersKHR)(device, pCreateInfo, nullptr, pVideoSessionParameters);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkDebugMarkerSetObjectNameEXT(VkDevice device, const VkDebugMarkerObjectNameInfoEXT* pNameInfo) {
  return ELIXIR_VK_HOST(vkDebugMarkerSetObjectNameEXT)(device, pNameInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkDebugMarkerSetObjectTagEXT(VkDevice device, const VkDebugMarkerObjectTagInfoEXT* pTagInfo) {
  return ELIXIR_VK_HOST(vkDebugMarkerSetObjectTagEXT)(device, pTagInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDebugReportMessageEXT(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage) {
  ELIXIR_VK_HOST(vkDebugReportMessageEXT)(instance, flags, objectType, object, location, messageCode, pLayerPrefix, pMessage);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkDeferredOperationJoinKHR(VkDevice device, VkDeferredOperationKHR operation) {
  return ELIXIR_VK_HOST(vkDeferredOperationJoinKHR)(device, operation);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyAccelerationStructureKHR)(device, accelerationStructure, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyAccelerationStructureNV(VkDevice device, VkAccelerationStructureNV accelerationStructure, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyAccelerationStructureNV)(device, accelerationStructure, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyBufferView)(device, bufferView, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyCuFunctionNVX(VkDevice device, VkCuFunctionNVX function, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyCuFunctionNVX)(device, function, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyCuModuleNVX(VkDevice device, VkCuModuleNVX module, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyCuModuleNVX)(device, module, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyCudaFunctionNV(VkDevice device, VkCudaFunctionNV function, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyCudaFunctionNV)(device, function, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyCudaModuleNV(VkDevice device, VkCudaModuleNV module, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyCudaModuleNV)(device, module, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyDataGraphPipelineSessionARM(VkDevice device, VkDataGraphPipelineSessionARM session, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyDataGraphPipelineSessionARM)(device, session, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyDeferredOperationKHR(VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyDeferredOperationKHR)(device, operation, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyDescriptorPool)(device, descriptorPool, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyDescriptorSetLayout)(device, descriptorSetLayout, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyDescriptorUpdateTemplate)(device, descriptorUpdateTemplate, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyDescriptorUpdateTemplateKHR(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyDescriptorUpdateTemplateKHR)(device, descriptorUpdateTemplate, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyEvent)(device, event, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyExternalComputeQueueNV(VkDevice device, VkExternalComputeQueueNV externalQueue, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyExternalComputeQueueNV)(device, externalQueue, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyFence)(device, fence, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyFramebuffer)(device, framebuffer, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyGpaSessionAMD(VkDevice device, VkGpaSessionAMD gpaSession, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyGpaSessionAMD)(device, gpaSession, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyImageView)(device, imageView, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyIndirectCommandsLayoutEXT(VkDevice device, VkIndirectCommandsLayoutEXT indirectCommandsLayout, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyIndirectCommandsLayoutEXT)(device, indirectCommandsLayout, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyIndirectCommandsLayoutNV(VkDevice device, VkIndirectCommandsLayoutNV indirectCommandsLayout, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyIndirectCommandsLayoutNV)(device, indirectCommandsLayout, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyIndirectExecutionSetEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyIndirectExecutionSetEXT)(device, indirectExecutionSet, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyMicromapEXT(VkDevice device, VkMicromapEXT micromap, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyMicromapEXT)(device, micromap, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyOpticalFlowSessionNV(VkDevice device, VkOpticalFlowSessionNV session, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyOpticalFlowSessionNV)(device, session, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyPipeline)(device, pipeline, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyPipelineBinaryKHR(VkDevice device, VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyPipelineBinaryKHR)(device, pipelineBinary, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyPipelineCache)(device, pipelineCache, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyPipelineLayout)(device, pipelineLayout, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyPrivateDataSlot)(device, privateDataSlot, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyPrivateDataSlotEXT(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyPrivateDataSlotEXT)(device, privateDataSlot, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyQueryPool)(device, queryPool, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyRenderPass)(device, renderPass, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroySampler)(device, sampler, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroySamplerYcbcrConversion)(device, ycbcrConversion, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroySamplerYcbcrConversionKHR(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroySamplerYcbcrConversionKHR)(device, ycbcrConversion, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroySemaphore)(device, semaphore, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyShaderEXT(VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyShaderEXT)(device, shader, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyShaderInstrumentationARM(VkDevice device, VkShaderInstrumentationARM instrumentation, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyShaderInstrumentationARM)(device, instrumentation, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyShaderModule)(device, shaderModule, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroySurfaceKHR)(instance, surface, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroySwapchainKHR)(device, swapchain, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyTensorARM(VkDevice device, VkTensorARM tensor, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyTensorARM)(device, tensor, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyTensorViewARM(VkDevice device, VkTensorViewARM tensorView, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyTensorViewARM)(device, tensorView, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyValidationCacheEXT(VkDevice device, VkValidationCacheEXT validationCache, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyValidationCacheEXT)(device, validationCache, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyVideoSessionKHR(VkDevice device, VkVideoSessionKHR videoSession, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyVideoSessionKHR)(device, videoSession, nullptr);
}

ELIXIR_SYSV void elixir_vk_sysv_vkDestroyVideoSessionParametersKHR(VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkAllocationCallbacks*) {
  ELIXIR_VK_HOST(vkDestroyVideoSessionParametersKHR)(device, videoSessionParameters, nullptr);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkDeviceWaitIdle(VkDevice device) {
  return ELIXIR_VK_HOST(vkDeviceWaitIdle)(device);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkDisplayPowerControlEXT(VkDevice device, VkDisplayKHR display, const VkDisplayPowerInfoEXT* pDisplayPowerInfo) {
  return ELIXIR_VK_HOST(vkDisplayPowerControlEXT)(device, display, pDisplayPowerInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
  return ELIXIR_VK_HOST(vkEndCommandBuffer)(commandBuffer);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
  return ELIXIR_VK_HOST(vkEnumerateDeviceLayerProperties)(physicalDevice, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkEnumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t* pCounterCount, VkPerformanceCounterARM* pCounters, VkPerformanceCounterDescriptionARM* pCounterDescriptions) {
  return ELIXIR_VK_HOST(vkEnumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM)(physicalDevice, queueFamilyIndex, pCounterCount, pCounters, pCounterDescriptions);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t* pCounterCount, VkPerformanceCounterKHR* pCounters, VkPerformanceCounterDescriptionKHR* pCounterDescriptions) {
  return ELIXIR_VK_HOST(vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR)(physicalDevice, queueFamilyIndex, pCounterCount, pCounters, pCounterDescriptions);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkEnumeratePhysicalDeviceShaderInstrumentationMetricsARM(VkPhysicalDevice physicalDevice, uint32_t* pDescriptionCount, VkShaderInstrumentationMetricDescriptionARM* pDescriptions) {
  return ELIXIR_VK_HOST(vkEnumeratePhysicalDeviceShaderInstrumentationMetricsARM)(physicalDevice, pDescriptionCount, pDescriptions);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
  return ELIXIR_VK_HOST(vkFlushMappedMemoryRanges)(device, memoryRangeCount, pMemoryRanges);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
  return ELIXIR_VK_HOST(vkFreeDescriptorSets)(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) {
  ELIXIR_VK_HOST(vkGetAccelerationStructureBuildSizesKHR)(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

ELIXIR_SYSV VkDeviceAddress elixir_vk_sysv_vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) {
  return ELIXIR_VK_HOST(vkGetAccelerationStructureDeviceAddressKHR)(device, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetAccelerationStructureHandleNV(VkDevice device, VkAccelerationStructureNV accelerationStructure, size_t dataSize, void* pData) {
  return ELIXIR_VK_HOST(vkGetAccelerationStructureHandleNV)(device, accelerationStructure, dataSize, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetAccelerationStructureMemoryRequirementsNV(VkDevice device, const VkAccelerationStructureMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetAccelerationStructureMemoryRequirementsNV)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkAccelerationStructureCaptureDescriptorDataInfoEXT* pInfo, void* pData) {
  return ELIXIR_VK_HOST(vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData);
}

ELIXIR_SYSV VkDeviceAddress elixir_vk_sysv_vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
  return ELIXIR_VK_HOST(vkGetBufferDeviceAddress)(device, pInfo);
}

ELIXIR_SYSV VkDeviceAddress elixir_vk_sysv_vkGetBufferDeviceAddressEXT(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
  return ELIXIR_VK_HOST(vkGetBufferDeviceAddressEXT)(device, pInfo);
}

ELIXIR_SYSV VkDeviceAddress elixir_vk_sysv_vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
  return ELIXIR_VK_HOST(vkGetBufferDeviceAddressKHR)(device, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetBufferMemoryRequirements)(device, buffer, pMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetBufferMemoryRequirements2)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetBufferMemoryRequirements2KHR)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV uint64_t elixir_vk_sysv_vkGetBufferOpaqueCaptureAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
  return ELIXIR_VK_HOST(vkGetBufferOpaqueCaptureAddress)(device, pInfo);
}

ELIXIR_SYSV uint64_t elixir_vk_sysv_vkGetBufferOpaqueCaptureAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
  return ELIXIR_VK_HOST(vkGetBufferOpaqueCaptureAddressKHR)(device, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetBufferOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkBufferCaptureDescriptorDataInfoEXT* pInfo, void* pData) {
  return ELIXIR_VK_HOST(vkGetBufferOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) {
  return ELIXIR_VK_HOST(vkGetCalibratedTimestampsEXT)(device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetCalibratedTimestampsKHR(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) {
  return ELIXIR_VK_HOST(vkGetCalibratedTimestampsKHR)(device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetClusterAccelerationStructureBuildSizesNV(VkDevice device, const VkClusterAccelerationStructureInputInfoNV* pInfo, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) {
  ELIXIR_VK_HOST(vkGetClusterAccelerationStructureBuildSizesNV)(device, pInfo, pSizeInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetCudaModuleCacheNV(VkDevice device, VkCudaModuleNV module, size_t* pCacheSize, void* pCacheData) {
  return ELIXIR_VK_HOST(vkGetCudaModuleCacheNV)(device, module, pCacheSize, pCacheData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDataGraphPipelineAvailablePropertiesARM(VkDevice device, const VkDataGraphPipelineInfoARM* pPipelineInfo, uint32_t* pPropertiesCount, VkDataGraphPipelinePropertyARM* pProperties) {
  return ELIXIR_VK_HOST(vkGetDataGraphPipelineAvailablePropertiesARM)(device, pPipelineInfo, pPropertiesCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDataGraphPipelinePropertiesARM(VkDevice device, const VkDataGraphPipelineInfoARM* pPipelineInfo, uint32_t propertiesCount, VkDataGraphPipelinePropertyQueryResultARM* pProperties) {
  return ELIXIR_VK_HOST(vkGetDataGraphPipelinePropertiesARM)(device, pPipelineInfo, propertiesCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDataGraphPipelineSessionBindPointRequirementsARM(VkDevice device, const VkDataGraphPipelineSessionBindPointRequirementsInfoARM* pInfo, uint32_t* pBindPointRequirementCount, VkDataGraphPipelineSessionBindPointRequirementARM* pBindPointRequirements) {
  return ELIXIR_VK_HOST(vkGetDataGraphPipelineSessionBindPointRequirementsARM)(device, pInfo, pBindPointRequirementCount, pBindPointRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDataGraphPipelineSessionMemoryRequirementsARM(VkDevice device, const VkDataGraphPipelineSessionMemoryRequirementsInfoARM* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetDataGraphPipelineSessionMemoryRequirementsARM)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV uint32_t elixir_vk_sysv_vkGetDeferredOperationMaxConcurrencyKHR(VkDevice device, VkDeferredOperationKHR operation) {
  return ELIXIR_VK_HOST(vkGetDeferredOperationMaxConcurrencyKHR)(device, operation);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDeferredOperationResultKHR(VkDevice device, VkDeferredOperationKHR operation) {
  return ELIXIR_VK_HOST(vkGetDeferredOperationResultKHR)(device, operation);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDescriptorEXT(VkDevice device, const VkDescriptorGetInfoEXT* pDescriptorInfo, size_t dataSize, void* pDescriptor) {
  ELIXIR_VK_HOST(vkGetDescriptorEXT)(device, pDescriptorInfo, dataSize, pDescriptor);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDescriptorSetHostMappingVALVE(VkDevice device, VkDescriptorSet descriptorSet, void** ppData) {
  ELIXIR_VK_HOST(vkGetDescriptorSetHostMappingVALVE)(device, descriptorSet, ppData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDescriptorSetLayoutBindingOffsetEXT(VkDevice device, VkDescriptorSetLayout layout, uint32_t binding, VkDeviceSize* pOffset) {
  ELIXIR_VK_HOST(vkGetDescriptorSetLayoutBindingOffsetEXT)(device, layout, binding, pOffset);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDescriptorSetLayoutHostMappingInfoVALVE(VkDevice device, const VkDescriptorSetBindingReferenceVALVE* pBindingReference, VkDescriptorSetLayoutHostMappingInfoVALVE* pHostMapping) {
  ELIXIR_VK_HOST(vkGetDescriptorSetLayoutHostMappingInfoVALVE)(device, pBindingReference, pHostMapping);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDescriptorSetLayoutSizeEXT(VkDevice device, VkDescriptorSetLayout layout, VkDeviceSize* pLayoutSizeInBytes) {
  ELIXIR_VK_HOST(vkGetDescriptorSetLayoutSizeEXT)(device, layout, pLayoutSizeInBytes);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDescriptorSetLayoutSupport(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
  ELIXIR_VK_HOST(vkGetDescriptorSetLayoutSupport)(device, pCreateInfo, pSupport);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDescriptorSetLayoutSupportKHR(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
  ELIXIR_VK_HOST(vkGetDescriptorSetLayoutSupportKHR)(device, pCreateInfo, pSupport);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceAccelerationStructureCompatibilityKHR(VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) {
  ELIXIR_VK_HOST(vkGetDeviceAccelerationStructureCompatibilityKHR)(device, pVersionInfo, pCompatibility);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetDeviceBufferMemoryRequirements)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetDeviceBufferMemoryRequirementsKHR)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV uint64_t elixir_vk_sysv_vkGetDeviceCombinedImageSamplerIndexNVX(VkDevice device, uint64_t imageViewIndex, uint64_t samplerIndex) {
  return ELIXIR_VK_HOST(vkGetDeviceCombinedImageSamplerIndexNVX)(device, imageViewIndex, samplerIndex);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDeviceFaultDebugInfoKHR(VkDevice device, VkDeviceFaultDebugInfoKHR* pDebugInfo) {
  return ELIXIR_VK_HOST(vkGetDeviceFaultDebugInfoKHR)(device, pDebugInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDeviceFaultInfoEXT(VkDevice device, VkDeviceFaultCountsEXT* pFaultCounts, VkDeviceFaultInfoEXT* pFaultInfo) {
  return ELIXIR_VK_HOST(vkGetDeviceFaultInfoEXT)(device, pFaultCounts, pFaultInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDeviceFaultReportsKHR(VkDevice device, uint64_t timeout, uint32_t* pFaultCounts, VkDeviceFaultInfoKHR* pFaultInfo) {
  return ELIXIR_VK_HOST(vkGetDeviceFaultReportsKHR)(device, timeout, pFaultCounts, pFaultInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
  ELIXIR_VK_HOST(vkGetDeviceGroupPeerMemoryFeatures)(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceGroupPeerMemoryFeaturesKHR(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
  ELIXIR_VK_HOST(vkGetDeviceGroupPeerMemoryFeaturesKHR)(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDeviceGroupPresentCapabilitiesKHR(VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) {
  return ELIXIR_VK_HOST(vkGetDeviceGroupPresentCapabilitiesKHR)(device, pDeviceGroupPresentCapabilities);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) {
  return ELIXIR_VK_HOST(vkGetDeviceGroupSurfacePresentModesKHR)(device, surface, pModes);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
  ELIXIR_VK_HOST(vkGetDeviceMemoryCommitment)(device, memory, pCommittedMemoryInBytes);
}

ELIXIR_SYSV uint64_t elixir_vk_sysv_vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
  return ELIXIR_VK_HOST(vkGetDeviceMemoryOpaqueCaptureAddress)(device, pInfo);
}

ELIXIR_SYSV uint64_t elixir_vk_sysv_vkGetDeviceMemoryOpaqueCaptureAddressKHR(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
  return ELIXIR_VK_HOST(vkGetDeviceMemoryOpaqueCaptureAddressKHR)(device, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceMicromapCompatibilityEXT(VkDevice device, const VkMicromapVersionInfoEXT* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) {
  ELIXIR_VK_HOST(vkGetDeviceMicromapCompatibilityEXT)(device, pVersionInfo, pCompatibility);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
  ELIXIR_VK_HOST(vkGetDeviceQueue)(device, queueFamilyIndex, queueIndex, pQueue);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) {
  ELIXIR_VK_HOST(vkGetDeviceQueue2)(device, pQueueInfo, pQueue);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI(VkDevice device, VkRenderPass renderpass, VkExtent2D* pMaxWorkgroupSize) {
  return ELIXIR_VK_HOST(vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI)(device, renderpass, pMaxWorkgroupSize);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetDeviceTensorMemoryRequirementsARM(VkDevice device, const VkDeviceTensorMemoryRequirementsARM* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetDeviceTensorMemoryRequirementsARM)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDisplayModeProperties2KHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display, uint32_t* pPropertyCount, VkDisplayModeProperties2KHR* pProperties) {
  return ELIXIR_VK_HOST(vkGetDisplayModeProperties2KHR)(physicalDevice, display, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDisplayModePropertiesKHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display, uint32_t* pPropertyCount, VkDisplayModePropertiesKHR* pProperties) {
  return ELIXIR_VK_HOST(vkGetDisplayModePropertiesKHR)(physicalDevice, display, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDisplayPlaneCapabilities2KHR(VkPhysicalDevice physicalDevice, const VkDisplayPlaneInfo2KHR* pDisplayPlaneInfo, VkDisplayPlaneCapabilities2KHR* pCapabilities) {
  return ELIXIR_VK_HOST(vkGetDisplayPlaneCapabilities2KHR)(physicalDevice, pDisplayPlaneInfo, pCapabilities);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDisplayPlaneCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkDisplayModeKHR mode, uint32_t planeIndex, VkDisplayPlaneCapabilitiesKHR* pCapabilities) {
  return ELIXIR_VK_HOST(vkGetDisplayPlaneCapabilitiesKHR)(physicalDevice, mode, planeIndex, pCapabilities);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDisplayPlaneSupportedDisplaysKHR(VkPhysicalDevice physicalDevice, uint32_t planeIndex, uint32_t* pDisplayCount, VkDisplayKHR* pDisplays) {
  return ELIXIR_VK_HOST(vkGetDisplayPlaneSupportedDisplaysKHR)(physicalDevice, planeIndex, pDisplayCount, pDisplays);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDrmDisplayEXT(VkPhysicalDevice physicalDevice, int32_t drmFd, uint32_t connectorId, VkDisplayKHR* display) {
  return ELIXIR_VK_HOST(vkGetDrmDisplayEXT)(physicalDevice, drmFd, connectorId, display);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetDynamicRenderingTilePropertiesQCOM(VkDevice device, const VkRenderingInfo* pRenderingInfo, VkTilePropertiesQCOM* pProperties) {
  return ELIXIR_VK_HOST(vkGetDynamicRenderingTilePropertiesQCOM)(device, pRenderingInfo, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetEncodedVideoSessionParametersKHR(VkDevice device, const VkVideoEncodeSessionParametersGetInfoKHR* pVideoSessionParametersInfo, VkVideoEncodeSessionParametersFeedbackInfoKHR* pFeedbackInfo, size_t* pDataSize, void* pData) {
  return ELIXIR_VK_HOST(vkGetEncodedVideoSessionParametersKHR)(device, pVideoSessionParametersInfo, pFeedbackInfo, pDataSize, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetEventStatus(VkDevice device, VkEvent event) {
  return ELIXIR_VK_HOST(vkGetEventStatus)(device, event);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetExecutionGraphPipelineNodeIndexAMDX(VkDevice device, VkPipeline executionGraph, const VkPipelineShaderStageNodeCreateInfoAMDX* pNodeInfo, uint32_t* pNodeIndex) {
  return ELIXIR_VK_HOST(vkGetExecutionGraphPipelineNodeIndexAMDX)(device, executionGraph, pNodeInfo, pNodeIndex);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetExecutionGraphPipelineScratchSizeAMDX(VkDevice device, VkPipeline executionGraph, VkExecutionGraphPipelineScratchSizeAMDX* pSizeInfo) {
  return ELIXIR_VK_HOST(vkGetExecutionGraphPipelineScratchSizeAMDX)(device, executionGraph, pSizeInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetExternalComputeQueueDataNV(VkExternalComputeQueueNV externalQueue, VkExternalComputeQueueDataParamsNV* params, void* pData) {
  ELIXIR_VK_HOST(vkGetExternalComputeQueueDataNV)(externalQueue, params, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) {
  return ELIXIR_VK_HOST(vkGetFenceFdKHR)(device, pGetFdInfo, pFd);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetFenceStatus(VkDevice device, VkFence fence) {
  return ELIXIR_VK_HOST(vkGetFenceStatus)(device, fence);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetFramebufferTilePropertiesQCOM(VkDevice device, VkFramebuffer framebuffer, uint32_t* pPropertiesCount, VkTilePropertiesQCOM* pProperties) {
  return ELIXIR_VK_HOST(vkGetFramebufferTilePropertiesQCOM)(device, framebuffer, pPropertiesCount, pProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetGeneratedCommandsMemoryRequirementsEXT(VkDevice device, const VkGeneratedCommandsMemoryRequirementsInfoEXT* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetGeneratedCommandsMemoryRequirementsEXT)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetGeneratedCommandsMemoryRequirementsNV(VkDevice device, const VkGeneratedCommandsMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetGeneratedCommandsMemoryRequirementsNV)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetGpaDeviceClockInfoAMD(VkDevice device, VkGpaDeviceGetClockInfoAMD* pInfo) {
  return ELIXIR_VK_HOST(vkGetGpaDeviceClockInfoAMD)(device, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetGpaSessionResultsAMD(VkDevice device, VkGpaSessionAMD gpaSession, uint32_t sampleID, size_t* pSizeInBytes, void* pData) {
  return ELIXIR_VK_HOST(vkGetGpaSessionResultsAMD)(device, gpaSession, sampleID, pSizeInBytes, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetGpaSessionStatusAMD(VkDevice device, VkGpaSessionAMD gpaSession) {
  return ELIXIR_VK_HOST(vkGetGpaSessionStatusAMD)(device, gpaSession);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetImageDrmFormatModifierPropertiesEXT(VkDevice device, VkImage image, VkImageDrmFormatModifierPropertiesEXT* pProperties) {
  return ELIXIR_VK_HOST(vkGetImageDrmFormatModifierPropertiesEXT)(device, image, pProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetImageMemoryRequirements)(device, image, pMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetImageMemoryRequirements2)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetImageMemoryRequirements2KHR)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetImageOpaqueCaptureDataEXT(VkDevice device, uint32_t imageCount, const VkImage* pImages, VkHostAddressRangeEXT* pDatas) {
  return ELIXIR_VK_HOST(vkGetImageOpaqueCaptureDataEXT)(device, imageCount, pImages, pDatas);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetImageOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkImageCaptureDescriptorDataInfoEXT* pInfo, void* pData) {
  return ELIXIR_VK_HOST(vkGetImageOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetImageSparseMemoryRequirements)(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageSparseMemoryRequirements2(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetImageSparseMemoryRequirements2)(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageSparseMemoryRequirements2KHR(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetImageSparseMemoryRequirements2KHR)(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
  ELIXIR_VK_HOST(vkGetImageSubresourceLayout)(device, image, pSubresource, pLayout);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageSubresourceLayout2(VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
  ELIXIR_VK_HOST(vkGetImageSubresourceLayout2)(device, image, pSubresource, pLayout);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageSubresourceLayout2EXT(VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
  ELIXIR_VK_HOST(vkGetImageSubresourceLayout2EXT)(device, image, pSubresource, pLayout);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetImageSubresourceLayout2KHR(VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
  ELIXIR_VK_HOST(vkGetImageSubresourceLayout2KHR)(device, image, pSubresource, pLayout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetImageViewAddressNVX(VkDevice device, VkImageView imageView, VkImageViewAddressPropertiesNVX* pProperties) {
  return ELIXIR_VK_HOST(vkGetImageViewAddressNVX)(device, imageView, pProperties);
}

ELIXIR_SYSV uint64_t elixir_vk_sysv_vkGetImageViewHandle64NVX(VkDevice device, const VkImageViewHandleInfoNVX* pInfo) {
  return ELIXIR_VK_HOST(vkGetImageViewHandle64NVX)(device, pInfo);
}

ELIXIR_SYSV uint32_t elixir_vk_sysv_vkGetImageViewHandleNVX(VkDevice device, const VkImageViewHandleInfoNVX* pInfo) {
  return ELIXIR_VK_HOST(vkGetImageViewHandleNVX)(device, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetImageViewOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkImageViewCaptureDescriptorDataInfoEXT* pInfo, void* pData) {
  return ELIXIR_VK_HOST(vkGetImageViewOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetLatencyTimingsNV(VkDevice device, VkSwapchainKHR swapchain, VkGetLatencyMarkerInfoNV* pLatencyMarkerInfo) {
  ELIXIR_VK_HOST(vkGetLatencyTimingsNV)(device, swapchain, pLatencyMarkerInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) {
  return ELIXIR_VK_HOST(vkGetMemoryFdKHR)(device, pGetFdInfo, pFd);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
  return ELIXIR_VK_HOST(vkGetMemoryFdPropertiesKHR)(device, handleType, fd, pMemoryFdProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetMemoryHostPointerPropertiesEXT(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, const void* pHostPointer, VkMemoryHostPointerPropertiesEXT* pMemoryHostPointerProperties) {
  return ELIXIR_VK_HOST(vkGetMemoryHostPointerPropertiesEXT)(device, handleType, pHostPointer, pMemoryHostPointerProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetMemoryRemoteAddressNV(VkDevice device, const VkMemoryGetRemoteAddressInfoNV* pMemoryGetRemoteAddressInfo, VkRemoteAddressNV* pAddress) {
  return ELIXIR_VK_HOST(vkGetMemoryRemoteAddressNV)(device, pMemoryGetRemoteAddressInfo, pAddress);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetMicromapBuildSizesEXT(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkMicromapBuildInfoEXT* pBuildInfo, VkMicromapBuildSizesInfoEXT* pSizeInfo) {
  ELIXIR_VK_HOST(vkGetMicromapBuildSizesEXT)(device, buildType, pBuildInfo, pSizeInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPartitionedAccelerationStructuresBuildSizesNV(VkDevice device, const VkPartitionedAccelerationStructureInstancesInputNV* pInfo, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) {
  ELIXIR_VK_HOST(vkGetPartitionedAccelerationStructuresBuildSizesNV)(device, pInfo, pSizeInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPastPresentationTimingEXT(VkDevice device, const VkPastPresentationTimingInfoEXT* pPastPresentationTimingInfo, VkPastPresentationTimingPropertiesEXT* pPastPresentationTimingProperties) {
  return ELIXIR_VK_HOST(vkGetPastPresentationTimingEXT)(device, pPastPresentationTimingInfo, pPastPresentationTimingProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPastPresentationTimingGOOGLE(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pPresentationTimingCount, VkPastPresentationTimingGOOGLE* pPresentationTimings) {
  return ELIXIR_VK_HOST(vkGetPastPresentationTimingGOOGLE)(device, swapchain, pPresentationTimingCount, pPresentationTimings);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPerformanceParameterINTEL(VkDevice device, VkPerformanceParameterTypeINTEL parameter, VkPerformanceValueINTEL* pValue) {
  return ELIXIR_VK_HOST(vkGetPerformanceParameterINTEL)(device, parameter, pValue);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)(physicalDevice, pTimeDomainCount, pTimeDomains);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceCalibrateableTimeDomainsKHR)(physicalDevice, pTimeDomainCount, pTimeDomains);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixFlexibleDimensionsPropertiesNV* pProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV)(physicalDevice, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesKHR* pProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR)(physicalDevice, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesNV* pProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceCooperativeMatrixPropertiesNV)(physicalDevice, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceCooperativeVectorPropertiesNV(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeVectorPropertiesNV* pProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceCooperativeVectorPropertiesNV)(physicalDevice, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkDeviceSize elixir_vk_sysv_vkGetPhysicalDeviceDescriptorSizeEXT(VkPhysicalDevice physicalDevice, VkDescriptorType descriptorType) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceDescriptorSizeEXT)(physicalDevice, descriptorType);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceDisplayPlaneProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayPlaneProperties2KHR* pProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceDisplayPlaneProperties2KHR)(physicalDevice, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceDisplayPlanePropertiesKHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayPlanePropertiesKHR* pProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceDisplayPlanePropertiesKHR)(physicalDevice, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceDisplayProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayProperties2KHR* pProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceDisplayProperties2KHR)(physicalDevice, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceDisplayPropertiesKHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkDisplayPropertiesKHR* pProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceDisplayPropertiesKHR)(physicalDevice, pPropertyCount, pProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceExternalBufferProperties)(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceExternalBufferPropertiesKHR)(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceExternalFenceProperties)(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceExternalFencePropertiesKHR)(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceExternalImageFormatPropertiesNV(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkExternalMemoryHandleTypeFlagsNV externalHandleType, VkExternalImageFormatPropertiesNV* pExternalImageFormatProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceExternalImageFormatPropertiesNV)(physicalDevice, format, type, tiling, usage, flags, externalHandleType, pExternalImageFormatProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceExternalSemaphoreProperties)(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceExternalTensorPropertiesARM(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalTensorInfoARM* pExternalTensorInfo, VkExternalTensorPropertiesARM* pExternalTensorProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceExternalTensorPropertiesARM)(physicalDevice, pExternalTensorInfo, pExternalTensorProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceFragmentShadingRatesKHR(VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceFragmentShadingRatesKHR)(physicalDevice, pFragmentShadingRateCount, pFragmentShadingRates);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceMemoryProperties)(physicalDevice, pMemoryProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceMemoryProperties2)(physicalDevice, pMemoryProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceMemoryProperties2KHR)(physicalDevice, pMemoryProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceMultisamplePropertiesEXT)(physicalDevice, samples, pMultisampleProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceOpticalFlowImageFormatsNV(VkPhysicalDevice physicalDevice, const VkOpticalFlowImageFormatInfoNV* pOpticalFlowImageFormatInfo, uint32_t* pFormatCount, VkOpticalFlowImageFormatPropertiesNV* pImageFormatProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceOpticalFlowImageFormatsNV)(physicalDevice, pOpticalFlowImageFormatInfo, pFormatCount, pImageFormatProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) {
  return ELIXIR_VK_HOST(vkGetPhysicalDevicePresentRectanglesKHR)(physicalDevice, surface, pRectCount, pRects);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, const VkQueueFamilyDataGraphPropertiesARM* pQueueFamilyDataGraphProperties, VkBaseOutStructure* pProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM)(physicalDevice, queueFamilyIndex, pQueueFamilyDataGraphProperties, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, const VkQueueFamilyDataGraphPropertiesARM* pQueueFamilyDataGraphProperties, const VkDataGraphOpticalFlowImageFormatInfoARM* pOpticalFlowImageFormatInfo, uint32_t* pFormatCount, VkDataGraphOpticalFlowImageFormatPropertiesARM* pImageFormatProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM)(physicalDevice, queueFamilyIndex, pQueueFamilyDataGraphProperties, pOpticalFlowImageFormatInfo, pFormatCount, pImageFormatProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceQueueFamilyDataGraphProcessingEngineInfoARM* pQueueFamilyDataGraphProcessingEngineInfo, VkQueueFamilyDataGraphProcessingEnginePropertiesARM* pQueueFamilyDataGraphProcessingEngineProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM)(physicalDevice, pQueueFamilyDataGraphProcessingEngineInfo, pQueueFamilyDataGraphProcessingEngineProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t* pQueueFamilyDataGraphPropertyCount, VkQueueFamilyDataGraphPropertiesARM* pQueueFamilyDataGraphProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM)(physicalDevice, queueFamilyIndex, pQueueFamilyDataGraphPropertyCount, pQueueFamilyDataGraphProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(VkPhysicalDevice physicalDevice, const VkQueryPoolPerformanceCreateInfoKHR* pPerformanceQueryCreateInfo, uint32_t* pNumPasses) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR)(physicalDevice, pPerformanceQueryCreateInfo, pNumPasses);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceQueueFamilyProperties)(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceQueueFamilyProperties2)(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceQueueFamilyProperties2KHR)(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceSparseImageFormatProperties)(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceSparseImageFormatProperties2)(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
  ELIXIR_VK_HOST(vkGetPhysicalDeviceSparseImageFormatProperties2KHR)(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(VkPhysicalDevice physicalDevice, uint32_t* pCombinationCount, VkFramebufferMixedSamplesCombinationNV* pCombinations) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV)(physicalDevice, pCombinationCount, pCombinations);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceSurfaceCapabilities2EXT(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilities2EXT* pSurfaceCapabilities) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceCapabilities2EXT)(physicalDevice, surface, pSurfaceCapabilities);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, VkSurfaceCapabilities2KHR* pSurfaceCapabilities) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceCapabilities2KHR)(physicalDevice, pSurfaceInfo, pSurfaceCapabilities);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(physicalDevice, surface, pSurfaceCapabilities);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, uint32_t* pSurfaceFormatCount, VkSurfaceFormat2KHR* pSurfaceFormats) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceFormats2KHR)(physicalDevice, pSurfaceInfo, pSurfaceFormatCount, pSurfaceFormats);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceFormatsKHR)(physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceSurfaceSupportKHR)(physicalDevice, queueFamilyIndex, surface, pSupported);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceToolProperties)(physicalDevice, pToolCount, pToolProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceToolPropertiesEXT(VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceToolPropertiesEXT)(physicalDevice, pToolCount, pToolProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceVideoCapabilitiesKHR(VkPhysicalDevice physicalDevice, const VkVideoProfileInfoKHR* pVideoProfile, VkVideoCapabilitiesKHR* pCapabilities) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceVideoCapabilitiesKHR)(physicalDevice, pVideoProfile, pCapabilities);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR* pQualityLevelInfo, VkVideoEncodeQualityLevelPropertiesKHR* pQualityLevelProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR)(physicalDevice, pQualityLevelInfo, pQualityLevelProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPhysicalDeviceVideoFormatPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceVideoFormatInfoKHR* pVideoFormatInfo, uint32_t* pVideoFormatPropertyCount, VkVideoFormatPropertiesKHR* pVideoFormatProperties) {
  return ELIXIR_VK_HOST(vkGetPhysicalDeviceVideoFormatPropertiesKHR)(physicalDevice, pVideoFormatInfo, pVideoFormatPropertyCount, pVideoFormatProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPipelineBinaryDataKHR(VkDevice device, const VkPipelineBinaryDataInfoKHR* pInfo, VkPipelineBinaryKeyKHR* pPipelineBinaryKey, size_t* pPipelineBinaryDataSize, void* pPipelineBinaryData) {
  return ELIXIR_VK_HOST(vkGetPipelineBinaryDataKHR)(device, pInfo, pPipelineBinaryKey, pPipelineBinaryDataSize, pPipelineBinaryData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) {
  return ELIXIR_VK_HOST(vkGetPipelineCacheData)(device, pipelineCache, pDataSize, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPipelineExecutableInternalRepresentationsKHR(VkDevice device, const VkPipelineExecutableInfoKHR* pExecutableInfo, uint32_t* pInternalRepresentationCount, VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations) {
  return ELIXIR_VK_HOST(vkGetPipelineExecutableInternalRepresentationsKHR)(device, pExecutableInfo, pInternalRepresentationCount, pInternalRepresentations);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPipelineExecutablePropertiesKHR(VkDevice device, const VkPipelineInfoKHR* pPipelineInfo, uint32_t* pExecutableCount, VkPipelineExecutablePropertiesKHR* pProperties) {
  return ELIXIR_VK_HOST(vkGetPipelineExecutablePropertiesKHR)(device, pPipelineInfo, pExecutableCount, pProperties);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPipelineExecutableStatisticsKHR(VkDevice device, const VkPipelineExecutableInfoKHR* pExecutableInfo, uint32_t* pStatisticCount, VkPipelineExecutableStatisticKHR* pStatistics) {
  return ELIXIR_VK_HOST(vkGetPipelineExecutableStatisticsKHR)(device, pExecutableInfo, pStatisticCount, pStatistics);
}

ELIXIR_SYSV VkDeviceAddress elixir_vk_sysv_vkGetPipelineIndirectDeviceAddressNV(VkDevice device, const VkPipelineIndirectDeviceAddressInfoNV* pInfo) {
  return ELIXIR_VK_HOST(vkGetPipelineIndirectDeviceAddressNV)(device, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPipelineIndirectMemoryRequirementsNV(VkDevice device, const VkComputePipelineCreateInfo* pCreateInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetPipelineIndirectMemoryRequirementsNV)(device, pCreateInfo, pMemoryRequirements);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPipelineKeyKHR(VkDevice device, const VkPipelineCreateInfoKHR* pPipelineCreateInfo, VkPipelineBinaryKeyKHR* pPipelineKey) {
  return ELIXIR_VK_HOST(vkGetPipelineKeyKHR)(device, pPipelineCreateInfo, pPipelineKey);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetPipelinePropertiesEXT(VkDevice device, const VkPipelineInfoKHR* pPipelineInfo, VkBaseOutStructure* pPipelineProperties) {
  return ELIXIR_VK_HOST(vkGetPipelinePropertiesEXT)(device, pPipelineInfo, pPipelineProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) {
  ELIXIR_VK_HOST(vkGetPrivateData)(device, objectType, objectHandle, privateDataSlot, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetPrivateDataEXT(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) {
  ELIXIR_VK_HOST(vkGetPrivateDataEXT)(device, objectType, objectHandle, privateDataSlot, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
  return ELIXIR_VK_HOST(vkGetQueryPoolResults)(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetQueueCheckpointData2NV(VkQueue queue, uint32_t* pCheckpointDataCount, VkCheckpointData2NV* pCheckpointData) {
  ELIXIR_VK_HOST(vkGetQueueCheckpointData2NV)(queue, pCheckpointDataCount, pCheckpointData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetQueueCheckpointDataNV(VkQueue queue, uint32_t* pCheckpointDataCount, VkCheckpointDataNV* pCheckpointData) {
  ELIXIR_VK_HOST(vkGetQueueCheckpointDataNV)(queue, pCheckpointDataCount, pCheckpointData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
  return ELIXIR_VK_HOST(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR)(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
  return ELIXIR_VK_HOST(vkGetRayTracingShaderGroupHandlesKHR)(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetRayTracingShaderGroupHandlesNV(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
  return ELIXIR_VK_HOST(vkGetRayTracingShaderGroupHandlesNV)(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

ELIXIR_SYSV VkDeviceSize elixir_vk_sysv_vkGetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) {
  return ELIXIR_VK_HOST(vkGetRayTracingShaderGroupStackSizeKHR)(device, pipeline, group, groupShader);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetRefreshCycleDurationGOOGLE(VkDevice device, VkSwapchainKHR swapchain, VkRefreshCycleDurationGOOGLE* pDisplayTimingProperties) {
  return ELIXIR_VK_HOST(vkGetRefreshCycleDurationGOOGLE)(device, swapchain, pDisplayTimingProperties);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
  ELIXIR_VK_HOST(vkGetRenderAreaGranularity)(device, renderPass, pGranularity);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetRenderingAreaGranularity(VkDevice device, const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity) {
  ELIXIR_VK_HOST(vkGetRenderingAreaGranularity)(device, pRenderingAreaInfo, pGranularity);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetRenderingAreaGranularityKHR(VkDevice device, const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity) {
  ELIXIR_VK_HOST(vkGetRenderingAreaGranularityKHR)(device, pRenderingAreaInfo, pGranularity);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetSamplerOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkSamplerCaptureDescriptorDataInfoEXT* pInfo, void* pData) {
  return ELIXIR_VK_HOST(vkGetSamplerOpaqueCaptureDescriptorDataEXT)(device, pInfo, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
  return ELIXIR_VK_HOST(vkGetSemaphoreCounterValue)(device, semaphore, pValue);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetSemaphoreCounterValueKHR(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
  return ELIXIR_VK_HOST(vkGetSemaphoreCounterValueKHR)(device, semaphore, pValue);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) {
  return ELIXIR_VK_HOST(vkGetSemaphoreFdKHR)(device, pGetFdInfo, pFd);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetShaderBinaryDataEXT(VkDevice device, VkShaderEXT shader, size_t* pDataSize, void* pData) {
  return ELIXIR_VK_HOST(vkGetShaderBinaryDataEXT)(device, shader, pDataSize, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetShaderInfoAMD(VkDevice device, VkPipeline pipeline, VkShaderStageFlagBits shaderStage, VkShaderInfoTypeAMD infoType, size_t* pInfoSize, void* pInfo) {
  return ELIXIR_VK_HOST(vkGetShaderInfoAMD)(device, pipeline, shaderStage, infoType, pInfoSize, pInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetShaderInstrumentationValuesARM(VkDevice device, VkShaderInstrumentationARM instrumentation, uint32_t* pMetricBlockCount, void* pMetricValues, VkShaderInstrumentationValuesFlagsARM flags) {
  return ELIXIR_VK_HOST(vkGetShaderInstrumentationValuesARM)(device, instrumentation, pMetricBlockCount, pMetricValues, flags);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetShaderModuleCreateInfoIdentifierEXT(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModuleIdentifierEXT* pIdentifier) {
  ELIXIR_VK_HOST(vkGetShaderModuleCreateInfoIdentifierEXT)(device, pCreateInfo, pIdentifier);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetShaderModuleIdentifierEXT(VkDevice device, VkShaderModule shaderModule, VkShaderModuleIdentifierEXT* pIdentifier) {
  ELIXIR_VK_HOST(vkGetShaderModuleIdentifierEXT)(device, shaderModule, pIdentifier);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetSwapchainCounterEXT(VkDevice device, VkSwapchainKHR swapchain, VkSurfaceCounterFlagBitsEXT counter, uint64_t* pCounterValue) {
  return ELIXIR_VK_HOST(vkGetSwapchainCounterEXT)(device, swapchain, counter, pCounterValue);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
  return ELIXIR_VK_HOST(vkGetSwapchainImagesKHR)(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetSwapchainStatusKHR(VkDevice device, VkSwapchainKHR swapchain) {
  return ELIXIR_VK_HOST(vkGetSwapchainStatusKHR)(device, swapchain);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetSwapchainTimeDomainPropertiesEXT(VkDevice device, VkSwapchainKHR swapchain, VkSwapchainTimeDomainPropertiesEXT* pSwapchainTimeDomainProperties, uint64_t* pTimeDomainsCounter) {
  return ELIXIR_VK_HOST(vkGetSwapchainTimeDomainPropertiesEXT)(device, swapchain, pSwapchainTimeDomainProperties, pTimeDomainsCounter);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetSwapchainTimingPropertiesEXT(VkDevice device, VkSwapchainKHR swapchain, VkSwapchainTimingPropertiesEXT* pSwapchainTimingProperties, uint64_t* pSwapchainTimingPropertiesCounter) {
  return ELIXIR_VK_HOST(vkGetSwapchainTimingPropertiesEXT)(device, swapchain, pSwapchainTimingProperties, pSwapchainTimingPropertiesCounter);
}

ELIXIR_SYSV void elixir_vk_sysv_vkGetTensorMemoryRequirementsARM(VkDevice device, const VkTensorMemoryRequirementsInfoARM* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
  ELIXIR_VK_HOST(vkGetTensorMemoryRequirementsARM)(device, pInfo, pMemoryRequirements);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetTensorOpaqueCaptureDataARM(VkDevice device, uint32_t tensorCount, const VkTensorARM* pTensors, VkHostAddressRangeEXT* pDatas) {
  return ELIXIR_VK_HOST(vkGetTensorOpaqueCaptureDataARM)(device, tensorCount, pTensors, pDatas);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetTensorOpaqueCaptureDescriptorDataARM(VkDevice device, const VkTensorCaptureDescriptorDataInfoARM* pInfo, void* pData) {
  return ELIXIR_VK_HOST(vkGetTensorOpaqueCaptureDescriptorDataARM)(device, pInfo, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetTensorViewOpaqueCaptureDescriptorDataARM(VkDevice device, const VkTensorViewCaptureDescriptorDataInfoARM* pInfo, void* pData) {
  return ELIXIR_VK_HOST(vkGetTensorViewOpaqueCaptureDescriptorDataARM)(device, pInfo, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetValidationCacheDataEXT(VkDevice device, VkValidationCacheEXT validationCache, size_t* pDataSize, void* pData) {
  return ELIXIR_VK_HOST(vkGetValidationCacheDataEXT)(device, validationCache, pDataSize, pData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkGetVideoSessionMemoryRequirementsKHR(VkDevice device, VkVideoSessionKHR videoSession, uint32_t* pMemoryRequirementsCount, VkVideoSessionMemoryRequirementsKHR* pMemoryRequirements) {
  return ELIXIR_VK_HOST(vkGetVideoSessionMemoryRequirementsKHR)(device, videoSession, pMemoryRequirementsCount, pMemoryRequirements);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkImportFenceFdKHR(VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) {
  return ELIXIR_VK_HOST(vkImportFenceFdKHR)(device, pImportFenceFdInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkImportSemaphoreFdKHR(VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) {
  return ELIXIR_VK_HOST(vkImportSemaphoreFdKHR)(device, pImportSemaphoreFdInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkInitializePerformanceApiINTEL(VkDevice device, const VkInitializePerformanceApiInfoINTEL* pInitializeInfo) {
  return ELIXIR_VK_HOST(vkInitializePerformanceApiINTEL)(device, pInitializeInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
  return ELIXIR_VK_HOST(vkInvalidateMappedMemoryRanges)(device, memoryRangeCount, pMemoryRanges);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkLatencySleepNV(VkDevice device, VkSwapchainKHR swapchain, const VkLatencySleepInfoNV* pSleepInfo) {
  return ELIXIR_VK_HOST(vkLatencySleepNV)(device, swapchain, pSleepInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
  return ELIXIR_VK_HOST(vkMergePipelineCaches)(device, dstCache, srcCacheCount, pSrcCaches);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkMergeValidationCachesEXT(VkDevice device, VkValidationCacheEXT dstCache, uint32_t srcCacheCount, const VkValidationCacheEXT* pSrcCaches) {
  return ELIXIR_VK_HOST(vkMergeValidationCachesEXT)(device, dstCache, srcCacheCount, pSrcCaches);
}

ELIXIR_SYSV void elixir_vk_sysv_vkQueueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) {
  ELIXIR_VK_HOST(vkQueueBeginDebugUtilsLabelEXT)(queue, pLabelInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
  return ELIXIR_VK_HOST(vkQueueBindSparse)(queue, bindInfoCount, pBindInfo, fence);
}

ELIXIR_SYSV void elixir_vk_sysv_vkQueueEndDebugUtilsLabelEXT(VkQueue queue) {
  ELIXIR_VK_HOST(vkQueueEndDebugUtilsLabelEXT)(queue);
}

ELIXIR_SYSV void elixir_vk_sysv_vkQueueInsertDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) {
  ELIXIR_VK_HOST(vkQueueInsertDebugUtilsLabelEXT)(queue, pLabelInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkQueueNotifyOutOfBandNV(VkQueue queue, const VkOutOfBandQueueTypeInfoNV* pQueueTypeInfo) {
  ELIXIR_VK_HOST(vkQueueNotifyOutOfBandNV)(queue, pQueueTypeInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkQueueSetPerfHintQCOM(VkQueue queue, const VkPerfHintInfoQCOM* pPerfHintInfo) {
  return ELIXIR_VK_HOST(vkQueueSetPerfHintQCOM)(queue, pPerfHintInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkQueueSetPerformanceConfigurationINTEL(VkQueue queue, VkPerformanceConfigurationINTEL configuration) {
  return ELIXIR_VK_HOST(vkQueueSetPerformanceConfigurationINTEL)(queue, configuration);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkQueueWaitIdle(VkQueue queue) {
  return ELIXIR_VK_HOST(vkQueueWaitIdle)(queue);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkRegisterCustomBorderColorEXT(VkDevice device, const VkSamplerCustomBorderColorCreateInfoEXT* pBorderColor, VkBool32 requestIndex, uint32_t* pIndex) {
  return ELIXIR_VK_HOST(vkRegisterCustomBorderColorEXT)(device, pBorderColor, requestIndex, pIndex);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkRegisterDeviceEventEXT(VkDevice device, const VkDeviceEventInfoEXT* pDeviceEventInfo, const VkAllocationCallbacks*, VkFence* pFence) {
  return ELIXIR_VK_HOST(vkRegisterDeviceEventEXT)(device, pDeviceEventInfo, nullptr, pFence);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkRegisterDisplayEventEXT(VkDevice device, VkDisplayKHR display, const VkDisplayEventInfoEXT* pDisplayEventInfo, const VkAllocationCallbacks*, VkFence* pFence) {
  return ELIXIR_VK_HOST(vkRegisterDisplayEventEXT)(device, display, pDisplayEventInfo, nullptr, pFence);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkReleaseCapturedPipelineDataKHR(VkDevice device, const VkReleaseCapturedPipelineDataInfoKHR* pInfo, const VkAllocationCallbacks*) {
  return ELIXIR_VK_HOST(vkReleaseCapturedPipelineDataKHR)(device, pInfo, nullptr);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkReleaseDisplayEXT(VkPhysicalDevice physicalDevice, VkDisplayKHR display) {
  return ELIXIR_VK_HOST(vkReleaseDisplayEXT)(physicalDevice, display);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkReleasePerformanceConfigurationINTEL(VkDevice device, VkPerformanceConfigurationINTEL configuration) {
  return ELIXIR_VK_HOST(vkReleasePerformanceConfigurationINTEL)(device, configuration);
}

ELIXIR_SYSV void elixir_vk_sysv_vkReleaseProfilingLockKHR(VkDevice device) {
  ELIXIR_VK_HOST(vkReleaseProfilingLockKHR)(device);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkReleaseSwapchainImagesEXT(VkDevice device, const VkReleaseSwapchainImagesInfoKHR* pReleaseInfo) {
  return ELIXIR_VK_HOST(vkReleaseSwapchainImagesEXT)(device, pReleaseInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkReleaseSwapchainImagesKHR(VkDevice device, const VkReleaseSwapchainImagesInfoKHR* pReleaseInfo) {
  return ELIXIR_VK_HOST(vkReleaseSwapchainImagesKHR)(device, pReleaseInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
  return ELIXIR_VK_HOST(vkResetDescriptorPool)(device, descriptorPool, flags);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkResetEvent(VkDevice device, VkEvent event) {
  return ELIXIR_VK_HOST(vkResetEvent)(device, event);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
  return ELIXIR_VK_HOST(vkResetFences)(device, fenceCount, pFences);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkResetGpaSessionAMD(VkDevice device, VkGpaSessionAMD gpaSession) {
  return ELIXIR_VK_HOST(vkResetGpaSessionAMD)(device, gpaSession);
}

ELIXIR_SYSV void elixir_vk_sysv_vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
  ELIXIR_VK_HOST(vkResetQueryPool)(device, queryPool, firstQuery, queryCount);
}

ELIXIR_SYSV void elixir_vk_sysv_vkResetQueryPoolEXT(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
  ELIXIR_VK_HOST(vkResetQueryPoolEXT)(device, queryPool, firstQuery, queryCount);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo) {
  return ELIXIR_VK_HOST(vkSetDebugUtilsObjectNameEXT)(device, pNameInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSetDebugUtilsObjectTagEXT(VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo) {
  return ELIXIR_VK_HOST(vkSetDebugUtilsObjectTagEXT)(device, pTagInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkSetDeviceMemoryPriorityEXT(VkDevice device, VkDeviceMemory memory, float priority) {
  ELIXIR_VK_HOST(vkSetDeviceMemoryPriorityEXT)(device, memory, priority);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSetEvent(VkDevice device, VkEvent event) {
  return ELIXIR_VK_HOST(vkSetEvent)(device, event);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSetGpaDeviceClockModeAMD(VkDevice device, VkGpaDeviceClockModeInfoAMD* pInfo) {
  return ELIXIR_VK_HOST(vkSetGpaDeviceClockModeAMD)(device, pInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkSetHdrMetadataEXT(VkDevice device, uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata) {
  ELIXIR_VK_HOST(vkSetHdrMetadataEXT)(device, swapchainCount, pSwapchains, pMetadata);
}

ELIXIR_SYSV void elixir_vk_sysv_vkSetLatencyMarkerNV(VkDevice device, VkSwapchainKHR swapchain, const VkSetLatencyMarkerInfoNV* pLatencyMarkerInfo) {
  ELIXIR_VK_HOST(vkSetLatencyMarkerNV)(device, swapchain, pLatencyMarkerInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSetLatencySleepModeNV(VkDevice device, VkSwapchainKHR swapchain, const VkLatencySleepModeInfoNV* pSleepModeInfo) {
  return ELIXIR_VK_HOST(vkSetLatencySleepModeNV)(device, swapchain, pSleepModeInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkSetLocalDimmingAMD(VkDevice device, VkSwapchainKHR swapChain, VkBool32 localDimmingEnable) {
  ELIXIR_VK_HOST(vkSetLocalDimmingAMD)(device, swapChain, localDimmingEnable);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) {
  return ELIXIR_VK_HOST(vkSetPrivateData)(device, objectType, objectHandle, privateDataSlot, data);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSetPrivateDataEXT(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) {
  return ELIXIR_VK_HOST(vkSetPrivateDataEXT)(device, objectType, objectHandle, privateDataSlot, data);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSetSwapchainPresentTimingQueueSizeEXT(VkDevice device, VkSwapchainKHR swapchain, uint32_t size) {
  return ELIXIR_VK_HOST(vkSetSwapchainPresentTimingQueueSizeEXT)(device, swapchain, size);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
  return ELIXIR_VK_HOST(vkSignalSemaphore)(device, pSignalInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkSignalSemaphoreKHR(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
  return ELIXIR_VK_HOST(vkSignalSemaphoreKHR)(device, pSignalInfo);
}

ELIXIR_SYSV void elixir_vk_sysv_vkSubmitDebugUtilsMessageEXT(VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) {
  ELIXIR_VK_HOST(vkSubmitDebugUtilsMessageEXT)(instance, messageSeverity, messageTypes, pCallbackData);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkTransitionImageLayout(VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo* pTransitions) {
  return ELIXIR_VK_HOST(vkTransitionImageLayout)(device, transitionCount, pTransitions);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkTransitionImageLayoutEXT(VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo* pTransitions) {
  return ELIXIR_VK_HOST(vkTransitionImageLayoutEXT)(device, transitionCount, pTransitions);
}

ELIXIR_SYSV void elixir_vk_sysv_vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
  ELIXIR_VK_HOST(vkTrimCommandPool)(device, commandPool, flags);
}

ELIXIR_SYSV void elixir_vk_sysv_vkTrimCommandPoolKHR(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
  ELIXIR_VK_HOST(vkTrimCommandPoolKHR)(device, commandPool, flags);
}

ELIXIR_SYSV void elixir_vk_sysv_vkUninitializePerformanceApiINTEL(VkDevice device) {
  ELIXIR_VK_HOST(vkUninitializePerformanceApiINTEL)(device);
}

ELIXIR_SYSV void elixir_vk_sysv_vkUnregisterCustomBorderColorEXT(VkDevice device, uint32_t index) {
  ELIXIR_VK_HOST(vkUnregisterCustomBorderColorEXT)(device, index);
}

ELIXIR_SYSV void elixir_vk_sysv_vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
  ELIXIR_VK_HOST(vkUpdateDescriptorSetWithTemplate)(device, descriptorSet, descriptorUpdateTemplate, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkUpdateDescriptorSetWithTemplateKHR(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
  ELIXIR_VK_HOST(vkUpdateDescriptorSetWithTemplateKHR)(device, descriptorSet, descriptorUpdateTemplate, pData);
}

ELIXIR_SYSV void elixir_vk_sysv_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
  ELIXIR_VK_HOST(vkUpdateDescriptorSets)(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

ELIXIR_SYSV void elixir_vk_sysv_vkUpdateIndirectExecutionSetPipelineEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet, uint32_t executionSetWriteCount, const VkWriteIndirectExecutionSetPipelineEXT* pExecutionSetWrites) {
  ELIXIR_VK_HOST(vkUpdateIndirectExecutionSetPipelineEXT)(device, indirectExecutionSet, executionSetWriteCount, pExecutionSetWrites);
}

ELIXIR_SYSV void elixir_vk_sysv_vkUpdateIndirectExecutionSetShaderEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet, uint32_t executionSetWriteCount, const VkWriteIndirectExecutionSetShaderEXT* pExecutionSetWrites) {
  ELIXIR_VK_HOST(vkUpdateIndirectExecutionSetShaderEXT)(device, indirectExecutionSet, executionSetWriteCount, pExecutionSetWrites);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkUpdateVideoSessionParametersKHR(VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkVideoSessionParametersUpdateInfoKHR* pUpdateInfo) {
  return ELIXIR_VK_HOST(vkUpdateVideoSessionParametersKHR)(device, videoSessionParameters, pUpdateInfo);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
  return ELIXIR_VK_HOST(vkWaitForFences)(device, fenceCount, pFences, waitAll, timeout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkWaitForPresent2KHR(VkDevice device, VkSwapchainKHR swapchain, const VkPresentWait2InfoKHR* pPresentWait2Info) {
  return ELIXIR_VK_HOST(vkWaitForPresent2KHR)(device, swapchain, pPresentWait2Info);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkWaitForPresentKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t presentId, uint64_t timeout) {
  return ELIXIR_VK_HOST(vkWaitForPresentKHR)(device, swapchain, presentId, timeout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
  return ELIXIR_VK_HOST(vkWaitSemaphores)(device, pWaitInfo, timeout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
  return ELIXIR_VK_HOST(vkWaitSemaphoresKHR)(device, pWaitInfo, timeout);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkWriteAccelerationStructuresPropertiesKHR(VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) {
  return ELIXIR_VK_HOST(vkWriteAccelerationStructuresPropertiesKHR)(device, accelerationStructureCount, pAccelerationStructures, queryType, dataSize, pData, stride);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkWriteMicromapsPropertiesEXT(VkDevice device, uint32_t micromapCount, const VkMicromapEXT* pMicromaps, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) {
  return ELIXIR_VK_HOST(vkWriteMicromapsPropertiesEXT)(device, micromapCount, pMicromaps, queryType, dataSize, pData, stride);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkWriteResourceDescriptorsEXT(VkDevice device, uint32_t resourceCount, const VkResourceDescriptorInfoEXT* pResources, const VkHostAddressRangeEXT* pDescriptors) {
  return ELIXIR_VK_HOST(vkWriteResourceDescriptorsEXT)(device, resourceCount, pResources, pDescriptors);
}

ELIXIR_SYSV VkResult elixir_vk_sysv_vkWriteSamplerDescriptorsEXT(VkDevice device, uint32_t samplerCount, const VkSamplerCreateInfo* pSamplers, const VkHostAddressRangeEXT* pDescriptors) {
  return ELIXIR_VK_HOST(vkWriteSamplerDescriptorsEXT)(device, samplerCount, pSamplers, pDescriptors);
}
}

namespace elixir::vulkan::generated {
namespace {

template <typename HostFn>
struct GuestSignature;
template <typename R, typename... A>
struct GuestSignature<R(VKAPI_PTR*)(A...)> {
  using type = R(ELIXIR_SYSV*)(A...);
};

}

#define ELIXIR_VK_SIGNATURE(cmd, fn) \
  static_assert(std::is_same_v<decltype(&fn), GuestSignature<PFN_##cmd>::type>, #fn " differs from PFN_" #cmd);
ELIXIR_VK_SIGNATURE(vkAcquireDrmDisplayEXT, elixir_vk_sysv_vkAcquireDrmDisplayEXT)
ELIXIR_VK_SIGNATURE(vkAcquireNextImage2KHR, elixir_vk_intercept_vkAcquireNextImage2KHR)
ELIXIR_VK_SIGNATURE(vkAcquireNextImageKHR, elixir_vk_intercept_vkAcquireNextImageKHR)
ELIXIR_VK_SIGNATURE(vkAcquirePerformanceConfigurationINTEL, elixir_vk_sysv_vkAcquirePerformanceConfigurationINTEL)
ELIXIR_VK_SIGNATURE(vkAcquireProfilingLockKHR, elixir_vk_sysv_vkAcquireProfilingLockKHR)
ELIXIR_VK_SIGNATURE(vkAllocateCommandBuffers, elixir_vk_intercept_vkAllocateCommandBuffers)
ELIXIR_VK_SIGNATURE(vkAllocateDescriptorSets, elixir_vk_sysv_vkAllocateDescriptorSets)
ELIXIR_VK_SIGNATURE(vkAllocateMemory, elixir_vk_sysv_vkAllocateMemory)
ELIXIR_VK_SIGNATURE(vkAntiLagUpdateAMD, elixir_vk_sysv_vkAntiLagUpdateAMD)
ELIXIR_VK_SIGNATURE(vkBeginCommandBuffer, elixir_vk_intercept_vkBeginCommandBuffer)
ELIXIR_VK_SIGNATURE(vkBindAccelerationStructureMemoryNV, elixir_vk_sysv_vkBindAccelerationStructureMemoryNV)
ELIXIR_VK_SIGNATURE(vkBindBufferMemory, elixir_vk_intercept_vkBindBufferMemory)
ELIXIR_VK_SIGNATURE(vkBindBufferMemory2, elixir_vk_intercept_vkBindBufferMemory2)
ELIXIR_VK_SIGNATURE(vkBindBufferMemory2KHR, elixir_vk_intercept_vkBindBufferMemory2KHR)
ELIXIR_VK_SIGNATURE(vkBindDataGraphPipelineSessionMemoryARM, elixir_vk_sysv_vkBindDataGraphPipelineSessionMemoryARM)
ELIXIR_VK_SIGNATURE(vkBindImageMemory, elixir_vk_sysv_vkBindImageMemory)
ELIXIR_VK_SIGNATURE(vkBindImageMemory2, elixir_vk_sysv_vkBindImageMemory2)
ELIXIR_VK_SIGNATURE(vkBindImageMemory2KHR, elixir_vk_sysv_vkBindImageMemory2KHR)
ELIXIR_VK_SIGNATURE(vkBindOpticalFlowSessionImageNV, elixir_vk_sysv_vkBindOpticalFlowSessionImageNV)
ELIXIR_VK_SIGNATURE(vkBindTensorMemoryARM, elixir_vk_sysv_vkBindTensorMemoryARM)
ELIXIR_VK_SIGNATURE(vkBindVideoSessionMemoryKHR, elixir_vk_sysv_vkBindVideoSessionMemoryKHR)
ELIXIR_VK_SIGNATURE(vkBuildAccelerationStructuresKHR, elixir_vk_sysv_vkBuildAccelerationStructuresKHR)
ELIXIR_VK_SIGNATURE(vkBuildMicromapsEXT, elixir_vk_sysv_vkBuildMicromapsEXT)
ELIXIR_VK_SIGNATURE(vkClearShaderInstrumentationMetricsARM, elixir_vk_sysv_vkClearShaderInstrumentationMetricsARM)
ELIXIR_VK_SIGNATURE(vkCmdBeginConditionalRendering2EXT, elixir_vk_sysv_vkCmdBeginConditionalRendering2EXT)
ELIXIR_VK_SIGNATURE(vkCmdBeginConditionalRenderingEXT, elixir_vk_sysv_vkCmdBeginConditionalRenderingEXT)
ELIXIR_VK_SIGNATURE(vkCmdBeginCustomResolveEXT, elixir_vk_sysv_vkCmdBeginCustomResolveEXT)
ELIXIR_VK_SIGNATURE(vkCmdBeginDebugUtilsLabelEXT, elixir_vk_sysv_vkCmdBeginDebugUtilsLabelEXT)
ELIXIR_VK_SIGNATURE(vkCmdBeginGpaSampleAMD, elixir_vk_sysv_vkCmdBeginGpaSampleAMD)
ELIXIR_VK_SIGNATURE(vkCmdBeginGpaSessionAMD, elixir_vk_sysv_vkCmdBeginGpaSessionAMD)
ELIXIR_VK_SIGNATURE(vkCmdBeginPerTileExecutionQCOM, elixir_vk_sysv_vkCmdBeginPerTileExecutionQCOM)
ELIXIR_VK_SIGNATURE(vkCmdBeginQuery, elixir_vk_sysv_vkCmdBeginQuery)
ELIXIR_VK_SIGNATURE(vkCmdBeginQueryIndexedEXT, elixir_vk_sysv_vkCmdBeginQueryIndexedEXT)
ELIXIR_VK_SIGNATURE(vkCmdBeginRenderPass, elixir_vk_sysv_vkCmdBeginRenderPass)
ELIXIR_VK_SIGNATURE(vkCmdBeginRenderPass2, elixir_vk_sysv_vkCmdBeginRenderPass2)
ELIXIR_VK_SIGNATURE(vkCmdBeginRenderPass2KHR, elixir_vk_sysv_vkCmdBeginRenderPass2KHR)
ELIXIR_VK_SIGNATURE(vkCmdBeginRendering, elixir_vk_sysv_vkCmdBeginRendering)
ELIXIR_VK_SIGNATURE(vkCmdBeginRenderingKHR, elixir_vk_sysv_vkCmdBeginRenderingKHR)
ELIXIR_VK_SIGNATURE(vkCmdBeginShaderInstrumentationARM, elixir_vk_sysv_vkCmdBeginShaderInstrumentationARM)
ELIXIR_VK_SIGNATURE(vkCmdBeginTransformFeedback2EXT, elixir_vk_sysv_vkCmdBeginTransformFeedback2EXT)
ELIXIR_VK_SIGNATURE(vkCmdBeginTransformFeedbackEXT, elixir_vk_sysv_vkCmdBeginTransformFeedbackEXT)
ELIXIR_VK_SIGNATURE(vkCmdBeginVideoCodingKHR, elixir_vk_sysv_vkCmdBeginVideoCodingKHR)
ELIXIR_VK_SIGNATURE(vkCmdBindDescriptorBufferEmbeddedSamplers2EXT, elixir_vk_sysv_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT)
ELIXIR_VK_SIGNATURE(vkCmdBindDescriptorBufferEmbeddedSamplersEXT, elixir_vk_sysv_vkCmdBindDescriptorBufferEmbeddedSamplersEXT)
ELIXIR_VK_SIGNATURE(vkCmdBindDescriptorBuffersEXT, elixir_vk_sysv_vkCmdBindDescriptorBuffersEXT)
ELIXIR_VK_SIGNATURE(vkCmdBindDescriptorSets, elixir_vk_sysv_vkCmdBindDescriptorSets)
ELIXIR_VK_SIGNATURE(vkCmdBindDescriptorSets2, elixir_vk_sysv_vkCmdBindDescriptorSets2)
ELIXIR_VK_SIGNATURE(vkCmdBindDescriptorSets2KHR, elixir_vk_sysv_vkCmdBindDescriptorSets2KHR)
ELIXIR_VK_SIGNATURE(vkCmdBindIndexBuffer, elixir_vk_sysv_vkCmdBindIndexBuffer)
ELIXIR_VK_SIGNATURE(vkCmdBindIndexBuffer2, elixir_vk_sysv_vkCmdBindIndexBuffer2)
ELIXIR_VK_SIGNATURE(vkCmdBindIndexBuffer2KHR, elixir_vk_sysv_vkCmdBindIndexBuffer2KHR)
ELIXIR_VK_SIGNATURE(vkCmdBindIndexBuffer3KHR, elixir_vk_sysv_vkCmdBindIndexBuffer3KHR)
ELIXIR_VK_SIGNATURE(vkCmdBindInvocationMaskHUAWEI, elixir_vk_sysv_vkCmdBindInvocationMaskHUAWEI)
ELIXIR_VK_SIGNATURE(vkCmdBindPipeline, elixir_vk_sysv_vkCmdBindPipeline)
ELIXIR_VK_SIGNATURE(vkCmdBindPipelineShaderGroupNV, elixir_vk_sysv_vkCmdBindPipelineShaderGroupNV)
ELIXIR_VK_SIGNATURE(vkCmdBindResourceHeapEXT, elixir_vk_sysv_vkCmdBindResourceHeapEXT)
ELIXIR_VK_SIGNATURE(vkCmdBindSamplerHeapEXT, elixir_vk_sysv_vkCmdBindSamplerHeapEXT)
ELIXIR_VK_SIGNATURE(vkCmdBindShadersEXT, elixir_vk_sysv_vkCmdBindShadersEXT)
ELIXIR_VK_SIGNATURE(vkCmdBindShadingRateImageNV, elixir_vk_sysv_vkCmdBindShadingRateImageNV)
ELIXIR_VK_SIGNATURE(vkCmdBindTileMemoryQCOM, elixir_vk_sysv_vkCmdBindTileMemoryQCOM)
ELIXIR_VK_SIGNATURE(vkCmdBindTransformFeedbackBuffers2EXT, elixir_vk_sysv_vkCmdBindTransformFeedbackBuffers2EXT)
ELIXIR_VK_SIGNATURE(vkCmdBindTransformFeedbackBuffersEXT, elixir_vk_sysv_vkCmdBindTransformFeedbackBuffersEXT)
ELIXIR_VK_SIGNATURE(vkCmdBindVertexBuffers, elixir_vk_sysv_vkCmdBindVertexBuffers)
ELIXIR_VK_SIGNATURE(vkCmdBindVertexBuffers2, elixir_vk_sysv_vkCmdBindVertexBuffers2)
ELIXIR_VK_SIGNATURE(vkCmdBindVertexBuffers2EXT, elixir_vk_sysv_vkCmdBindVertexBuffers2EXT)
ELIXIR_VK_SIGNATURE(vkCmdBindVertexBuffers3KHR, elixir_vk_sysv_vkCmdBindVertexBuffers3KHR)
ELIXIR_VK_SIGNATURE(vkCmdBlitImage, elixir_vk_sysv_vkCmdBlitImage)
ELIXIR_VK_SIGNATURE(vkCmdBlitImage2, elixir_vk_sysv_vkCmdBlitImage2)
ELIXIR_VK_SIGNATURE(vkCmdBlitImage2KHR, elixir_vk_sysv_vkCmdBlitImage2KHR)
ELIXIR_VK_SIGNATURE(vkCmdBuildAccelerationStructureNV, elixir_vk_sysv_vkCmdBuildAccelerationStructureNV)
ELIXIR_VK_SIGNATURE(vkCmdBuildAccelerationStructuresIndirectKHR, elixir_vk_sysv_vkCmdBuildAccelerationStructuresIndirectKHR)
ELIXIR_VK_SIGNATURE(vkCmdBuildAccelerationStructuresKHR, elixir_vk_sysv_vkCmdBuildAccelerationStructuresKHR)
ELIXIR_VK_SIGNATURE(vkCmdBuildClusterAccelerationStructureIndirectNV, elixir_vk_sysv_vkCmdBuildClusterAccelerationStructureIndirectNV)
ELIXIR_VK_SIGNATURE(vkCmdBuildMicromapsEXT, elixir_vk_sysv_vkCmdBuildMicromapsEXT)
ELIXIR_VK_SIGNATURE(vkCmdBuildPartitionedAccelerationStructuresNV, elixir_vk_sysv_vkCmdBuildPartitionedAccelerationStructuresNV)
ELIXIR_VK_SIGNATURE(vkCmdClearAttachments, elixir_vk_sysv_vkCmdClearAttachments)
ELIXIR_VK_SIGNATURE(vkCmdClearColorImage, elixir_vk_sysv_vkCmdClearColorImage)
ELIXIR_VK_SIGNATURE(vkCmdClearDepthStencilImage, elixir_vk_sysv_vkCmdClearDepthStencilImage)
ELIXIR_VK_SIGNATURE(vkCmdControlVideoCodingKHR, elixir_vk_sysv_vkCmdControlVideoCodingKHR)
ELIXIR_VK_SIGNATURE(vkCmdConvertCooperativeVectorMatrixNV, elixir_vk_sysv_vkCmdConvertCooperativeVectorMatrixNV)
ELIXIR_VK_SIGNATURE(vkCmdCopyAccelerationStructureKHR, elixir_vk_sysv_vkCmdCopyAccelerationStructureKHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyAccelerationStructureNV, elixir_vk_sysv_vkCmdCopyAccelerationStructureNV)
ELIXIR_VK_SIGNATURE(vkCmdCopyAccelerationStructureToMemoryKHR, elixir_vk_sysv_vkCmdCopyAccelerationStructureToMemoryKHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyBuffer, elixir_vk_sysv_vkCmdCopyBuffer)
ELIXIR_VK_SIGNATURE(vkCmdCopyBuffer2, elixir_vk_sysv_vkCmdCopyBuffer2)
ELIXIR_VK_SIGNATURE(vkCmdCopyBuffer2KHR, elixir_vk_sysv_vkCmdCopyBuffer2KHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyBufferToImage, elixir_vk_intercept_vkCmdCopyBufferToImage)
ELIXIR_VK_SIGNATURE(vkCmdCopyBufferToImage2, elixir_vk_intercept_vkCmdCopyBufferToImage2)
ELIXIR_VK_SIGNATURE(vkCmdCopyBufferToImage2KHR, elixir_vk_intercept_vkCmdCopyBufferToImage2KHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyGpaSessionResultsAMD, elixir_vk_sysv_vkCmdCopyGpaSessionResultsAMD)
ELIXIR_VK_SIGNATURE(vkCmdCopyImage, elixir_vk_intercept_vkCmdCopyImage)
ELIXIR_VK_SIGNATURE(vkCmdCopyImage2, elixir_vk_intercept_vkCmdCopyImage2)
ELIXIR_VK_SIGNATURE(vkCmdCopyImage2KHR, elixir_vk_intercept_vkCmdCopyImage2KHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyImageToBuffer, elixir_vk_intercept_vkCmdCopyImageToBuffer)
ELIXIR_VK_SIGNATURE(vkCmdCopyImageToBuffer2, elixir_vk_intercept_vkCmdCopyImageToBuffer2)
ELIXIR_VK_SIGNATURE(vkCmdCopyImageToBuffer2KHR, elixir_vk_intercept_vkCmdCopyImageToBuffer2KHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyImageToMemoryKHR, elixir_vk_sysv_vkCmdCopyImageToMemoryKHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyMemoryIndirectKHR, elixir_vk_sysv_vkCmdCopyMemoryIndirectKHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyMemoryIndirectNV, elixir_vk_sysv_vkCmdCopyMemoryIndirectNV)
ELIXIR_VK_SIGNATURE(vkCmdCopyMemoryKHR, elixir_vk_sysv_vkCmdCopyMemoryKHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyMemoryToAccelerationStructureKHR, elixir_vk_sysv_vkCmdCopyMemoryToAccelerationStructureKHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyMemoryToImageIndirectKHR, elixir_vk_sysv_vkCmdCopyMemoryToImageIndirectKHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyMemoryToImageIndirectNV, elixir_vk_sysv_vkCmdCopyMemoryToImageIndirectNV)
ELIXIR_VK_SIGNATURE(vkCmdCopyMemoryToImageKHR, elixir_vk_sysv_vkCmdCopyMemoryToImageKHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyMemoryToMicromapEXT, elixir_vk_sysv_vkCmdCopyMemoryToMicromapEXT)
ELIXIR_VK_SIGNATURE(vkCmdCopyMicromapEXT, elixir_vk_sysv_vkCmdCopyMicromapEXT)
ELIXIR_VK_SIGNATURE(vkCmdCopyMicromapToMemoryEXT, elixir_vk_sysv_vkCmdCopyMicromapToMemoryEXT)
ELIXIR_VK_SIGNATURE(vkCmdCopyQueryPoolResults, elixir_vk_sysv_vkCmdCopyQueryPoolResults)
ELIXIR_VK_SIGNATURE(vkCmdCopyQueryPoolResultsToMemoryKHR, elixir_vk_sysv_vkCmdCopyQueryPoolResultsToMemoryKHR)
ELIXIR_VK_SIGNATURE(vkCmdCopyTensorARM, elixir_vk_sysv_vkCmdCopyTensorARM)
ELIXIR_VK_SIGNATURE(vkCmdCuLaunchKernelNVX, elixir_vk_sysv_vkCmdCuLaunchKernelNVX)
ELIXIR_VK_SIGNATURE(vkCmdCudaLaunchKernelNV, elixir_vk_sysv_vkCmdCudaLaunchKernelNV)
ELIXIR_VK_SIGNATURE(vkCmdDebugMarkerBeginEXT, elixir_vk_sysv_vkCmdDebugMarkerBeginEXT)
ELIXIR_VK_SIGNATURE(vkCmdDebugMarkerEndEXT, elixir_vk_sysv_vkCmdDebugMarkerEndEXT)
ELIXIR_VK_SIGNATURE(vkCmdDebugMarkerInsertEXT, elixir_vk_sysv_vkCmdDebugMarkerInsertEXT)
ELIXIR_VK_SIGNATURE(vkCmdDecodeVideoKHR, elixir_vk_sysv_vkCmdDecodeVideoKHR)
ELIXIR_VK_SIGNATURE(vkCmdDecompressMemoryEXT, elixir_vk_sysv_vkCmdDecompressMemoryEXT)
ELIXIR_VK_SIGNATURE(vkCmdDecompressMemoryIndirectCountEXT, elixir_vk_sysv_vkCmdDecompressMemoryIndirectCountEXT)
ELIXIR_VK_SIGNATURE(vkCmdDecompressMemoryIndirectCountNV, elixir_vk_sysv_vkCmdDecompressMemoryIndirectCountNV)
ELIXIR_VK_SIGNATURE(vkCmdDecompressMemoryNV, elixir_vk_sysv_vkCmdDecompressMemoryNV)
ELIXIR_VK_SIGNATURE(vkCmdDispatch, elixir_vk_sysv_vkCmdDispatch)
ELIXIR_VK_SIGNATURE(vkCmdDispatchBase, elixir_vk_sysv_vkCmdDispatchBase)
ELIXIR_VK_SIGNATURE(vkCmdDispatchBaseKHR, elixir_vk_sysv_vkCmdDispatchBaseKHR)
ELIXIR_VK_SIGNATURE(vkCmdDispatchDataGraphARM, elixir_vk_sysv_vkCmdDispatchDataGraphARM)
ELIXIR_VK_SIGNATURE(vkCmdDispatchGraphAMDX, elixir_vk_sysv_vkCmdDispatchGraphAMDX)
ELIXIR_VK_SIGNATURE(vkCmdDispatchGraphIndirectAMDX, elixir_vk_sysv_vkCmdDispatchGraphIndirectAMDX)
ELIXIR_VK_SIGNATURE(vkCmdDispatchGraphIndirectCountAMDX, elixir_vk_sysv_vkCmdDispatchGraphIndirectCountAMDX)
ELIXIR_VK_SIGNATURE(vkCmdDispatchIndirect, elixir_vk_sysv_vkCmdDispatchIndirect)
ELIXIR_VK_SIGNATURE(vkCmdDispatchIndirect2KHR, elixir_vk_sysv_vkCmdDispatchIndirect2KHR)
ELIXIR_VK_SIGNATURE(vkCmdDispatchTileQCOM, elixir_vk_sysv_vkCmdDispatchTileQCOM)
ELIXIR_VK_SIGNATURE(vkCmdDraw, elixir_vk_sysv_vkCmdDraw)
ELIXIR_VK_SIGNATURE(vkCmdDrawClusterHUAWEI, elixir_vk_sysv_vkCmdDrawClusterHUAWEI)
ELIXIR_VK_SIGNATURE(vkCmdDrawClusterIndirectHUAWEI, elixir_vk_sysv_vkCmdDrawClusterIndirectHUAWEI)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndexed, elixir_vk_sysv_vkCmdDrawIndexed)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndexedIndirect, elixir_vk_sysv_vkCmdDrawIndexedIndirect)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndexedIndirect2KHR, elixir_vk_sysv_vkCmdDrawIndexedIndirect2KHR)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndexedIndirectCount, elixir_vk_sysv_vkCmdDrawIndexedIndirectCount)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndexedIndirectCount2KHR, elixir_vk_sysv_vkCmdDrawIndexedIndirectCount2KHR)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndexedIndirectCountAMD, elixir_vk_sysv_vkCmdDrawIndexedIndirectCountAMD)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndexedIndirectCountKHR, elixir_vk_sysv_vkCmdDrawIndexedIndirectCountKHR)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndirect, elixir_vk_sysv_vkCmdDrawIndirect)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndirect2KHR, elixir_vk_sysv_vkCmdDrawIndirect2KHR)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndirectByteCount2EXT, elixir_vk_sysv_vkCmdDrawIndirectByteCount2EXT)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndirectByteCountEXT, elixir_vk_sysv_vkCmdDrawIndirectByteCountEXT)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndirectCount, elixir_vk_sysv_vkCmdDrawIndirectCount)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndirectCount2KHR, elixir_vk_sysv_vkCmdDrawIndirectCount2KHR)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndirectCountAMD, elixir_vk_sysv_vkCmdDrawIndirectCountAMD)
ELIXIR_VK_SIGNATURE(vkCmdDrawIndirectCountKHR, elixir_vk_sysv_vkCmdDrawIndirectCountKHR)
ELIXIR_VK_SIGNATURE(vkCmdDrawMeshTasksEXT, elixir_vk_sysv_vkCmdDrawMeshTasksEXT)
ELIXIR_VK_SIGNATURE(vkCmdDrawMeshTasksIndirect2EXT, elixir_vk_sysv_vkCmdDrawMeshTasksIndirect2EXT)
ELIXIR_VK_SIGNATURE(vkCmdDrawMeshTasksIndirectCount2EXT, elixir_vk_sysv_vkCmdDrawMeshTasksIndirectCount2EXT)
ELIXIR_VK_SIGNATURE(vkCmdDrawMeshTasksIndirectCountEXT, elixir_vk_sysv_vkCmdDrawMeshTasksIndirectCountEXT)
ELIXIR_VK_SIGNATURE(vkCmdDrawMeshTasksIndirectCountNV, elixir_vk_sysv_vkCmdDrawMeshTasksIndirectCountNV)
ELIXIR_VK_SIGNATURE(vkCmdDrawMeshTasksIndirectEXT, elixir_vk_sysv_vkCmdDrawMeshTasksIndirectEXT)
ELIXIR_VK_SIGNATURE(vkCmdDrawMeshTasksIndirectNV, elixir_vk_sysv_vkCmdDrawMeshTasksIndirectNV)
ELIXIR_VK_SIGNATURE(vkCmdDrawMeshTasksNV, elixir_vk_sysv_vkCmdDrawMeshTasksNV)
ELIXIR_VK_SIGNATURE(vkCmdDrawMultiEXT, elixir_vk_sysv_vkCmdDrawMultiEXT)
ELIXIR_VK_SIGNATURE(vkCmdDrawMultiIndexedEXT, elixir_vk_sysv_vkCmdDrawMultiIndexedEXT)
ELIXIR_VK_SIGNATURE(vkCmdEncodeVideoKHR, elixir_vk_sysv_vkCmdEncodeVideoKHR)
ELIXIR_VK_SIGNATURE(vkCmdEndConditionalRenderingEXT, elixir_vk_sysv_vkCmdEndConditionalRenderingEXT)
ELIXIR_VK_SIGNATURE(vkCmdEndDebugUtilsLabelEXT, elixir_vk_sysv_vkCmdEndDebugUtilsLabelEXT)
ELIXIR_VK_SIGNATURE(vkCmdEndGpaSampleAMD, elixir_vk_sysv_vkCmdEndGpaSampleAMD)
ELIXIR_VK_SIGNATURE(vkCmdEndGpaSessionAMD, elixir_vk_sysv_vkCmdEndGpaSessionAMD)
ELIXIR_VK_SIGNATURE(vkCmdEndPerTileExecutionQCOM, elixir_vk_sysv_vkCmdEndPerTileExecutionQCOM)
ELIXIR_VK_SIGNATURE(vkCmdEndQuery, elixir_vk_sysv_vkCmdEndQuery)
ELIXIR_VK_SIGNATURE(vkCmdEndQueryIndexedEXT, elixir_vk_sysv_vkCmdEndQueryIndexedEXT)
ELIXIR_VK_SIGNATURE(vkCmdEndRenderPass, elixir_vk_sysv_vkCmdEndRenderPass)
ELIXIR_VK_SIGNATURE(vkCmdEndRenderPass2, elixir_vk_sysv_vkCmdEndRenderPass2)
ELIXIR_VK_SIGNATURE(vkCmdEndRenderPass2KHR, elixir_vk_sysv_vkCmdEndRenderPass2KHR)
ELIXIR_VK_SIGNATURE(vkCmdEndRendering, elixir_vk_sysv_vkCmdEndRendering)
ELIXIR_VK_SIGNATURE(vkCmdEndRendering2EXT, elixir_vk_sysv_vkCmdEndRendering2EXT)
ELIXIR_VK_SIGNATURE(vkCmdEndRendering2KHR, elixir_vk_sysv_vkCmdEndRendering2KHR)
ELIXIR_VK_SIGNATURE(vkCmdEndRenderingKHR, elixir_vk_sysv_vkCmdEndRenderingKHR)
ELIXIR_VK_SIGNATURE(vkCmdEndShaderInstrumentationARM, elixir_vk_sysv_vkCmdEndShaderInstrumentationARM)
ELIXIR_VK_SIGNATURE(vkCmdEndTransformFeedback2EXT, elixir_vk_sysv_vkCmdEndTransformFeedback2EXT)
ELIXIR_VK_SIGNATURE(vkCmdEndTransformFeedbackEXT, elixir_vk_sysv_vkCmdEndTransformFeedbackEXT)
ELIXIR_VK_SIGNATURE(vkCmdEndVideoCodingKHR, elixir_vk_sysv_vkCmdEndVideoCodingKHR)
ELIXIR_VK_SIGNATURE(vkCmdExecuteCommands, elixir_vk_intercept_vkCmdExecuteCommands)
ELIXIR_VK_SIGNATURE(vkCmdExecuteGeneratedCommandsEXT, elixir_vk_sysv_vkCmdExecuteGeneratedCommandsEXT)
ELIXIR_VK_SIGNATURE(vkCmdExecuteGeneratedCommandsNV, elixir_vk_sysv_vkCmdExecuteGeneratedCommandsNV)
ELIXIR_VK_SIGNATURE(vkCmdFillBuffer, elixir_vk_sysv_vkCmdFillBuffer)
ELIXIR_VK_SIGNATURE(vkCmdFillMemoryKHR, elixir_vk_sysv_vkCmdFillMemoryKHR)
ELIXIR_VK_SIGNATURE(vkCmdInitializeGraphScratchMemoryAMDX, elixir_vk_sysv_vkCmdInitializeGraphScratchMemoryAMDX)
ELIXIR_VK_SIGNATURE(vkCmdInsertDebugUtilsLabelEXT, elixir_vk_sysv_vkCmdInsertDebugUtilsLabelEXT)
ELIXIR_VK_SIGNATURE(vkCmdNextSubpass, elixir_vk_sysv_vkCmdNextSubpass)
ELIXIR_VK_SIGNATURE(vkCmdNextSubpass2, elixir_vk_sysv_vkCmdNextSubpass2)
ELIXIR_VK_SIGNATURE(vkCmdNextSubpass2KHR, elixir_vk_sysv_vkCmdNextSubpass2KHR)
ELIXIR_VK_SIGNATURE(vkCmdOpticalFlowExecuteNV, elixir_vk_sysv_vkCmdOpticalFlowExecuteNV)
ELIXIR_VK_SIGNATURE(vkCmdPipelineBarrier, elixir_vk_sysv_vkCmdPipelineBarrier)
ELIXIR_VK_SIGNATURE(vkCmdPipelineBarrier2, elixir_vk_sysv_vkCmdPipelineBarrier2)
ELIXIR_VK_SIGNATURE(vkCmdPipelineBarrier2KHR, elixir_vk_sysv_vkCmdPipelineBarrier2KHR)
ELIXIR_VK_SIGNATURE(vkCmdPreprocessGeneratedCommandsEXT, elixir_vk_sysv_vkCmdPreprocessGeneratedCommandsEXT)
ELIXIR_VK_SIGNATURE(vkCmdPreprocessGeneratedCommandsNV, elixir_vk_sysv_vkCmdPreprocessGeneratedCommandsNV)
ELIXIR_VK_SIGNATURE(vkCmdPushConstants, elixir_vk_sysv_vkCmdPushConstants)
ELIXIR_VK_SIGNATURE(vkCmdPushConstants2, elixir_vk_sysv_vkCmdPushConstants2)
ELIXIR_VK_SIGNATURE(vkCmdPushConstants2KHR, elixir_vk_sysv_vkCmdPushConstants2KHR)
ELIXIR_VK_SIGNATURE(vkCmdPushDataEXT, elixir_vk_sysv_vkCmdPushDataEXT)
ELIXIR_VK_SIGNATURE(vkCmdPushDescriptorSet, elixir_vk_sysv_vkCmdPushDescriptorSet)
ELIXIR_VK_SIGNATURE(vkCmdPushDescriptorSet2, elixir_vk_sysv_vkCmdPushDescriptorSet2)
ELIXIR_VK_SIGNATURE(vkCmdPushDescriptorSet2KHR, elixir_vk_sysv_vkCmdPushDescriptorSet2KHR)
ELIXIR_VK_SIGNATURE(vkCmdPushDescriptorSetKHR, elixir_vk_sysv_vkCmdPushDescriptorSetKHR)
ELIXIR_VK_SIGNATURE(vkCmdPushDescriptorSetWithTemplate, elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplate)
ELIXIR_VK_SIGNATURE(vkCmdPushDescriptorSetWithTemplate2, elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplate2)
ELIXIR_VK_SIGNATURE(vkCmdPushDescriptorSetWithTemplate2KHR, elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplate2KHR)
ELIXIR_VK_SIGNATURE(vkCmdPushDescriptorSetWithTemplateKHR, elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplateKHR)
ELIXIR_VK_SIGNATURE(vkCmdResetEvent, elixir_vk_sysv_vkCmdResetEvent)
ELIXIR_VK_SIGNATURE(vkCmdResetEvent2, elixir_vk_sysv_vkCmdResetEvent2)
ELIXIR_VK_SIGNATURE(vkCmdResetEvent2KHR, elixir_vk_sysv_vkCmdResetEvent2KHR)
ELIXIR_VK_SIGNATURE(vkCmdResetQueryPool, elixir_vk_sysv_vkCmdResetQueryPool)
ELIXIR_VK_SIGNATURE(vkCmdResolveImage, elixir_vk_sysv_vkCmdResolveImage)
ELIXIR_VK_SIGNATURE(vkCmdResolveImage2, elixir_vk_sysv_vkCmdResolveImage2)
ELIXIR_VK_SIGNATURE(vkCmdResolveImage2KHR, elixir_vk_sysv_vkCmdResolveImage2KHR)
ELIXIR_VK_SIGNATURE(vkCmdSetAlphaToCoverageEnableEXT, elixir_vk_sysv_vkCmdSetAlphaToCoverageEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetAlphaToOneEnableEXT, elixir_vk_sysv_vkCmdSetAlphaToOneEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetAttachmentFeedbackLoopEnableEXT, elixir_vk_sysv_vkCmdSetAttachmentFeedbackLoopEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetBlendConstants, elixir_vk_sysv_vkCmdSetBlendConstants)
ELIXIR_VK_SIGNATURE(vkCmdSetCheckpointNV, elixir_vk_sysv_vkCmdSetCheckpointNV)
ELIXIR_VK_SIGNATURE(vkCmdSetCoarseSampleOrderNV, elixir_vk_sysv_vkCmdSetCoarseSampleOrderNV)
ELIXIR_VK_SIGNATURE(vkCmdSetColorBlendAdvancedEXT, elixir_vk_sysv_vkCmdSetColorBlendAdvancedEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetColorBlendEnableEXT, elixir_vk_sysv_vkCmdSetColorBlendEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetColorBlendEquationEXT, elixir_vk_sysv_vkCmdSetColorBlendEquationEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetColorWriteEnableEXT, elixir_vk_sysv_vkCmdSetColorWriteEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetColorWriteMaskEXT, elixir_vk_sysv_vkCmdSetColorWriteMaskEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetComputeOccupancyPriorityNV, elixir_vk_sysv_vkCmdSetComputeOccupancyPriorityNV)
ELIXIR_VK_SIGNATURE(vkCmdSetConservativeRasterizationModeEXT, elixir_vk_sysv_vkCmdSetConservativeRasterizationModeEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetCoverageModulationModeNV, elixir_vk_sysv_vkCmdSetCoverageModulationModeNV)
ELIXIR_VK_SIGNATURE(vkCmdSetCoverageModulationTableEnableNV, elixir_vk_sysv_vkCmdSetCoverageModulationTableEnableNV)
ELIXIR_VK_SIGNATURE(vkCmdSetCoverageModulationTableNV, elixir_vk_sysv_vkCmdSetCoverageModulationTableNV)
ELIXIR_VK_SIGNATURE(vkCmdSetCoverageReductionModeNV, elixir_vk_sysv_vkCmdSetCoverageReductionModeNV)
ELIXIR_VK_SIGNATURE(vkCmdSetCoverageToColorEnableNV, elixir_vk_sysv_vkCmdSetCoverageToColorEnableNV)
ELIXIR_VK_SIGNATURE(vkCmdSetCoverageToColorLocationNV, elixir_vk_sysv_vkCmdSetCoverageToColorLocationNV)
ELIXIR_VK_SIGNATURE(vkCmdSetCullMode, elixir_vk_sysv_vkCmdSetCullMode)
ELIXIR_VK_SIGNATURE(vkCmdSetCullModeEXT, elixir_vk_sysv_vkCmdSetCullModeEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthBias, elixir_vk_sysv_vkCmdSetDepthBias)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthBias2EXT, elixir_vk_sysv_vkCmdSetDepthBias2EXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthBiasEnable, elixir_vk_sysv_vkCmdSetDepthBiasEnable)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthBiasEnableEXT, elixir_vk_sysv_vkCmdSetDepthBiasEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthBounds, elixir_vk_sysv_vkCmdSetDepthBounds)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthBoundsTestEnable, elixir_vk_sysv_vkCmdSetDepthBoundsTestEnable)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthBoundsTestEnableEXT, elixir_vk_sysv_vkCmdSetDepthBoundsTestEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthClampEnableEXT, elixir_vk_sysv_vkCmdSetDepthClampEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthClampRangeEXT, elixir_vk_sysv_vkCmdSetDepthClampRangeEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthClipEnableEXT, elixir_vk_sysv_vkCmdSetDepthClipEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthClipNegativeOneToOneEXT, elixir_vk_sysv_vkCmdSetDepthClipNegativeOneToOneEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthCompareOp, elixir_vk_sysv_vkCmdSetDepthCompareOp)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthCompareOpEXT, elixir_vk_sysv_vkCmdSetDepthCompareOpEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthTestEnable, elixir_vk_sysv_vkCmdSetDepthTestEnable)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthTestEnableEXT, elixir_vk_sysv_vkCmdSetDepthTestEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthWriteEnable, elixir_vk_sysv_vkCmdSetDepthWriteEnable)
ELIXIR_VK_SIGNATURE(vkCmdSetDepthWriteEnableEXT, elixir_vk_sysv_vkCmdSetDepthWriteEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDescriptorBufferOffsets2EXT, elixir_vk_sysv_vkCmdSetDescriptorBufferOffsets2EXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDescriptorBufferOffsetsEXT, elixir_vk_sysv_vkCmdSetDescriptorBufferOffsetsEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDeviceMask, elixir_vk_sysv_vkCmdSetDeviceMask)
ELIXIR_VK_SIGNATURE(vkCmdSetDeviceMaskKHR, elixir_vk_sysv_vkCmdSetDeviceMaskKHR)
ELIXIR_VK_SIGNATURE(vkCmdSetDiscardRectangleEXT, elixir_vk_sysv_vkCmdSetDiscardRectangleEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDiscardRectangleEnableEXT, elixir_vk_sysv_vkCmdSetDiscardRectangleEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDiscardRectangleModeEXT, elixir_vk_sysv_vkCmdSetDiscardRectangleModeEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetDispatchParametersARM, elixir_vk_sysv_vkCmdSetDispatchParametersARM)
ELIXIR_VK_SIGNATURE(vkCmdSetEvent, elixir_vk_sysv_vkCmdSetEvent)
ELIXIR_VK_SIGNATURE(vkCmdSetEvent2, elixir_vk_sysv_vkCmdSetEvent2)
ELIXIR_VK_SIGNATURE(vkCmdSetEvent2KHR, elixir_vk_sysv_vkCmdSetEvent2KHR)
ELIXIR_VK_SIGNATURE(vkCmdSetExclusiveScissorEnableNV, elixir_vk_sysv_vkCmdSetExclusiveScissorEnableNV)
ELIXIR_VK_SIGNATURE(vkCmdSetExclusiveScissorNV, elixir_vk_sysv_vkCmdSetExclusiveScissorNV)
ELIXIR_VK_SIGNATURE(vkCmdSetExtraPrimitiveOverestimationSizeEXT, elixir_vk_sysv_vkCmdSetExtraPrimitiveOverestimationSizeEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetFragmentShadingRateEnumNV, elixir_vk_sysv_vkCmdSetFragmentShadingRateEnumNV)
ELIXIR_VK_SIGNATURE(vkCmdSetFragmentShadingRateKHR, elixir_vk_sysv_vkCmdSetFragmentShadingRateKHR)
ELIXIR_VK_SIGNATURE(vkCmdSetFrontFace, elixir_vk_sysv_vkCmdSetFrontFace)
ELIXIR_VK_SIGNATURE(vkCmdSetFrontFaceEXT, elixir_vk_sysv_vkCmdSetFrontFaceEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetLineRasterizationModeEXT, elixir_vk_sysv_vkCmdSetLineRasterizationModeEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetLineStipple, elixir_vk_sysv_vkCmdSetLineStipple)
ELIXIR_VK_SIGNATURE(vkCmdSetLineStippleEXT, elixir_vk_sysv_vkCmdSetLineStippleEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetLineStippleEnableEXT, elixir_vk_sysv_vkCmdSetLineStippleEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetLineStippleKHR, elixir_vk_sysv_vkCmdSetLineStippleKHR)
ELIXIR_VK_SIGNATURE(vkCmdSetLineWidth, elixir_vk_sysv_vkCmdSetLineWidth)
ELIXIR_VK_SIGNATURE(vkCmdSetLogicOpEXT, elixir_vk_sysv_vkCmdSetLogicOpEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetLogicOpEnableEXT, elixir_vk_sysv_vkCmdSetLogicOpEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetPatchControlPointsEXT, elixir_vk_sysv_vkCmdSetPatchControlPointsEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetPerformanceMarkerINTEL, elixir_vk_sysv_vkCmdSetPerformanceMarkerINTEL)
ELIXIR_VK_SIGNATURE(vkCmdSetPerformanceOverrideINTEL, elixir_vk_sysv_vkCmdSetPerformanceOverrideINTEL)
ELIXIR_VK_SIGNATURE(vkCmdSetPerformanceStreamMarkerINTEL, elixir_vk_sysv_vkCmdSetPerformanceStreamMarkerINTEL)
ELIXIR_VK_SIGNATURE(vkCmdSetPolygonModeEXT, elixir_vk_sysv_vkCmdSetPolygonModeEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetPrimitiveRestartEnable, elixir_vk_sysv_vkCmdSetPrimitiveRestartEnable)
ELIXIR_VK_SIGNATURE(vkCmdSetPrimitiveRestartEnableEXT, elixir_vk_sysv_vkCmdSetPrimitiveRestartEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetPrimitiveRestartIndexEXT, elixir_vk_sysv_vkCmdSetPrimitiveRestartIndexEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetPrimitiveTopology, elixir_vk_sysv_vkCmdSetPrimitiveTopology)
ELIXIR_VK_SIGNATURE(vkCmdSetPrimitiveTopologyEXT, elixir_vk_sysv_vkCmdSetPrimitiveTopologyEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetProvokingVertexModeEXT, elixir_vk_sysv_vkCmdSetProvokingVertexModeEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetRasterizationSamplesEXT, elixir_vk_sysv_vkCmdSetRasterizationSamplesEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetRasterizationStreamEXT, elixir_vk_sysv_vkCmdSetRasterizationStreamEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetRasterizerDiscardEnable, elixir_vk_sysv_vkCmdSetRasterizerDiscardEnable)
ELIXIR_VK_SIGNATURE(vkCmdSetRasterizerDiscardEnableEXT, elixir_vk_sysv_vkCmdSetRasterizerDiscardEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetRayTracingPipelineStackSizeKHR, elixir_vk_sysv_vkCmdSetRayTracingPipelineStackSizeKHR)
ELIXIR_VK_SIGNATURE(vkCmdSetRenderingAttachmentLocations, elixir_vk_sysv_vkCmdSetRenderingAttachmentLocations)
ELIXIR_VK_SIGNATURE(vkCmdSetRenderingAttachmentLocationsKHR, elixir_vk_sysv_vkCmdSetRenderingAttachmentLocationsKHR)
ELIXIR_VK_SIGNATURE(vkCmdSetRenderingInputAttachmentIndices, elixir_vk_sysv_vkCmdSetRenderingInputAttachmentIndices)
ELIXIR_VK_SIGNATURE(vkCmdSetRenderingInputAttachmentIndicesKHR, elixir_vk_sysv_vkCmdSetRenderingInputAttachmentIndicesKHR)
ELIXIR_VK_SIGNATURE(vkCmdSetRepresentativeFragmentTestEnableNV, elixir_vk_sysv_vkCmdSetRepresentativeFragmentTestEnableNV)
ELIXIR_VK_SIGNATURE(vkCmdSetSampleLocationsEXT, elixir_vk_sysv_vkCmdSetSampleLocationsEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetSampleLocationsEnableEXT, elixir_vk_sysv_vkCmdSetSampleLocationsEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetSampleMaskEXT, elixir_vk_sysv_vkCmdSetSampleMaskEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetScissor, elixir_vk_sysv_vkCmdSetScissor)
ELIXIR_VK_SIGNATURE(vkCmdSetScissorWithCount, elixir_vk_sysv_vkCmdSetScissorWithCount)
ELIXIR_VK_SIGNATURE(vkCmdSetScissorWithCountEXT, elixir_vk_sysv_vkCmdSetScissorWithCountEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetShadingRateImageEnableNV, elixir_vk_sysv_vkCmdSetShadingRateImageEnableNV)
ELIXIR_VK_SIGNATURE(vkCmdSetStencilCompareMask, elixir_vk_sysv_vkCmdSetStencilCompareMask)
ELIXIR_VK_SIGNATURE(vkCmdSetStencilOp, elixir_vk_sysv_vkCmdSetStencilOp)
ELIXIR_VK_SIGNATURE(vkCmdSetStencilOpEXT, elixir_vk_sysv_vkCmdSetStencilOpEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetStencilReference, elixir_vk_sysv_vkCmdSetStencilReference)
ELIXIR_VK_SIGNATURE(vkCmdSetStencilTestEnable, elixir_vk_sysv_vkCmdSetStencilTestEnable)
ELIXIR_VK_SIGNATURE(vkCmdSetStencilTestEnableEXT, elixir_vk_sysv_vkCmdSetStencilTestEnableEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetStencilWriteMask, elixir_vk_sysv_vkCmdSetStencilWriteMask)
ELIXIR_VK_SIGNATURE(vkCmdSetTessellationDomainOriginEXT, elixir_vk_sysv_vkCmdSetTessellationDomainOriginEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetVertexInputEXT, elixir_vk_sysv_vkCmdSetVertexInputEXT)
ELIXIR_VK_SIGNATURE(vkCmdSetViewport, elixir_vk_sysv_vkCmdSetViewport)
ELIXIR_VK_SIGNATURE(vkCmdSetViewportShadingRatePaletteNV, elixir_vk_sysv_vkCmdSetViewportShadingRatePaletteNV)
ELIXIR_VK_SIGNATURE(vkCmdSetViewportSwizzleNV, elixir_vk_sysv_vkCmdSetViewportSwizzleNV)
ELIXIR_VK_SIGNATURE(vkCmdSetViewportWScalingEnableNV, elixir_vk_sysv_vkCmdSetViewportWScalingEnableNV)
ELIXIR_VK_SIGNATURE(vkCmdSetViewportWScalingNV, elixir_vk_sysv_vkCmdSetViewportWScalingNV)
ELIXIR_VK_SIGNATURE(vkCmdSetViewportWithCount, elixir_vk_sysv_vkCmdSetViewportWithCount)
ELIXIR_VK_SIGNATURE(vkCmdSetViewportWithCountEXT, elixir_vk_sysv_vkCmdSetViewportWithCountEXT)
ELIXIR_VK_SIGNATURE(vkCmdSubpassShadingHUAWEI, elixir_vk_sysv_vkCmdSubpassShadingHUAWEI)
ELIXIR_VK_SIGNATURE(vkCmdTraceRaysIndirect2KHR, elixir_vk_sysv_vkCmdTraceRaysIndirect2KHR)
ELIXIR_VK_SIGNATURE(vkCmdTraceRaysIndirectKHR, elixir_vk_sysv_vkCmdTraceRaysIndirectKHR)
ELIXIR_VK_SIGNATURE(vkCmdTraceRaysKHR, elixir_vk_sysv_vkCmdTraceRaysKHR)
ELIXIR_VK_SIGNATURE(vkCmdTraceRaysNV, elixir_vk_sysv_vkCmdTraceRaysNV)
ELIXIR_VK_SIGNATURE(vkCmdUpdateBuffer, elixir_vk_sysv_vkCmdUpdateBuffer)
ELIXIR_VK_SIGNATURE(vkCmdUpdateMemoryKHR, elixir_vk_sysv_vkCmdUpdateMemoryKHR)
ELIXIR_VK_SIGNATURE(vkCmdUpdatePipelineIndirectBufferNV, elixir_vk_sysv_vkCmdUpdatePipelineIndirectBufferNV)
ELIXIR_VK_SIGNATURE(vkCmdWaitEvents, elixir_vk_sysv_vkCmdWaitEvents)
ELIXIR_VK_SIGNATURE(vkCmdWaitEvents2, elixir_vk_sysv_vkCmdWaitEvents2)
ELIXIR_VK_SIGNATURE(vkCmdWaitEvents2KHR, elixir_vk_sysv_vkCmdWaitEvents2KHR)
ELIXIR_VK_SIGNATURE(vkCmdWriteAccelerationStructuresPropertiesKHR, elixir_vk_sysv_vkCmdWriteAccelerationStructuresPropertiesKHR)
ELIXIR_VK_SIGNATURE(vkCmdWriteAccelerationStructuresPropertiesNV, elixir_vk_sysv_vkCmdWriteAccelerationStructuresPropertiesNV)
ELIXIR_VK_SIGNATURE(vkCmdWriteBufferMarker2AMD, elixir_vk_sysv_vkCmdWriteBufferMarker2AMD)
ELIXIR_VK_SIGNATURE(vkCmdWriteBufferMarkerAMD, elixir_vk_sysv_vkCmdWriteBufferMarkerAMD)
ELIXIR_VK_SIGNATURE(vkCmdWriteMarkerToMemoryAMD, elixir_vk_sysv_vkCmdWriteMarkerToMemoryAMD)
ELIXIR_VK_SIGNATURE(vkCmdWriteMicromapsPropertiesEXT, elixir_vk_sysv_vkCmdWriteMicromapsPropertiesEXT)
ELIXIR_VK_SIGNATURE(vkCmdWriteTimestamp, elixir_vk_sysv_vkCmdWriteTimestamp)
ELIXIR_VK_SIGNATURE(vkCmdWriteTimestamp2, elixir_vk_sysv_vkCmdWriteTimestamp2)
ELIXIR_VK_SIGNATURE(vkCmdWriteTimestamp2KHR, elixir_vk_sysv_vkCmdWriteTimestamp2KHR)
ELIXIR_VK_SIGNATURE(vkCompileDeferredNV, elixir_vk_sysv_vkCompileDeferredNV)
ELIXIR_VK_SIGNATURE(vkConvertCooperativeVectorMatrixNV, elixir_vk_sysv_vkConvertCooperativeVectorMatrixNV)
ELIXIR_VK_SIGNATURE(vkCopyAccelerationStructureKHR, elixir_vk_sysv_vkCopyAccelerationStructureKHR)
ELIXIR_VK_SIGNATURE(vkCopyAccelerationStructureToMemoryKHR, elixir_vk_sysv_vkCopyAccelerationStructureToMemoryKHR)
ELIXIR_VK_SIGNATURE(vkCopyImageToImage, elixir_vk_sysv_vkCopyImageToImage)
ELIXIR_VK_SIGNATURE(vkCopyImageToImageEXT, elixir_vk_sysv_vkCopyImageToImageEXT)
ELIXIR_VK_SIGNATURE(vkCopyImageToMemory, elixir_vk_sysv_vkCopyImageToMemory)
ELIXIR_VK_SIGNATURE(vkCopyImageToMemoryEXT, elixir_vk_sysv_vkCopyImageToMemoryEXT)
ELIXIR_VK_SIGNATURE(vkCopyMemoryToAccelerationStructureKHR, elixir_vk_sysv_vkCopyMemoryToAccelerationStructureKHR)
ELIXIR_VK_SIGNATURE(vkCopyMemoryToImage, elixir_vk_sysv_vkCopyMemoryToImage)
ELIXIR_VK_SIGNATURE(vkCopyMemoryToImageEXT, elixir_vk_sysv_vkCopyMemoryToImageEXT)
ELIXIR_VK_SIGNATURE(vkCopyMemoryToMicromapEXT, elixir_vk_sysv_vkCopyMemoryToMicromapEXT)
ELIXIR_VK_SIGNATURE(vkCopyMicromapEXT, elixir_vk_sysv_vkCopyMicromapEXT)
ELIXIR_VK_SIGNATURE(vkCopyMicromapToMemoryEXT, elixir_vk_sysv_vkCopyMicromapToMemoryEXT)
ELIXIR_VK_SIGNATURE(vkCreateAccelerationStructure2KHR, elixir_vk_sysv_vkCreateAccelerationStructure2KHR)
ELIXIR_VK_SIGNATURE(vkCreateAccelerationStructureKHR, elixir_vk_sysv_vkCreateAccelerationStructureKHR)
ELIXIR_VK_SIGNATURE(vkCreateAccelerationStructureNV, elixir_vk_sysv_vkCreateAccelerationStructureNV)
ELIXIR_VK_SIGNATURE(vkCreateAndroidSurfaceKHR, elixir_vk_intercept_vkCreateAndroidSurfaceKHR)
ELIXIR_VK_SIGNATURE(vkCreateBuffer, elixir_vk_sysv_vkCreateBuffer)
ELIXIR_VK_SIGNATURE(vkCreateBufferView, elixir_vk_sysv_vkCreateBufferView)
ELIXIR_VK_SIGNATURE(vkCreateCommandPool, elixir_vk_sysv_vkCreateCommandPool)
ELIXIR_VK_SIGNATURE(vkCreateComputePipelines, elixir_vk_sysv_vkCreateComputePipelines)
ELIXIR_VK_SIGNATURE(vkCreateCuFunctionNVX, elixir_vk_sysv_vkCreateCuFunctionNVX)
ELIXIR_VK_SIGNATURE(vkCreateCuModuleNVX, elixir_vk_sysv_vkCreateCuModuleNVX)
ELIXIR_VK_SIGNATURE(vkCreateCudaFunctionNV, elixir_vk_sysv_vkCreateCudaFunctionNV)
ELIXIR_VK_SIGNATURE(vkCreateCudaModuleNV, elixir_vk_sysv_vkCreateCudaModuleNV)
ELIXIR_VK_SIGNATURE(vkCreateDataGraphPipelineSessionARM, elixir_vk_sysv_vkCreateDataGraphPipelineSessionARM)
ELIXIR_VK_SIGNATURE(vkCreateDataGraphPipelinesARM, elixir_vk_sysv_vkCreateDataGraphPipelinesARM)
ELIXIR_VK_SIGNATURE(vkCreateDebugReportCallbackEXT, elixir_vk_intercept_vkCreateDebugReportCallbackEXT)
ELIXIR_VK_SIGNATURE(vkCreateDebugUtilsMessengerEXT, elixir_vk_intercept_vkCreateDebugUtilsMessengerEXT)
ELIXIR_VK_SIGNATURE(vkCreateDeferredOperationKHR, elixir_vk_sysv_vkCreateDeferredOperationKHR)
ELIXIR_VK_SIGNATURE(vkCreateDescriptorPool, elixir_vk_sysv_vkCreateDescriptorPool)
ELIXIR_VK_SIGNATURE(vkCreateDescriptorSetLayout, elixir_vk_sysv_vkCreateDescriptorSetLayout)
ELIXIR_VK_SIGNATURE(vkCreateDescriptorUpdateTemplate, elixir_vk_sysv_vkCreateDescriptorUpdateTemplate)
ELIXIR_VK_SIGNATURE(vkCreateDescriptorUpdateTemplateKHR, elixir_vk_sysv_vkCreateDescriptorUpdateTemplateKHR)
ELIXIR_VK_SIGNATURE(vkCreateDevice, elixir_vk_intercept_vkCreateDevice)
ELIXIR_VK_SIGNATURE(vkCreateDisplayModeKHR, elixir_vk_sysv_vkCreateDisplayModeKHR)
ELIXIR_VK_SIGNATURE(vkCreateDisplayPlaneSurfaceKHR, elixir_vk_sysv_vkCreateDisplayPlaneSurfaceKHR)
ELIXIR_VK_SIGNATURE(vkCreateEvent, elixir_vk_sysv_vkCreateEvent)
ELIXIR_VK_SIGNATURE(vkCreateExecutionGraphPipelinesAMDX, elixir_vk_sysv_vkCreateExecutionGraphPipelinesAMDX)
ELIXIR_VK_SIGNATURE(vkCreateExternalComputeQueueNV, elixir_vk_sysv_vkCreateExternalComputeQueueNV)
ELIXIR_VK_SIGNATURE(vkCreateFence, elixir_vk_sysv_vkCreateFence)
ELIXIR_VK_SIGNATURE(vkCreateFramebuffer, elixir_vk_sysv_vkCreateFramebuffer)
ELIXIR_VK_SIGNATURE(vkCreateGpaSessionAMD, elixir_vk_sysv_vkCreateGpaSessionAMD)
ELIXIR_VK_SIGNATURE(vkCreateGraphicsPipelines, elixir_vk_sysv_vkCreateGraphicsPipelines)
ELIXIR_VK_SIGNATURE(vkCreateHeadlessSurfaceEXT, elixir_vk_sysv_vkCreateHeadlessSurfaceEXT)
ELIXIR_VK_SIGNATURE(vkCreateImage, elixir_vk_intercept_vkCreateImage)
ELIXIR_VK_SIGNATURE(vkCreateImageView, elixir_vk_intercept_vkCreateImageView)
ELIXIR_VK_SIGNATURE(vkCreateIndirectCommandsLayoutEXT, elixir_vk_sysv_vkCreateIndirectCommandsLayoutEXT)
ELIXIR_VK_SIGNATURE(vkCreateIndirectCommandsLayoutNV, elixir_vk_sysv_vkCreateIndirectCommandsLayoutNV)
ELIXIR_VK_SIGNATURE(vkCreateIndirectExecutionSetEXT, elixir_vk_sysv_vkCreateIndirectExecutionSetEXT)
ELIXIR_VK_SIGNATURE(vkCreateInstance, elixir_vk_intercept_vkCreateInstance)
ELIXIR_VK_SIGNATURE(vkCreateMicromapEXT, elixir_vk_sysv_vkCreateMicromapEXT)
ELIXIR_VK_SIGNATURE(vkCreateOpticalFlowSessionNV, elixir_vk_sysv_vkCreateOpticalFlowSessionNV)
ELIXIR_VK_SIGNATURE(vkCreatePipelineBinariesKHR, elixir_vk_sysv_vkCreatePipelineBinariesKHR)
ELIXIR_VK_SIGNATURE(vkCreatePipelineCache, elixir_vk_sysv_vkCreatePipelineCache)
ELIXIR_VK_SIGNATURE(vkCreatePipelineLayout, elixir_vk_sysv_vkCreatePipelineLayout)
ELIXIR_VK_SIGNATURE(vkCreatePrivateDataSlot, elixir_vk_sysv_vkCreatePrivateDataSlot)
ELIXIR_VK_SIGNATURE(vkCreatePrivateDataSlotEXT, elixir_vk_sysv_vkCreatePrivateDataSlotEXT)
ELIXIR_VK_SIGNATURE(vkCreateQueryPool, elixir_vk_sysv_vkCreateQueryPool)
ELIXIR_VK_SIGNATURE(vkCreateRayTracingPipelinesKHR, elixir_vk_sysv_vkCreateRayTracingPipelinesKHR)
ELIXIR_VK_SIGNATURE(vkCreateRayTracingPipelinesNV, elixir_vk_sysv_vkCreateRayTracingPipelinesNV)
ELIXIR_VK_SIGNATURE(vkCreateRenderPass, elixir_vk_sysv_vkCreateRenderPass)
ELIXIR_VK_SIGNATURE(vkCreateRenderPass2, elixir_vk_sysv_vkCreateRenderPass2)
ELIXIR_VK_SIGNATURE(vkCreateRenderPass2KHR, elixir_vk_sysv_vkCreateRenderPass2KHR)
ELIXIR_VK_SIGNATURE(vkCreateSampler, elixir_vk_sysv_vkCreateSampler)
ELIXIR_VK_SIGNATURE(vkCreateSamplerYcbcrConversion, elixir_vk_sysv_vkCreateSamplerYcbcrConversion)
ELIXIR_VK_SIGNATURE(vkCreateSamplerYcbcrConversionKHR, elixir_vk_sysv_vkCreateSamplerYcbcrConversionKHR)
ELIXIR_VK_SIGNATURE(vkCreateSemaphore, elixir_vk_sysv_vkCreateSemaphore)
ELIXIR_VK_SIGNATURE(vkCreateShaderInstrumentationARM, elixir_vk_sysv_vkCreateShaderInstrumentationARM)
ELIXIR_VK_SIGNATURE(vkCreateShaderModule, elixir_vk_sysv_vkCreateShaderModule)
ELIXIR_VK_SIGNATURE(vkCreateShadersEXT, elixir_vk_sysv_vkCreateShadersEXT)
ELIXIR_VK_SIGNATURE(vkCreateSharedSwapchainsKHR, elixir_vk_sysv_vkCreateSharedSwapchainsKHR)
ELIXIR_VK_SIGNATURE(vkCreateSwapchainKHR, elixir_vk_intercept_vkCreateSwapchainKHR)
ELIXIR_VK_SIGNATURE(vkCreateTensorARM, elixir_vk_sysv_vkCreateTensorARM)
ELIXIR_VK_SIGNATURE(vkCreateTensorViewARM, elixir_vk_sysv_vkCreateTensorViewARM)
ELIXIR_VK_SIGNATURE(vkCreateValidationCacheEXT, elixir_vk_sysv_vkCreateValidationCacheEXT)
ELIXIR_VK_SIGNATURE(vkCreateVideoSessionKHR, elixir_vk_sysv_vkCreateVideoSessionKHR)
ELIXIR_VK_SIGNATURE(vkCreateVideoSessionParametersKHR, elixir_vk_sysv_vkCreateVideoSessionParametersKHR)
ELIXIR_VK_SIGNATURE(vkDebugMarkerSetObjectNameEXT, elixir_vk_sysv_vkDebugMarkerSetObjectNameEXT)
ELIXIR_VK_SIGNATURE(vkDebugMarkerSetObjectTagEXT, elixir_vk_sysv_vkDebugMarkerSetObjectTagEXT)
ELIXIR_VK_SIGNATURE(vkDebugReportMessageEXT, elixir_vk_sysv_vkDebugReportMessageEXT)
ELIXIR_VK_SIGNATURE(vkDeferredOperationJoinKHR, elixir_vk_sysv_vkDeferredOperationJoinKHR)
ELIXIR_VK_SIGNATURE(vkDestroyAccelerationStructureKHR, elixir_vk_sysv_vkDestroyAccelerationStructureKHR)
ELIXIR_VK_SIGNATURE(vkDestroyAccelerationStructureNV, elixir_vk_sysv_vkDestroyAccelerationStructureNV)
ELIXIR_VK_SIGNATURE(vkDestroyBuffer, elixir_vk_intercept_vkDestroyBuffer)
ELIXIR_VK_SIGNATURE(vkDestroyBufferView, elixir_vk_sysv_vkDestroyBufferView)
ELIXIR_VK_SIGNATURE(vkDestroyCommandPool, elixir_vk_intercept_vkDestroyCommandPool)
ELIXIR_VK_SIGNATURE(vkDestroyCuFunctionNVX, elixir_vk_sysv_vkDestroyCuFunctionNVX)
ELIXIR_VK_SIGNATURE(vkDestroyCuModuleNVX, elixir_vk_sysv_vkDestroyCuModuleNVX)
ELIXIR_VK_SIGNATURE(vkDestroyCudaFunctionNV, elixir_vk_sysv_vkDestroyCudaFunctionNV)
ELIXIR_VK_SIGNATURE(vkDestroyCudaModuleNV, elixir_vk_sysv_vkDestroyCudaModuleNV)
ELIXIR_VK_SIGNATURE(vkDestroyDataGraphPipelineSessionARM, elixir_vk_sysv_vkDestroyDataGraphPipelineSessionARM)
ELIXIR_VK_SIGNATURE(vkDestroyDebugReportCallbackEXT, elixir_vk_intercept_vkDestroyDebugReportCallbackEXT)
ELIXIR_VK_SIGNATURE(vkDestroyDebugUtilsMessengerEXT, elixir_vk_intercept_vkDestroyDebugUtilsMessengerEXT)
ELIXIR_VK_SIGNATURE(vkDestroyDeferredOperationKHR, elixir_vk_sysv_vkDestroyDeferredOperationKHR)
ELIXIR_VK_SIGNATURE(vkDestroyDescriptorPool, elixir_vk_sysv_vkDestroyDescriptorPool)
ELIXIR_VK_SIGNATURE(vkDestroyDescriptorSetLayout, elixir_vk_sysv_vkDestroyDescriptorSetLayout)
ELIXIR_VK_SIGNATURE(vkDestroyDescriptorUpdateTemplate, elixir_vk_sysv_vkDestroyDescriptorUpdateTemplate)
ELIXIR_VK_SIGNATURE(vkDestroyDescriptorUpdateTemplateKHR, elixir_vk_sysv_vkDestroyDescriptorUpdateTemplateKHR)
ELIXIR_VK_SIGNATURE(vkDestroyDevice, elixir_vk_intercept_vkDestroyDevice)
ELIXIR_VK_SIGNATURE(vkDestroyEvent, elixir_vk_sysv_vkDestroyEvent)
ELIXIR_VK_SIGNATURE(vkDestroyExternalComputeQueueNV, elixir_vk_sysv_vkDestroyExternalComputeQueueNV)
ELIXIR_VK_SIGNATURE(vkDestroyFence, elixir_vk_sysv_vkDestroyFence)
ELIXIR_VK_SIGNATURE(vkDestroyFramebuffer, elixir_vk_sysv_vkDestroyFramebuffer)
ELIXIR_VK_SIGNATURE(vkDestroyGpaSessionAMD, elixir_vk_sysv_vkDestroyGpaSessionAMD)
ELIXIR_VK_SIGNATURE(vkDestroyImage, elixir_vk_intercept_vkDestroyImage)
ELIXIR_VK_SIGNATURE(vkDestroyImageView, elixir_vk_sysv_vkDestroyImageView)
ELIXIR_VK_SIGNATURE(vkDestroyIndirectCommandsLayoutEXT, elixir_vk_sysv_vkDestroyIndirectCommandsLayoutEXT)
ELIXIR_VK_SIGNATURE(vkDestroyIndirectCommandsLayoutNV, elixir_vk_sysv_vkDestroyIndirectCommandsLayoutNV)
ELIXIR_VK_SIGNATURE(vkDestroyIndirectExecutionSetEXT, elixir_vk_sysv_vkDestroyIndirectExecutionSetEXT)
ELIXIR_VK_SIGNATURE(vkDestroyInstance, elixir_vk_intercept_vkDestroyInstance)
ELIXIR_VK_SIGNATURE(vkDestroyMicromapEXT, elixir_vk_sysv_vkDestroyMicromapEXT)
ELIXIR_VK_SIGNATURE(vkDestroyOpticalFlowSessionNV, elixir_vk_sysv_vkDestroyOpticalFlowSessionNV)
ELIXIR_VK_SIGNATURE(vkDestroyPipeline, elixir_vk_sysv_vkDestroyPipeline)
ELIXIR_VK_SIGNATURE(vkDestroyPipelineBinaryKHR, elixir_vk_sysv_vkDestroyPipelineBinaryKHR)
ELIXIR_VK_SIGNATURE(vkDestroyPipelineCache, elixir_vk_sysv_vkDestroyPipelineCache)
ELIXIR_VK_SIGNATURE(vkDestroyPipelineLayout, elixir_vk_sysv_vkDestroyPipelineLayout)
ELIXIR_VK_SIGNATURE(vkDestroyPrivateDataSlot, elixir_vk_sysv_vkDestroyPrivateDataSlot)
ELIXIR_VK_SIGNATURE(vkDestroyPrivateDataSlotEXT, elixir_vk_sysv_vkDestroyPrivateDataSlotEXT)
ELIXIR_VK_SIGNATURE(vkDestroyQueryPool, elixir_vk_sysv_vkDestroyQueryPool)
ELIXIR_VK_SIGNATURE(vkDestroyRenderPass, elixir_vk_sysv_vkDestroyRenderPass)
ELIXIR_VK_SIGNATURE(vkDestroySampler, elixir_vk_sysv_vkDestroySampler)
ELIXIR_VK_SIGNATURE(vkDestroySamplerYcbcrConversion, elixir_vk_sysv_vkDestroySamplerYcbcrConversion)
ELIXIR_VK_SIGNATURE(vkDestroySamplerYcbcrConversionKHR, elixir_vk_sysv_vkDestroySamplerYcbcrConversionKHR)
ELIXIR_VK_SIGNATURE(vkDestroySemaphore, elixir_vk_sysv_vkDestroySemaphore)
ELIXIR_VK_SIGNATURE(vkDestroyShaderEXT, elixir_vk_sysv_vkDestroyShaderEXT)
ELIXIR_VK_SIGNATURE(vkDestroyShaderInstrumentationARM, elixir_vk_sysv_vkDestroyShaderInstrumentationARM)
ELIXIR_VK_SIGNATURE(vkDestroyShaderModule, elixir_vk_sysv_vkDestroyShaderModule)
ELIXIR_VK_SIGNATURE(vkDestroySurfaceKHR, elixir_vk_sysv_vkDestroySurfaceKHR)
ELIXIR_VK_SIGNATURE(vkDestroySwapchainKHR, elixir_vk_sysv_vkDestroySwapchainKHR)
ELIXIR_VK_SIGNATURE(vkDestroyTensorARM, elixir_vk_sysv_vkDestroyTensorARM)
ELIXIR_VK_SIGNATURE(vkDestroyTensorViewARM, elixir_vk_sysv_vkDestroyTensorViewARM)
ELIXIR_VK_SIGNATURE(vkDestroyValidationCacheEXT, elixir_vk_sysv_vkDestroyValidationCacheEXT)
ELIXIR_VK_SIGNATURE(vkDestroyVideoSessionKHR, elixir_vk_sysv_vkDestroyVideoSessionKHR)
ELIXIR_VK_SIGNATURE(vkDestroyVideoSessionParametersKHR, elixir_vk_sysv_vkDestroyVideoSessionParametersKHR)
ELIXIR_VK_SIGNATURE(vkDeviceWaitIdle, elixir_vk_sysv_vkDeviceWaitIdle)
ELIXIR_VK_SIGNATURE(vkDisplayPowerControlEXT, elixir_vk_sysv_vkDisplayPowerControlEXT)
ELIXIR_VK_SIGNATURE(vkEndCommandBuffer, elixir_vk_sysv_vkEndCommandBuffer)
ELIXIR_VK_SIGNATURE(vkEnumerateDeviceExtensionProperties, elixir_vk_intercept_vkEnumerateDeviceExtensionProperties)
ELIXIR_VK_SIGNATURE(vkEnumerateDeviceLayerProperties, elixir_vk_sysv_vkEnumerateDeviceLayerProperties)
ELIXIR_VK_SIGNATURE(vkEnumerateInstanceExtensionProperties, elixir_vk_intercept_vkEnumerateInstanceExtensionProperties)
ELIXIR_VK_SIGNATURE(vkEnumerateInstanceLayerProperties, elixir_vk_intercept_vkEnumerateInstanceLayerProperties)
ELIXIR_VK_SIGNATURE(vkEnumerateInstanceVersion, elixir_vk_intercept_vkEnumerateInstanceVersion)
ELIXIR_VK_SIGNATURE(vkEnumeratePhysicalDeviceGroups, elixir_vk_intercept_vkEnumeratePhysicalDeviceGroups)
ELIXIR_VK_SIGNATURE(vkEnumeratePhysicalDeviceGroupsKHR, elixir_vk_intercept_vkEnumeratePhysicalDeviceGroupsKHR)
ELIXIR_VK_SIGNATURE(vkEnumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM, elixir_vk_sysv_vkEnumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM)
ELIXIR_VK_SIGNATURE(vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR, elixir_vk_sysv_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR)
ELIXIR_VK_SIGNATURE(vkEnumeratePhysicalDeviceShaderInstrumentationMetricsARM, elixir_vk_sysv_vkEnumeratePhysicalDeviceShaderInstrumentationMetricsARM)
ELIXIR_VK_SIGNATURE(vkEnumeratePhysicalDevices, elixir_vk_intercept_vkEnumeratePhysicalDevices)
ELIXIR_VK_SIGNATURE(vkFlushMappedMemoryRanges, elixir_vk_sysv_vkFlushMappedMemoryRanges)
ELIXIR_VK_SIGNATURE(vkFreeCommandBuffers, elixir_vk_intercept_vkFreeCommandBuffers)
ELIXIR_VK_SIGNATURE(vkFreeDescriptorSets, elixir_vk_sysv_vkFreeDescriptorSets)
ELIXIR_VK_SIGNATURE(vkFreeMemory, elixir_vk_intercept_vkFreeMemory)
ELIXIR_VK_SIGNATURE(vkGetAccelerationStructureBuildSizesKHR, elixir_vk_sysv_vkGetAccelerationStructureBuildSizesKHR)
ELIXIR_VK_SIGNATURE(vkGetAccelerationStructureDeviceAddressKHR, elixir_vk_sysv_vkGetAccelerationStructureDeviceAddressKHR)
ELIXIR_VK_SIGNATURE(vkGetAccelerationStructureHandleNV, elixir_vk_sysv_vkGetAccelerationStructureHandleNV)
ELIXIR_VK_SIGNATURE(vkGetAccelerationStructureMemoryRequirementsNV, elixir_vk_sysv_vkGetAccelerationStructureMemoryRequirementsNV)
ELIXIR_VK_SIGNATURE(vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT, elixir_vk_sysv_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT)
ELIXIR_VK_SIGNATURE(vkGetAndroidHardwareBufferPropertiesANDROID, elixir_vk_intercept_vkGetAndroidHardwareBufferPropertiesANDROID)
ELIXIR_VK_SIGNATURE(vkGetBufferDeviceAddress, elixir_vk_sysv_vkGetBufferDeviceAddress)
ELIXIR_VK_SIGNATURE(vkGetBufferDeviceAddressEXT, elixir_vk_sysv_vkGetBufferDeviceAddressEXT)
ELIXIR_VK_SIGNATURE(vkGetBufferDeviceAddressKHR, elixir_vk_sysv_vkGetBufferDeviceAddressKHR)
ELIXIR_VK_SIGNATURE(vkGetBufferMemoryRequirements, elixir_vk_sysv_vkGetBufferMemoryRequirements)
ELIXIR_VK_SIGNATURE(vkGetBufferMemoryRequirements2, elixir_vk_sysv_vkGetBufferMemoryRequirements2)
ELIXIR_VK_SIGNATURE(vkGetBufferMemoryRequirements2KHR, elixir_vk_sysv_vkGetBufferMemoryRequirements2KHR)
ELIXIR_VK_SIGNATURE(vkGetBufferOpaqueCaptureAddress, elixir_vk_sysv_vkGetBufferOpaqueCaptureAddress)
ELIXIR_VK_SIGNATURE(vkGetBufferOpaqueCaptureAddressKHR, elixir_vk_sysv_vkGetBufferOpaqueCaptureAddressKHR)
ELIXIR_VK_SIGNATURE(vkGetBufferOpaqueCaptureDescriptorDataEXT, elixir_vk_sysv_vkGetBufferOpaqueCaptureDescriptorDataEXT)
ELIXIR_VK_SIGNATURE(vkGetCalibratedTimestampsEXT, elixir_vk_sysv_vkGetCalibratedTimestampsEXT)
ELIXIR_VK_SIGNATURE(vkGetCalibratedTimestampsKHR, elixir_vk_sysv_vkGetCalibratedTimestampsKHR)
ELIXIR_VK_SIGNATURE(vkGetClusterAccelerationStructureBuildSizesNV, elixir_vk_sysv_vkGetClusterAccelerationStructureBuildSizesNV)
ELIXIR_VK_SIGNATURE(vkGetCudaModuleCacheNV, elixir_vk_sysv_vkGetCudaModuleCacheNV)
ELIXIR_VK_SIGNATURE(vkGetDataGraphPipelineAvailablePropertiesARM, elixir_vk_sysv_vkGetDataGraphPipelineAvailablePropertiesARM)
ELIXIR_VK_SIGNATURE(vkGetDataGraphPipelinePropertiesARM, elixir_vk_sysv_vkGetDataGraphPipelinePropertiesARM)
ELIXIR_VK_SIGNATURE(vkGetDataGraphPipelineSessionBindPointRequirementsARM, elixir_vk_sysv_vkGetDataGraphPipelineSessionBindPointRequirementsARM)
ELIXIR_VK_SIGNATURE(vkGetDataGraphPipelineSessionMemoryRequirementsARM, elixir_vk_sysv_vkGetDataGraphPipelineSessionMemoryRequirementsARM)
ELIXIR_VK_SIGNATURE(vkGetDeferredOperationMaxConcurrencyKHR, elixir_vk_sysv_vkGetDeferredOperationMaxConcurrencyKHR)
ELIXIR_VK_SIGNATURE(vkGetDeferredOperationResultKHR, elixir_vk_sysv_vkGetDeferredOperationResultKHR)
ELIXIR_VK_SIGNATURE(vkGetDescriptorEXT, elixir_vk_sysv_vkGetDescriptorEXT)
ELIXIR_VK_SIGNATURE(vkGetDescriptorSetHostMappingVALVE, elixir_vk_sysv_vkGetDescriptorSetHostMappingVALVE)
ELIXIR_VK_SIGNATURE(vkGetDescriptorSetLayoutBindingOffsetEXT, elixir_vk_sysv_vkGetDescriptorSetLayoutBindingOffsetEXT)
ELIXIR_VK_SIGNATURE(vkGetDescriptorSetLayoutHostMappingInfoVALVE, elixir_vk_sysv_vkGetDescriptorSetLayoutHostMappingInfoVALVE)
ELIXIR_VK_SIGNATURE(vkGetDescriptorSetLayoutSizeEXT, elixir_vk_sysv_vkGetDescriptorSetLayoutSizeEXT)
ELIXIR_VK_SIGNATURE(vkGetDescriptorSetLayoutSupport, elixir_vk_sysv_vkGetDescriptorSetLayoutSupport)
ELIXIR_VK_SIGNATURE(vkGetDescriptorSetLayoutSupportKHR, elixir_vk_sysv_vkGetDescriptorSetLayoutSupportKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceAccelerationStructureCompatibilityKHR, elixir_vk_sysv_vkGetDeviceAccelerationStructureCompatibilityKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceBufferMemoryRequirements, elixir_vk_sysv_vkGetDeviceBufferMemoryRequirements)
ELIXIR_VK_SIGNATURE(vkGetDeviceBufferMemoryRequirementsKHR, elixir_vk_sysv_vkGetDeviceBufferMemoryRequirementsKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceCombinedImageSamplerIndexNVX, elixir_vk_sysv_vkGetDeviceCombinedImageSamplerIndexNVX)
ELIXIR_VK_SIGNATURE(vkGetDeviceFaultDebugInfoKHR, elixir_vk_sysv_vkGetDeviceFaultDebugInfoKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceFaultInfoEXT, elixir_vk_sysv_vkGetDeviceFaultInfoEXT)
ELIXIR_VK_SIGNATURE(vkGetDeviceFaultReportsKHR, elixir_vk_sysv_vkGetDeviceFaultReportsKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceGroupPeerMemoryFeatures, elixir_vk_sysv_vkGetDeviceGroupPeerMemoryFeatures)
ELIXIR_VK_SIGNATURE(vkGetDeviceGroupPeerMemoryFeaturesKHR, elixir_vk_sysv_vkGetDeviceGroupPeerMemoryFeaturesKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceGroupPresentCapabilitiesKHR, elixir_vk_sysv_vkGetDeviceGroupPresentCapabilitiesKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceGroupSurfacePresentModesKHR, elixir_vk_sysv_vkGetDeviceGroupSurfacePresentModesKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceImageMemoryRequirements, elixir_vk_intercept_vkGetDeviceImageMemoryRequirements)
ELIXIR_VK_SIGNATURE(vkGetDeviceImageMemoryRequirementsKHR, elixir_vk_intercept_vkGetDeviceImageMemoryRequirementsKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceImageSparseMemoryRequirements, elixir_vk_intercept_vkGetDeviceImageSparseMemoryRequirements)
ELIXIR_VK_SIGNATURE(vkGetDeviceImageSparseMemoryRequirementsKHR, elixir_vk_intercept_vkGetDeviceImageSparseMemoryRequirementsKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceImageSubresourceLayout, elixir_vk_intercept_vkGetDeviceImageSubresourceLayout)
ELIXIR_VK_SIGNATURE(vkGetDeviceImageSubresourceLayoutKHR, elixir_vk_intercept_vkGetDeviceImageSubresourceLayoutKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceMemoryCommitment, elixir_vk_sysv_vkGetDeviceMemoryCommitment)
ELIXIR_VK_SIGNATURE(vkGetDeviceMemoryOpaqueCaptureAddress, elixir_vk_sysv_vkGetDeviceMemoryOpaqueCaptureAddress)
ELIXIR_VK_SIGNATURE(vkGetDeviceMemoryOpaqueCaptureAddressKHR, elixir_vk_sysv_vkGetDeviceMemoryOpaqueCaptureAddressKHR)
ELIXIR_VK_SIGNATURE(vkGetDeviceMicromapCompatibilityEXT, elixir_vk_sysv_vkGetDeviceMicromapCompatibilityEXT)
ELIXIR_VK_SIGNATURE(vkGetDeviceProcAddr, elixir_vk_intercept_vkGetDeviceProcAddr)
ELIXIR_VK_SIGNATURE(vkGetDeviceQueue, elixir_vk_sysv_vkGetDeviceQueue)
ELIXIR_VK_SIGNATURE(vkGetDeviceQueue2, elixir_vk_sysv_vkGetDeviceQueue2)
ELIXIR_VK_SIGNATURE(vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI, elixir_vk_sysv_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI)
ELIXIR_VK_SIGNATURE(vkGetDeviceTensorMemoryRequirementsARM, elixir_vk_sysv_vkGetDeviceTensorMemoryRequirementsARM)
ELIXIR_VK_SIGNATURE(vkGetDisplayModeProperties2KHR, elixir_vk_sysv_vkGetDisplayModeProperties2KHR)
ELIXIR_VK_SIGNATURE(vkGetDisplayModePropertiesKHR, elixir_vk_sysv_vkGetDisplayModePropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetDisplayPlaneCapabilities2KHR, elixir_vk_sysv_vkGetDisplayPlaneCapabilities2KHR)
ELIXIR_VK_SIGNATURE(vkGetDisplayPlaneCapabilitiesKHR, elixir_vk_sysv_vkGetDisplayPlaneCapabilitiesKHR)
ELIXIR_VK_SIGNATURE(vkGetDisplayPlaneSupportedDisplaysKHR, elixir_vk_sysv_vkGetDisplayPlaneSupportedDisplaysKHR)
ELIXIR_VK_SIGNATURE(vkGetDrmDisplayEXT, elixir_vk_sysv_vkGetDrmDisplayEXT)
ELIXIR_VK_SIGNATURE(vkGetDynamicRenderingTilePropertiesQCOM, elixir_vk_sysv_vkGetDynamicRenderingTilePropertiesQCOM)
ELIXIR_VK_SIGNATURE(vkGetEncodedVideoSessionParametersKHR, elixir_vk_sysv_vkGetEncodedVideoSessionParametersKHR)
ELIXIR_VK_SIGNATURE(vkGetEventStatus, elixir_vk_sysv_vkGetEventStatus)
ELIXIR_VK_SIGNATURE(vkGetExecutionGraphPipelineNodeIndexAMDX, elixir_vk_sysv_vkGetExecutionGraphPipelineNodeIndexAMDX)
ELIXIR_VK_SIGNATURE(vkGetExecutionGraphPipelineScratchSizeAMDX, elixir_vk_sysv_vkGetExecutionGraphPipelineScratchSizeAMDX)
ELIXIR_VK_SIGNATURE(vkGetExternalComputeQueueDataNV, elixir_vk_sysv_vkGetExternalComputeQueueDataNV)
ELIXIR_VK_SIGNATURE(vkGetFenceFdKHR, elixir_vk_sysv_vkGetFenceFdKHR)
ELIXIR_VK_SIGNATURE(vkGetFenceStatus, elixir_vk_sysv_vkGetFenceStatus)
ELIXIR_VK_SIGNATURE(vkGetFramebufferTilePropertiesQCOM, elixir_vk_sysv_vkGetFramebufferTilePropertiesQCOM)
ELIXIR_VK_SIGNATURE(vkGetGeneratedCommandsMemoryRequirementsEXT, elixir_vk_sysv_vkGetGeneratedCommandsMemoryRequirementsEXT)
ELIXIR_VK_SIGNATURE(vkGetGeneratedCommandsMemoryRequirementsNV, elixir_vk_sysv_vkGetGeneratedCommandsMemoryRequirementsNV)
ELIXIR_VK_SIGNATURE(vkGetGpaDeviceClockInfoAMD, elixir_vk_sysv_vkGetGpaDeviceClockInfoAMD)
ELIXIR_VK_SIGNATURE(vkGetGpaSessionResultsAMD, elixir_vk_sysv_vkGetGpaSessionResultsAMD)
ELIXIR_VK_SIGNATURE(vkGetGpaSessionStatusAMD, elixir_vk_sysv_vkGetGpaSessionStatusAMD)
ELIXIR_VK_SIGNATURE(vkGetImageDrmFormatModifierPropertiesEXT, elixir_vk_sysv_vkGetImageDrmFormatModifierPropertiesEXT)
ELIXIR_VK_SIGNATURE(vkGetImageMemoryRequirements, elixir_vk_sysv_vkGetImageMemoryRequirements)
ELIXIR_VK_SIGNATURE(vkGetImageMemoryRequirements2, elixir_vk_sysv_vkGetImageMemoryRequirements2)
ELIXIR_VK_SIGNATURE(vkGetImageMemoryRequirements2KHR, elixir_vk_sysv_vkGetImageMemoryRequirements2KHR)
ELIXIR_VK_SIGNATURE(vkGetImageOpaqueCaptureDataEXT, elixir_vk_sysv_vkGetImageOpaqueCaptureDataEXT)
ELIXIR_VK_SIGNATURE(vkGetImageOpaqueCaptureDescriptorDataEXT, elixir_vk_sysv_vkGetImageOpaqueCaptureDescriptorDataEXT)
ELIXIR_VK_SIGNATURE(vkGetImageSparseMemoryRequirements, elixir_vk_sysv_vkGetImageSparseMemoryRequirements)
ELIXIR_VK_SIGNATURE(vkGetImageSparseMemoryRequirements2, elixir_vk_sysv_vkGetImageSparseMemoryRequirements2)
ELIXIR_VK_SIGNATURE(vkGetImageSparseMemoryRequirements2KHR, elixir_vk_sysv_vkGetImageSparseMemoryRequirements2KHR)
ELIXIR_VK_SIGNATURE(vkGetImageSubresourceLayout, elixir_vk_sysv_vkGetImageSubresourceLayout)
ELIXIR_VK_SIGNATURE(vkGetImageSubresourceLayout2, elixir_vk_sysv_vkGetImageSubresourceLayout2)
ELIXIR_VK_SIGNATURE(vkGetImageSubresourceLayout2EXT, elixir_vk_sysv_vkGetImageSubresourceLayout2EXT)
ELIXIR_VK_SIGNATURE(vkGetImageSubresourceLayout2KHR, elixir_vk_sysv_vkGetImageSubresourceLayout2KHR)
ELIXIR_VK_SIGNATURE(vkGetImageViewAddressNVX, elixir_vk_sysv_vkGetImageViewAddressNVX)
ELIXIR_VK_SIGNATURE(vkGetImageViewHandle64NVX, elixir_vk_sysv_vkGetImageViewHandle64NVX)
ELIXIR_VK_SIGNATURE(vkGetImageViewHandleNVX, elixir_vk_sysv_vkGetImageViewHandleNVX)
ELIXIR_VK_SIGNATURE(vkGetImageViewOpaqueCaptureDescriptorDataEXT, elixir_vk_sysv_vkGetImageViewOpaqueCaptureDescriptorDataEXT)
ELIXIR_VK_SIGNATURE(vkGetInstanceProcAddr, elixir_vk_intercept_vkGetInstanceProcAddr)
ELIXIR_VK_SIGNATURE(vkGetLatencyTimingsNV, elixir_vk_sysv_vkGetLatencyTimingsNV)
ELIXIR_VK_SIGNATURE(vkGetMemoryAndroidHardwareBufferANDROID, elixir_vk_intercept_vkGetMemoryAndroidHardwareBufferANDROID)
ELIXIR_VK_SIGNATURE(vkGetMemoryFdKHR, elixir_vk_sysv_vkGetMemoryFdKHR)
ELIXIR_VK_SIGNATURE(vkGetMemoryFdPropertiesKHR, elixir_vk_sysv_vkGetMemoryFdPropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetMemoryHostPointerPropertiesEXT, elixir_vk_sysv_vkGetMemoryHostPointerPropertiesEXT)
ELIXIR_VK_SIGNATURE(vkGetMemoryRemoteAddressNV, elixir_vk_sysv_vkGetMemoryRemoteAddressNV)
ELIXIR_VK_SIGNATURE(vkGetMicromapBuildSizesEXT, elixir_vk_sysv_vkGetMicromapBuildSizesEXT)
ELIXIR_VK_SIGNATURE(vkGetPartitionedAccelerationStructuresBuildSizesNV, elixir_vk_sysv_vkGetPartitionedAccelerationStructuresBuildSizesNV)
ELIXIR_VK_SIGNATURE(vkGetPastPresentationTimingEXT, elixir_vk_sysv_vkGetPastPresentationTimingEXT)
ELIXIR_VK_SIGNATURE(vkGetPastPresentationTimingGOOGLE, elixir_vk_sysv_vkGetPastPresentationTimingGOOGLE)
ELIXIR_VK_SIGNATURE(vkGetPerformanceParameterINTEL, elixir_vk_sysv_vkGetPerformanceParameterINTEL)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, elixir_vk_sysv_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceCalibrateableTimeDomainsKHR, elixir_vk_sysv_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV, elixir_vk_sysv_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceCooperativeMatrixPropertiesNV, elixir_vk_sysv_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceCooperativeVectorPropertiesNV, elixir_vk_sysv_vkGetPhysicalDeviceCooperativeVectorPropertiesNV)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceDescriptorSizeEXT, elixir_vk_sysv_vkGetPhysicalDeviceDescriptorSizeEXT)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceDisplayPlaneProperties2KHR, elixir_vk_sysv_vkGetPhysicalDeviceDisplayPlaneProperties2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceDisplayPlanePropertiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceDisplayPlanePropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceDisplayProperties2KHR, elixir_vk_sysv_vkGetPhysicalDeviceDisplayProperties2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceDisplayPropertiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceDisplayPropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceExternalBufferProperties, elixir_vk_sysv_vkGetPhysicalDeviceExternalBufferProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceExternalBufferPropertiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceExternalBufferPropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceExternalFenceProperties, elixir_vk_sysv_vkGetPhysicalDeviceExternalFenceProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceExternalFencePropertiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceExternalFencePropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceExternalImageFormatPropertiesNV, elixir_vk_sysv_vkGetPhysicalDeviceExternalImageFormatPropertiesNV)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceExternalSemaphoreProperties, elixir_vk_sysv_vkGetPhysicalDeviceExternalSemaphoreProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceExternalTensorPropertiesARM, elixir_vk_sysv_vkGetPhysicalDeviceExternalTensorPropertiesARM)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceFeatures, elixir_vk_intercept_vkGetPhysicalDeviceFeatures)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceFeatures2, elixir_vk_intercept_vkGetPhysicalDeviceFeatures2)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceFeatures2KHR, elixir_vk_intercept_vkGetPhysicalDeviceFeatures2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceFormatProperties, elixir_vk_intercept_vkGetPhysicalDeviceFormatProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceFormatProperties2, elixir_vk_intercept_vkGetPhysicalDeviceFormatProperties2)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceFormatProperties2KHR, elixir_vk_intercept_vkGetPhysicalDeviceFormatProperties2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceFragmentShadingRatesKHR, elixir_vk_sysv_vkGetPhysicalDeviceFragmentShadingRatesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceImageFormatProperties, elixir_vk_intercept_vkGetPhysicalDeviceImageFormatProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceImageFormatProperties2, elixir_vk_intercept_vkGetPhysicalDeviceImageFormatProperties2)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceImageFormatProperties2KHR, elixir_vk_intercept_vkGetPhysicalDeviceImageFormatProperties2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceMemoryProperties, elixir_vk_sysv_vkGetPhysicalDeviceMemoryProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceMemoryProperties2, elixir_vk_sysv_vkGetPhysicalDeviceMemoryProperties2)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceMemoryProperties2KHR, elixir_vk_sysv_vkGetPhysicalDeviceMemoryProperties2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceMultisamplePropertiesEXT, elixir_vk_sysv_vkGetPhysicalDeviceMultisamplePropertiesEXT)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceOpticalFlowImageFormatsNV, elixir_vk_sysv_vkGetPhysicalDeviceOpticalFlowImageFormatsNV)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDevicePresentRectanglesKHR, elixir_vk_sysv_vkGetPhysicalDevicePresentRectanglesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceProperties, elixir_vk_intercept_vkGetPhysicalDeviceProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceProperties2, elixir_vk_intercept_vkGetPhysicalDeviceProperties2)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceProperties2KHR, elixir_vk_intercept_vkGetPhysicalDeviceProperties2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM, elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM, elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM, elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM, elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR, elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceQueueFamilyProperties, elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceQueueFamilyProperties2, elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyProperties2)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceQueueFamilyProperties2KHR, elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyProperties2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSparseImageFormatProperties, elixir_vk_sysv_vkGetPhysicalDeviceSparseImageFormatProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSparseImageFormatProperties2, elixir_vk_sysv_vkGetPhysicalDeviceSparseImageFormatProperties2)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSparseImageFormatProperties2KHR, elixir_vk_sysv_vkGetPhysicalDeviceSparseImageFormatProperties2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV, elixir_vk_sysv_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSurfaceCapabilities2EXT, elixir_vk_sysv_vkGetPhysicalDeviceSurfaceCapabilities2EXT)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSurfaceCapabilities2KHR, elixir_vk_sysv_vkGetPhysicalDeviceSurfaceCapabilities2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSurfaceFormats2KHR, elixir_vk_sysv_vkGetPhysicalDeviceSurfaceFormats2KHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSurfaceFormatsKHR, elixir_vk_sysv_vkGetPhysicalDeviceSurfaceFormatsKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSurfacePresentModesKHR, elixir_vk_intercept_vkGetPhysicalDeviceSurfacePresentModesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceSurfaceSupportKHR, elixir_vk_sysv_vkGetPhysicalDeviceSurfaceSupportKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceToolProperties, elixir_vk_sysv_vkGetPhysicalDeviceToolProperties)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceToolPropertiesEXT, elixir_vk_sysv_vkGetPhysicalDeviceToolPropertiesEXT)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceVideoCapabilitiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceVideoCapabilitiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPhysicalDeviceVideoFormatPropertiesKHR, elixir_vk_sysv_vkGetPhysicalDeviceVideoFormatPropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPipelineBinaryDataKHR, elixir_vk_sysv_vkGetPipelineBinaryDataKHR)
ELIXIR_VK_SIGNATURE(vkGetPipelineCacheData, elixir_vk_sysv_vkGetPipelineCacheData)
ELIXIR_VK_SIGNATURE(vkGetPipelineExecutableInternalRepresentationsKHR, elixir_vk_sysv_vkGetPipelineExecutableInternalRepresentationsKHR)
ELIXIR_VK_SIGNATURE(vkGetPipelineExecutablePropertiesKHR, elixir_vk_sysv_vkGetPipelineExecutablePropertiesKHR)
ELIXIR_VK_SIGNATURE(vkGetPipelineExecutableStatisticsKHR, elixir_vk_sysv_vkGetPipelineExecutableStatisticsKHR)
ELIXIR_VK_SIGNATURE(vkGetPipelineIndirectDeviceAddressNV, elixir_vk_sysv_vkGetPipelineIndirectDeviceAddressNV)
ELIXIR_VK_SIGNATURE(vkGetPipelineIndirectMemoryRequirementsNV, elixir_vk_sysv_vkGetPipelineIndirectMemoryRequirementsNV)
ELIXIR_VK_SIGNATURE(vkGetPipelineKeyKHR, elixir_vk_sysv_vkGetPipelineKeyKHR)
ELIXIR_VK_SIGNATURE(vkGetPipelinePropertiesEXT, elixir_vk_sysv_vkGetPipelinePropertiesEXT)
ELIXIR_VK_SIGNATURE(vkGetPrivateData, elixir_vk_sysv_vkGetPrivateData)
ELIXIR_VK_SIGNATURE(vkGetPrivateDataEXT, elixir_vk_sysv_vkGetPrivateDataEXT)
ELIXIR_VK_SIGNATURE(vkGetQueryPoolResults, elixir_vk_sysv_vkGetQueryPoolResults)
ELIXIR_VK_SIGNATURE(vkGetQueueCheckpointData2NV, elixir_vk_sysv_vkGetQueueCheckpointData2NV)
ELIXIR_VK_SIGNATURE(vkGetQueueCheckpointDataNV, elixir_vk_sysv_vkGetQueueCheckpointDataNV)
ELIXIR_VK_SIGNATURE(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, elixir_vk_sysv_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR)
ELIXIR_VK_SIGNATURE(vkGetRayTracingShaderGroupHandlesKHR, elixir_vk_sysv_vkGetRayTracingShaderGroupHandlesKHR)
ELIXIR_VK_SIGNATURE(vkGetRayTracingShaderGroupHandlesNV, elixir_vk_sysv_vkGetRayTracingShaderGroupHandlesNV)
ELIXIR_VK_SIGNATURE(vkGetRayTracingShaderGroupStackSizeKHR, elixir_vk_sysv_vkGetRayTracingShaderGroupStackSizeKHR)
ELIXIR_VK_SIGNATURE(vkGetRefreshCycleDurationGOOGLE, elixir_vk_sysv_vkGetRefreshCycleDurationGOOGLE)
ELIXIR_VK_SIGNATURE(vkGetRenderAreaGranularity, elixir_vk_sysv_vkGetRenderAreaGranularity)
ELIXIR_VK_SIGNATURE(vkGetRenderingAreaGranularity, elixir_vk_sysv_vkGetRenderingAreaGranularity)
ELIXIR_VK_SIGNATURE(vkGetRenderingAreaGranularityKHR, elixir_vk_sysv_vkGetRenderingAreaGranularityKHR)
ELIXIR_VK_SIGNATURE(vkGetSamplerOpaqueCaptureDescriptorDataEXT, elixir_vk_sysv_vkGetSamplerOpaqueCaptureDescriptorDataEXT)
ELIXIR_VK_SIGNATURE(vkGetSemaphoreCounterValue, elixir_vk_sysv_vkGetSemaphoreCounterValue)
ELIXIR_VK_SIGNATURE(vkGetSemaphoreCounterValueKHR, elixir_vk_sysv_vkGetSemaphoreCounterValueKHR)
ELIXIR_VK_SIGNATURE(vkGetSemaphoreFdKHR, elixir_vk_sysv_vkGetSemaphoreFdKHR)
ELIXIR_VK_SIGNATURE(vkGetShaderBinaryDataEXT, elixir_vk_sysv_vkGetShaderBinaryDataEXT)
ELIXIR_VK_SIGNATURE(vkGetShaderInfoAMD, elixir_vk_sysv_vkGetShaderInfoAMD)
ELIXIR_VK_SIGNATURE(vkGetShaderInstrumentationValuesARM, elixir_vk_sysv_vkGetShaderInstrumentationValuesARM)
ELIXIR_VK_SIGNATURE(vkGetShaderModuleCreateInfoIdentifierEXT, elixir_vk_sysv_vkGetShaderModuleCreateInfoIdentifierEXT)
ELIXIR_VK_SIGNATURE(vkGetShaderModuleIdentifierEXT, elixir_vk_sysv_vkGetShaderModuleIdentifierEXT)
ELIXIR_VK_SIGNATURE(vkGetSwapchainCounterEXT, elixir_vk_sysv_vkGetSwapchainCounterEXT)
ELIXIR_VK_SIGNATURE(vkGetSwapchainImagesKHR, elixir_vk_sysv_vkGetSwapchainImagesKHR)
ELIXIR_VK_SIGNATURE(vkGetSwapchainStatusKHR, elixir_vk_sysv_vkGetSwapchainStatusKHR)
ELIXIR_VK_SIGNATURE(vkGetSwapchainTimeDomainPropertiesEXT, elixir_vk_sysv_vkGetSwapchainTimeDomainPropertiesEXT)
ELIXIR_VK_SIGNATURE(vkGetSwapchainTimingPropertiesEXT, elixir_vk_sysv_vkGetSwapchainTimingPropertiesEXT)
ELIXIR_VK_SIGNATURE(vkGetTensorMemoryRequirementsARM, elixir_vk_sysv_vkGetTensorMemoryRequirementsARM)
ELIXIR_VK_SIGNATURE(vkGetTensorOpaqueCaptureDataARM, elixir_vk_sysv_vkGetTensorOpaqueCaptureDataARM)
ELIXIR_VK_SIGNATURE(vkGetTensorOpaqueCaptureDescriptorDataARM, elixir_vk_sysv_vkGetTensorOpaqueCaptureDescriptorDataARM)
ELIXIR_VK_SIGNATURE(vkGetTensorViewOpaqueCaptureDescriptorDataARM, elixir_vk_sysv_vkGetTensorViewOpaqueCaptureDescriptorDataARM)
ELIXIR_VK_SIGNATURE(vkGetValidationCacheDataEXT, elixir_vk_sysv_vkGetValidationCacheDataEXT)
ELIXIR_VK_SIGNATURE(vkGetVideoSessionMemoryRequirementsKHR, elixir_vk_sysv_vkGetVideoSessionMemoryRequirementsKHR)
ELIXIR_VK_SIGNATURE(vkImportFenceFdKHR, elixir_vk_sysv_vkImportFenceFdKHR)
ELIXIR_VK_SIGNATURE(vkImportSemaphoreFdKHR, elixir_vk_sysv_vkImportSemaphoreFdKHR)
ELIXIR_VK_SIGNATURE(vkInitializePerformanceApiINTEL, elixir_vk_sysv_vkInitializePerformanceApiINTEL)
ELIXIR_VK_SIGNATURE(vkInvalidateMappedMemoryRanges, elixir_vk_sysv_vkInvalidateMappedMemoryRanges)
ELIXIR_VK_SIGNATURE(vkLatencySleepNV, elixir_vk_sysv_vkLatencySleepNV)
ELIXIR_VK_SIGNATURE(vkMapMemory, elixir_vk_intercept_vkMapMemory)
ELIXIR_VK_SIGNATURE(vkMapMemory2, elixir_vk_intercept_vkMapMemory2)
ELIXIR_VK_SIGNATURE(vkMapMemory2KHR, elixir_vk_intercept_vkMapMemory2KHR)
ELIXIR_VK_SIGNATURE(vkMergePipelineCaches, elixir_vk_sysv_vkMergePipelineCaches)
ELIXIR_VK_SIGNATURE(vkMergeValidationCachesEXT, elixir_vk_sysv_vkMergeValidationCachesEXT)
ELIXIR_VK_SIGNATURE(vkQueueBeginDebugUtilsLabelEXT, elixir_vk_sysv_vkQueueBeginDebugUtilsLabelEXT)
ELIXIR_VK_SIGNATURE(vkQueueBindSparse, elixir_vk_sysv_vkQueueBindSparse)
ELIXIR_VK_SIGNATURE(vkQueueEndDebugUtilsLabelEXT, elixir_vk_sysv_vkQueueEndDebugUtilsLabelEXT)
ELIXIR_VK_SIGNATURE(vkQueueInsertDebugUtilsLabelEXT, elixir_vk_sysv_vkQueueInsertDebugUtilsLabelEXT)
ELIXIR_VK_SIGNATURE(vkQueueNotifyOutOfBandNV, elixir_vk_sysv_vkQueueNotifyOutOfBandNV)
ELIXIR_VK_SIGNATURE(vkQueuePresentKHR, elixir_vk_intercept_vkQueuePresentKHR)
ELIXIR_VK_SIGNATURE(vkQueueSetPerfHintQCOM, elixir_vk_sysv_vkQueueSetPerfHintQCOM)
ELIXIR_VK_SIGNATURE(vkQueueSetPerformanceConfigurationINTEL, elixir_vk_sysv_vkQueueSetPerformanceConfigurationINTEL)
ELIXIR_VK_SIGNATURE(vkQueueSubmit, elixir_vk_intercept_vkQueueSubmit)
ELIXIR_VK_SIGNATURE(vkQueueSubmit2, elixir_vk_intercept_vkQueueSubmit2)
ELIXIR_VK_SIGNATURE(vkQueueSubmit2KHR, elixir_vk_intercept_vkQueueSubmit2KHR)
ELIXIR_VK_SIGNATURE(vkQueueWaitIdle, elixir_vk_sysv_vkQueueWaitIdle)
ELIXIR_VK_SIGNATURE(vkRegisterCustomBorderColorEXT, elixir_vk_sysv_vkRegisterCustomBorderColorEXT)
ELIXIR_VK_SIGNATURE(vkRegisterDeviceEventEXT, elixir_vk_sysv_vkRegisterDeviceEventEXT)
ELIXIR_VK_SIGNATURE(vkRegisterDisplayEventEXT, elixir_vk_sysv_vkRegisterDisplayEventEXT)
ELIXIR_VK_SIGNATURE(vkReleaseCapturedPipelineDataKHR, elixir_vk_sysv_vkReleaseCapturedPipelineDataKHR)
ELIXIR_VK_SIGNATURE(vkReleaseDisplayEXT, elixir_vk_sysv_vkReleaseDisplayEXT)
ELIXIR_VK_SIGNATURE(vkReleasePerformanceConfigurationINTEL, elixir_vk_sysv_vkReleasePerformanceConfigurationINTEL)
ELIXIR_VK_SIGNATURE(vkReleaseProfilingLockKHR, elixir_vk_sysv_vkReleaseProfilingLockKHR)
ELIXIR_VK_SIGNATURE(vkReleaseSwapchainImagesEXT, elixir_vk_sysv_vkReleaseSwapchainImagesEXT)
ELIXIR_VK_SIGNATURE(vkReleaseSwapchainImagesKHR, elixir_vk_sysv_vkReleaseSwapchainImagesKHR)
ELIXIR_VK_SIGNATURE(vkResetCommandBuffer, elixir_vk_intercept_vkResetCommandBuffer)
ELIXIR_VK_SIGNATURE(vkResetCommandPool, elixir_vk_intercept_vkResetCommandPool)
ELIXIR_VK_SIGNATURE(vkResetDescriptorPool, elixir_vk_sysv_vkResetDescriptorPool)
ELIXIR_VK_SIGNATURE(vkResetEvent, elixir_vk_sysv_vkResetEvent)
ELIXIR_VK_SIGNATURE(vkResetFences, elixir_vk_sysv_vkResetFences)
ELIXIR_VK_SIGNATURE(vkResetGpaSessionAMD, elixir_vk_sysv_vkResetGpaSessionAMD)
ELIXIR_VK_SIGNATURE(vkResetQueryPool, elixir_vk_sysv_vkResetQueryPool)
ELIXIR_VK_SIGNATURE(vkResetQueryPoolEXT, elixir_vk_sysv_vkResetQueryPoolEXT)
ELIXIR_VK_SIGNATURE(vkSetDebugUtilsObjectNameEXT, elixir_vk_sysv_vkSetDebugUtilsObjectNameEXT)
ELIXIR_VK_SIGNATURE(vkSetDebugUtilsObjectTagEXT, elixir_vk_sysv_vkSetDebugUtilsObjectTagEXT)
ELIXIR_VK_SIGNATURE(vkSetDeviceMemoryPriorityEXT, elixir_vk_sysv_vkSetDeviceMemoryPriorityEXT)
ELIXIR_VK_SIGNATURE(vkSetEvent, elixir_vk_sysv_vkSetEvent)
ELIXIR_VK_SIGNATURE(vkSetGpaDeviceClockModeAMD, elixir_vk_sysv_vkSetGpaDeviceClockModeAMD)
ELIXIR_VK_SIGNATURE(vkSetHdrMetadataEXT, elixir_vk_sysv_vkSetHdrMetadataEXT)
ELIXIR_VK_SIGNATURE(vkSetLatencyMarkerNV, elixir_vk_sysv_vkSetLatencyMarkerNV)
ELIXIR_VK_SIGNATURE(vkSetLatencySleepModeNV, elixir_vk_sysv_vkSetLatencySleepModeNV)
ELIXIR_VK_SIGNATURE(vkSetLocalDimmingAMD, elixir_vk_sysv_vkSetLocalDimmingAMD)
ELIXIR_VK_SIGNATURE(vkSetPrivateData, elixir_vk_sysv_vkSetPrivateData)
ELIXIR_VK_SIGNATURE(vkSetPrivateDataEXT, elixir_vk_sysv_vkSetPrivateDataEXT)
ELIXIR_VK_SIGNATURE(vkSetSwapchainPresentTimingQueueSizeEXT, elixir_vk_sysv_vkSetSwapchainPresentTimingQueueSizeEXT)
ELIXIR_VK_SIGNATURE(vkSignalSemaphore, elixir_vk_sysv_vkSignalSemaphore)
ELIXIR_VK_SIGNATURE(vkSignalSemaphoreKHR, elixir_vk_sysv_vkSignalSemaphoreKHR)
ELIXIR_VK_SIGNATURE(vkSubmitDebugUtilsMessageEXT, elixir_vk_sysv_vkSubmitDebugUtilsMessageEXT)
ELIXIR_VK_SIGNATURE(vkTransitionImageLayout, elixir_vk_sysv_vkTransitionImageLayout)
ELIXIR_VK_SIGNATURE(vkTransitionImageLayoutEXT, elixir_vk_sysv_vkTransitionImageLayoutEXT)
ELIXIR_VK_SIGNATURE(vkTrimCommandPool, elixir_vk_sysv_vkTrimCommandPool)
ELIXIR_VK_SIGNATURE(vkTrimCommandPoolKHR, elixir_vk_sysv_vkTrimCommandPoolKHR)
ELIXIR_VK_SIGNATURE(vkUninitializePerformanceApiINTEL, elixir_vk_sysv_vkUninitializePerformanceApiINTEL)
ELIXIR_VK_SIGNATURE(vkUnmapMemory, elixir_vk_intercept_vkUnmapMemory)
ELIXIR_VK_SIGNATURE(vkUnmapMemory2, elixir_vk_intercept_vkUnmapMemory2)
ELIXIR_VK_SIGNATURE(vkUnmapMemory2KHR, elixir_vk_intercept_vkUnmapMemory2KHR)
ELIXIR_VK_SIGNATURE(vkUnregisterCustomBorderColorEXT, elixir_vk_sysv_vkUnregisterCustomBorderColorEXT)
ELIXIR_VK_SIGNATURE(vkUpdateDescriptorSetWithTemplate, elixir_vk_sysv_vkUpdateDescriptorSetWithTemplate)
ELIXIR_VK_SIGNATURE(vkUpdateDescriptorSetWithTemplateKHR, elixir_vk_sysv_vkUpdateDescriptorSetWithTemplateKHR)
ELIXIR_VK_SIGNATURE(vkUpdateDescriptorSets, elixir_vk_sysv_vkUpdateDescriptorSets)
ELIXIR_VK_SIGNATURE(vkUpdateIndirectExecutionSetPipelineEXT, elixir_vk_sysv_vkUpdateIndirectExecutionSetPipelineEXT)
ELIXIR_VK_SIGNATURE(vkUpdateIndirectExecutionSetShaderEXT, elixir_vk_sysv_vkUpdateIndirectExecutionSetShaderEXT)
ELIXIR_VK_SIGNATURE(vkUpdateVideoSessionParametersKHR, elixir_vk_sysv_vkUpdateVideoSessionParametersKHR)
ELIXIR_VK_SIGNATURE(vkWaitForFences, elixir_vk_sysv_vkWaitForFences)
ELIXIR_VK_SIGNATURE(vkWaitForPresent2KHR, elixir_vk_sysv_vkWaitForPresent2KHR)
ELIXIR_VK_SIGNATURE(vkWaitForPresentKHR, elixir_vk_sysv_vkWaitForPresentKHR)
ELIXIR_VK_SIGNATURE(vkWaitSemaphores, elixir_vk_sysv_vkWaitSemaphores)
ELIXIR_VK_SIGNATURE(vkWaitSemaphoresKHR, elixir_vk_sysv_vkWaitSemaphoresKHR)
ELIXIR_VK_SIGNATURE(vkWriteAccelerationStructuresPropertiesKHR, elixir_vk_sysv_vkWriteAccelerationStructuresPropertiesKHR)
ELIXIR_VK_SIGNATURE(vkWriteMicromapsPropertiesEXT, elixir_vk_sysv_vkWriteMicromapsPropertiesEXT)
ELIXIR_VK_SIGNATURE(vkWriteResourceDescriptorsEXT, elixir_vk_sysv_vkWriteResourceDescriptorsEXT)
ELIXIR_VK_SIGNATURE(vkWriteSamplerDescriptorsEXT, elixir_vk_sysv_vkWriteSamplerDescriptorsEXT)
#undef ELIXIR_VK_SIGNATURE

#define ELIXIR_VK_PROC(fn) reinterpret_cast<void*>(&fn)

const CommandInfo kCommands[kCommandCount] = {
    {"vkAcquireDrmDisplayEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkAcquireDrmDisplayEXT), 0, "VK_EXT_acquire_drm_display", CommandLevel::kInstance, 0},
    {"vkAcquireFullScreenExclusiveModeEXT", nullptr, 0, "VK_EXT_full_screen_exclusive", CommandLevel::kDevice, kCommandHostOnly},
    {"vkAcquireNextImage2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkAcquireNextImage2KHR), 0, "VK_KHR_swapchain,VK_KHR_device_group", CommandLevel::kDevice, kCommandIntercepted | kCommandLibvulkanExport},
    {"vkAcquireNextImageKHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkAcquireNextImageKHR), 0, "VK_KHR_swapchain", CommandLevel::kDevice, kCommandIntercepted | kCommandLibvulkanExport},
    {"vkAcquirePerformanceConfigurationINTEL", ELIXIR_VK_PROC(elixir_vk_sysv_vkAcquirePerformanceConfigurationINTEL), 0, "VK_INTEL_performance_query", CommandLevel::kDevice, 0},
    {"vkAcquireProfilingLockKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkAcquireProfilingLockKHR), 0, "VK_KHR_performance_query", CommandLevel::kDevice, 0},
    {"vkAcquireWinrtDisplayNV", nullptr, 0, "VK_NV_acquire_winrt_display", CommandLevel::kInstance, kCommandHostOnly},
    {"vkAllocateCommandBuffers", ELIXIR_VK_PROC(elixir_vk_intercept_vkAllocateCommandBuffers), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkAllocateDescriptorSets", ELIXIR_VK_PROC(elixir_vk_sysv_vkAllocateDescriptorSets), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkAllocateMemory", ELIXIR_VK_PROC(elixir_vk_sysv_vkAllocateMemory), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkAntiLagUpdateAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkAntiLagUpdateAMD), 0, "VK_AMD_anti_lag", CommandLevel::kDevice, 0},
    {"vkBeginCommandBuffer", ELIXIR_VK_PROC(elixir_vk_intercept_vkBeginCommandBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkBindAccelerationStructureMemoryNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkBindAccelerationStructureMemoryNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkBindBufferMemory", ELIXIR_VK_PROC(elixir_vk_intercept_vkBindBufferMemory), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkBindBufferMemory2", ELIXIR_VK_PROC(elixir_vk_intercept_vkBindBufferMemory2), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkBindBufferMemory2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkBindBufferMemory2KHR), 0, "VK_KHR_bind_memory2", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkBindDataGraphPipelineSessionMemoryARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkBindDataGraphPipelineSessionMemoryARM), 0, "VK_ARM_data_graph", CommandLevel::kDevice, 0},
    {"vkBindImageMemory", ELIXIR_VK_PROC(elixir_vk_sysv_vkBindImageMemory), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkBindImageMemory2", ELIXIR_VK_PROC(elixir_vk_sysv_vkBindImageMemory2), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkBindImageMemory2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkBindImageMemory2KHR), 0, "VK_KHR_bind_memory2", CommandLevel::kDevice, kCommandAlias},
    {"vkBindOpticalFlowSessionImageNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkBindOpticalFlowSessionImageNV), 0, "VK_NV_optical_flow", CommandLevel::kDevice, 0},
    {"vkBindTensorMemoryARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkBindTensorMemoryARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkBindVideoSessionMemoryKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkBindVideoSessionMemoryKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkBuildAccelerationStructuresKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkBuildAccelerationStructuresKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkBuildMicromapsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkBuildMicromapsEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkClearShaderInstrumentationMetricsARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkClearShaderInstrumentationMetricsARM), 0, "VK_ARM_shader_instrumentation", CommandLevel::kDevice, 0},
    {"vkCmdBeginConditionalRendering2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginConditionalRendering2EXT), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdBeginConditionalRenderingEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginConditionalRenderingEXT), 0, "VK_EXT_conditional_rendering", CommandLevel::kDevice, 0},
    {"vkCmdBeginCustomResolveEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginCustomResolveEXT), 0, "VK_EXT_custom_resolve", CommandLevel::kDevice, 0},
    {"vkCmdBeginDebugUtilsLabelEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginDebugUtilsLabelEXT), 0, "VK_EXT_debug_utils", CommandLevel::kDevice, 0},
    {"vkCmdBeginGpaSampleAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginGpaSampleAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkCmdBeginGpaSessionAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginGpaSessionAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkCmdBeginPerTileExecutionQCOM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginPerTileExecutionQCOM), 0, "VK_QCOM_tile_shading", CommandLevel::kDevice, 0},
    {"vkCmdBeginQuery", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginQuery), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBeginQueryIndexedEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginQueryIndexedEXT), 0, "VK_EXT_transform_feedback", CommandLevel::kDevice, 0},
    {"vkCmdBeginRenderPass", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginRenderPass), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBeginRenderPass2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginRenderPass2), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBeginRenderPass2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginRenderPass2KHR), 0, "VK_KHR_create_renderpass2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdBeginRendering", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginRendering), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBeginRenderingKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginRenderingKHR), 0, "VK_KHR_dynamic_rendering", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdBeginShaderInstrumentationARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginShaderInstrumentationARM), 0, "VK_ARM_shader_instrumentation", CommandLevel::kDevice, 0},
    {"vkCmdBeginTransformFeedback2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginTransformFeedback2EXT), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdBeginTransformFeedbackEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginTransformFeedbackEXT), 0, "VK_EXT_transform_feedback", CommandLevel::kDevice, 0},
    {"vkCmdBeginVideoCodingKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBeginVideoCodingKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkCmdBindDescriptorBufferEmbeddedSamplers2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT), 0, "VK_KHR_maintenance6", CommandLevel::kDevice, 0},
    {"vkCmdBindDescriptorBufferEmbeddedSamplersEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindDescriptorBufferEmbeddedSamplersEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkCmdBindDescriptorBuffersEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindDescriptorBuffersEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkCmdBindDescriptorSets", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindDescriptorSets), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBindDescriptorSets2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindDescriptorSets2), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBindDescriptorSets2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindDescriptorSets2KHR), 0, "VK_KHR_maintenance6", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdBindIndexBuffer", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindIndexBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBindIndexBuffer2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindIndexBuffer2), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBindIndexBuffer2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindIndexBuffer2KHR), 0, "VK_KHR_maintenance5", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdBindIndexBuffer3KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindIndexBuffer3KHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdBindInvocationMaskHUAWEI", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindInvocationMaskHUAWEI), 0, "VK_HUAWEI_invocation_mask", CommandLevel::kDevice, 0},
    {"vkCmdBindPipeline", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindPipeline), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBindPipelineShaderGroupNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindPipelineShaderGroupNV), 0, "VK_NV_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkCmdBindResourceHeapEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindResourceHeapEXT), 0, "VK_EXT_descriptor_heap", CommandLevel::kDevice, 0},
    {"vkCmdBindSamplerHeapEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindSamplerHeapEXT), 0, "VK_EXT_descriptor_heap", CommandLevel::kDevice, 0},
    {"vkCmdBindShadersEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindShadersEXT), 0, "VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdBindShadingRateImageNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindShadingRateImageNV), 0, "VK_NV_shading_rate_image", CommandLevel::kDevice, 0},
    {"vkCmdBindTileMemoryQCOM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindTileMemoryQCOM), 0, "VK_QCOM_tile_memory_heap", CommandLevel::kDevice, 0},
    {"vkCmdBindTransformFeedbackBuffers2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindTransformFeedbackBuffers2EXT), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdBindTransformFeedbackBuffersEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindTransformFeedbackBuffersEXT), 0, "VK_EXT_transform_feedback", CommandLevel::kDevice, 0},
    {"vkCmdBindVertexBuffers", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindVertexBuffers), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBindVertexBuffers2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindVertexBuffers2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBindVertexBuffers2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindVertexBuffers2EXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdBindVertexBuffers3KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBindVertexBuffers3KHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdBlitImage", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBlitImage), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBlitImage2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBlitImage2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdBlitImage2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBlitImage2KHR), 0, "VK_KHR_copy_commands2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdBuildAccelerationStructureNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBuildAccelerationStructureNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkCmdBuildAccelerationStructuresIndirectKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBuildAccelerationStructuresIndirectKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCmdBuildAccelerationStructuresKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBuildAccelerationStructuresKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCmdBuildClusterAccelerationStructureIndirectNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBuildClusterAccelerationStructureIndirectNV), 0, "VK_NV_cluster_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCmdBuildMicromapsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBuildMicromapsEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkCmdBuildPartitionedAccelerationStructuresNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdBuildPartitionedAccelerationStructuresNV), 0, "VK_NV_partitioned_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCmdClearAttachments", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdClearAttachments), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdClearColorImage", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdClearColorImage), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdClearDepthStencilImage", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdClearDepthStencilImage), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdControlVideoCodingKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdControlVideoCodingKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkCmdConvertCooperativeVectorMatrixNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdConvertCooperativeVectorMatrixNV), 0, "VK_NV_cooperative_vector", CommandLevel::kDevice, 0},
    {"vkCmdCopyAccelerationStructureKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyAccelerationStructureKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCmdCopyAccelerationStructureNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyAccelerationStructureNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkCmdCopyAccelerationStructureToMemoryKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyAccelerationStructureToMemoryKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCmdCopyBuffer", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdCopyBuffer2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyBuffer2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdCopyBuffer2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyBuffer2KHR), 0, "VK_KHR_copy_commands2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdCopyBufferToImage", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdCopyBufferToImage), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCmdCopyBufferToImage2", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdCopyBufferToImage2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCmdCopyBufferToImage2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdCopyBufferToImage2KHR), 0, "VK_KHR_copy_commands2", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkCmdCopyGpaSessionResultsAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyGpaSessionResultsAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkCmdCopyImage", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdCopyImage), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCmdCopyImage2", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdCopyImage2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCmdCopyImage2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdCopyImage2KHR), 0, "VK_KHR_copy_commands2", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkCmdCopyImageToBuffer", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdCopyImageToBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCmdCopyImageToBuffer2", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdCopyImageToBuffer2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCmdCopyImageToBuffer2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdCopyImageToBuffer2KHR), 0, "VK_KHR_copy_commands2", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkCmdCopyImageToMemoryKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyImageToMemoryKHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdCopyMemoryIndirectKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMemoryIndirectKHR), 0, "VK_KHR_copy_memory_indirect", CommandLevel::kDevice, 0},
    {"vkCmdCopyMemoryIndirectNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMemoryIndirectNV), 0, "VK_NV_copy_memory_indirect", CommandLevel::kDevice, 0},
    {"vkCmdCopyMemoryKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMemoryKHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdCopyMemoryToAccelerationStructureKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMemoryToAccelerationStructureKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCmdCopyMemoryToImageIndirectKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMemoryToImageIndirectKHR), 0, "VK_KHR_copy_memory_indirect", CommandLevel::kDevice, 0},
    {"vkCmdCopyMemoryToImageIndirectNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMemoryToImageIndirectNV), 0, "VK_NV_copy_memory_indirect", CommandLevel::kDevice, 0},
    {"vkCmdCopyMemoryToImageKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMemoryToImageKHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdCopyMemoryToMicromapEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMemoryToMicromapEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkCmdCopyMicromapEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMicromapEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkCmdCopyMicromapToMemoryEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyMicromapToMemoryEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkCmdCopyQueryPoolResults", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyQueryPoolResults), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdCopyQueryPoolResultsToMemoryKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyQueryPoolResultsToMemoryKHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdCopyTensorARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCopyTensorARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkCmdCuLaunchKernelNVX", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCuLaunchKernelNVX), 0, "VK_NVX_binary_import", CommandLevel::kDevice, 0},
    {"vkCmdCudaLaunchKernelNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdCudaLaunchKernelNV), 0, "VK_NV_cuda_kernel_launch", CommandLevel::kDevice, kCommandProvisional},
    {"vkCmdDebugMarkerBeginEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDebugMarkerBeginEXT), 0, "VK_EXT_debug_marker", CommandLevel::kDevice, 0},
    {"vkCmdDebugMarkerEndEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDebugMarkerEndEXT), 0, "VK_EXT_debug_marker", CommandLevel::kDevice, 0},
    {"vkCmdDebugMarkerInsertEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDebugMarkerInsertEXT), 0, "VK_EXT_debug_marker", CommandLevel::kDevice, 0},
    {"vkCmdDecodeVideoKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDecodeVideoKHR), 0, "VK_KHR_video_decode_queue", CommandLevel::kDevice, 0},
    {"vkCmdDecompressMemoryEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDecompressMemoryEXT), 0, "VK_EXT_memory_decompression", CommandLevel::kDevice, 0},
    {"vkCmdDecompressMemoryIndirectCountEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDecompressMemoryIndirectCountEXT), 0, "VK_EXT_memory_decompression", CommandLevel::kDevice, 0},
    {"vkCmdDecompressMemoryIndirectCountNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDecompressMemoryIndirectCountNV), 0, "VK_NV_memory_decompression", CommandLevel::kDevice, 0},
    {"vkCmdDecompressMemoryNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDecompressMemoryNV), 0, "VK_NV_memory_decompression", CommandLevel::kDevice, 0},
    {"vkCmdDispatch", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatch), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdDispatchBase", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatchBase), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdDispatchBaseKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatchBaseKHR), 0, "VK_KHR_device_group", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdDispatchDataGraphARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatchDataGraphARM), 0, "VK_ARM_data_graph", CommandLevel::kDevice, 0},
    {"vkCmdDispatchGraphAMDX", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatchGraphAMDX), 0, "VK_AMDX_shader_enqueue", CommandLevel::kDevice, kCommandProvisional},
    {"vkCmdDispatchGraphIndirectAMDX", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatchGraphIndirectAMDX), 0, "VK_AMDX_shader_enqueue", CommandLevel::kDevice, kCommandProvisional},
    {"vkCmdDispatchGraphIndirectCountAMDX", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatchGraphIndirectCountAMDX), 0, "VK_AMDX_shader_enqueue", CommandLevel::kDevice, kCommandProvisional},
    {"vkCmdDispatchIndirect", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatchIndirect), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdDispatchIndirect2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatchIndirect2KHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdDispatchTileQCOM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDispatchTileQCOM), 0, "VK_QCOM_tile_shading", CommandLevel::kDevice, 0},
    {"vkCmdDraw", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDraw), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdDrawClusterHUAWEI", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawClusterHUAWEI), 0, "VK_HUAWEI_cluster_culling_shader", CommandLevel::kDevice, 0},
    {"vkCmdDrawClusterIndirectHUAWEI", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawClusterIndirectHUAWEI), 0, "VK_HUAWEI_cluster_culling_shader", CommandLevel::kDevice, 0},
    {"vkCmdDrawIndexed", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndexed), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdDrawIndexedIndirect", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndexedIndirect), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdDrawIndexedIndirect2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndexedIndirect2KHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdDrawIndexedIndirectCount", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndexedIndirectCount), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdDrawIndexedIndirectCount2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndexedIndirectCount2KHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdDrawIndexedIndirectCountAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndexedIndirectCountAMD), 0, "VK_AMD_draw_indirect_count", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdDrawIndexedIndirectCountKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndexedIndirectCountKHR), 0, "VK_KHR_draw_indirect_count", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdDrawIndirect", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndirect), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdDrawIndirect2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndirect2KHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdDrawIndirectByteCount2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndirectByteCount2EXT), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdDrawIndirectByteCountEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndirectByteCountEXT), 0, "VK_EXT_transform_feedback", CommandLevel::kDevice, 0},
    {"vkCmdDrawIndirectCount", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndirectCount), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdDrawIndirectCount2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndirectCount2KHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdDrawIndirectCountAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndirectCountAMD), 0, "VK_AMD_draw_indirect_count", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdDrawIndirectCountKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawIndirectCountKHR), 0, "VK_KHR_draw_indirect_count", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdDrawMeshTasksEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMeshTasksEXT), 0, "VK_EXT_mesh_shader", CommandLevel::kDevice, 0},
    {"vkCmdDrawMeshTasksIndirect2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMeshTasksIndirect2EXT), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdDrawMeshTasksIndirectCount2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMeshTasksIndirectCount2EXT), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdDrawMeshTasksIndirectCountEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMeshTasksIndirectCountEXT), 0, "VK_EXT_mesh_shader", CommandLevel::kDevice, 0},
    {"vkCmdDrawMeshTasksIndirectCountNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMeshTasksIndirectCountNV), 0, "VK_NV_mesh_shader", CommandLevel::kDevice, 0},
    {"vkCmdDrawMeshTasksIndirectEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMeshTasksIndirectEXT), 0, "VK_EXT_mesh_shader", CommandLevel::kDevice, 0},
    {"vkCmdDrawMeshTasksIndirectNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMeshTasksIndirectNV), 0, "VK_NV_mesh_shader", CommandLevel::kDevice, 0},
    {"vkCmdDrawMeshTasksNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMeshTasksNV), 0, "VK_NV_mesh_shader", CommandLevel::kDevice, 0},
    {"vkCmdDrawMultiEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMultiEXT), 0, "VK_EXT_multi_draw", CommandLevel::kDevice, 0},
    {"vkCmdDrawMultiIndexedEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdDrawMultiIndexedEXT), 0, "VK_EXT_multi_draw", CommandLevel::kDevice, 0},
    {"vkCmdEncodeVideoKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEncodeVideoKHR), 0, "VK_KHR_video_encode_queue", CommandLevel::kDevice, 0},
    {"vkCmdEndConditionalRenderingEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndConditionalRenderingEXT), 0, "VK_EXT_conditional_rendering", CommandLevel::kDevice, 0},
    {"vkCmdEndDebugUtilsLabelEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndDebugUtilsLabelEXT), 0, "VK_EXT_debug_utils", CommandLevel::kDevice, 0},
    {"vkCmdEndGpaSampleAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndGpaSampleAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkCmdEndGpaSessionAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndGpaSessionAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkCmdEndPerTileExecutionQCOM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndPerTileExecutionQCOM), 0, "VK_QCOM_tile_shading", CommandLevel::kDevice, 0},
    {"vkCmdEndQuery", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndQuery), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdEndQueryIndexedEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndQueryIndexedEXT), 0, "VK_EXT_transform_feedback", CommandLevel::kDevice, 0},
    {"vkCmdEndRenderPass", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndRenderPass), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdEndRenderPass2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndRenderPass2), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdEndRenderPass2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndRenderPass2KHR), 0, "VK_KHR_create_renderpass2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdEndRendering", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndRendering), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdEndRendering2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndRendering2EXT), 0, "VK_EXT_fragment_density_map_offset", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdEndRendering2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndRendering2KHR), 0, "VK_KHR_maintenance10", CommandLevel::kDevice, 0},
    {"vkCmdEndRenderingKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndRenderingKHR), 0, "VK_KHR_dynamic_rendering", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdEndShaderInstrumentationARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndShaderInstrumentationARM), 0, "VK_ARM_shader_instrumentation", CommandLevel::kDevice, 0},
    {"vkCmdEndTransformFeedback2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndTransformFeedback2EXT), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdEndTransformFeedbackEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndTransformFeedbackEXT), 0, "VK_EXT_transform_feedback", CommandLevel::kDevice, 0},
    {"vkCmdEndVideoCodingKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdEndVideoCodingKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkCmdExecuteCommands", ELIXIR_VK_PROC(elixir_vk_intercept_vkCmdExecuteCommands), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCmdExecuteGeneratedCommandsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdExecuteGeneratedCommandsEXT), 0, "VK_EXT_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkCmdExecuteGeneratedCommandsNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdExecuteGeneratedCommandsNV), 0, "VK_NV_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkCmdFillBuffer", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdFillBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdFillMemoryKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdFillMemoryKHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdInitializeGraphScratchMemoryAMDX", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdInitializeGraphScratchMemoryAMDX), 0, "VK_AMDX_shader_enqueue", CommandLevel::kDevice, kCommandProvisional},
    {"vkCmdInsertDebugUtilsLabelEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdInsertDebugUtilsLabelEXT), 0, "VK_EXT_debug_utils", CommandLevel::kDevice, 0},
    {"vkCmdNextSubpass", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdNextSubpass), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdNextSubpass2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdNextSubpass2), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdNextSubpass2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdNextSubpass2KHR), 0, "VK_KHR_create_renderpass2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdOpticalFlowExecuteNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdOpticalFlowExecuteNV), 0, "VK_NV_optical_flow", CommandLevel::kDevice, 0},
    {"vkCmdPipelineBarrier", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPipelineBarrier), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdPipelineBarrier2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPipelineBarrier2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdPipelineBarrier2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPipelineBarrier2KHR), 0, "VK_KHR_synchronization2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdPreprocessGeneratedCommandsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPreprocessGeneratedCommandsEXT), 0, "VK_EXT_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkCmdPreprocessGeneratedCommandsNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPreprocessGeneratedCommandsNV), 0, "VK_NV_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkCmdPushConstants", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushConstants), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdPushConstants2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushConstants2), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdPushConstants2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushConstants2KHR), 0, "VK_KHR_maintenance6", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdPushDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushDataEXT), 0, "VK_EXT_descriptor_heap", CommandLevel::kDevice, 0},
    {"vkCmdPushDescriptorSet", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushDescriptorSet), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdPushDescriptorSet2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushDescriptorSet2), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdPushDescriptorSet2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushDescriptorSet2KHR), 0, "VK_KHR_maintenance6", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdPushDescriptorSetKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushDescriptorSetKHR), 0, "VK_KHR_push_descriptor", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdPushDescriptorSetWithTemplate", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplate), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdPushDescriptorSetWithTemplate2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplate2), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdPushDescriptorSetWithTemplate2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplate2KHR), 0, "VK_KHR_maintenance6", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdPushDescriptorSetWithTemplateKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdPushDescriptorSetWithTemplateKHR), 0, "VK_KHR_push_descriptor,VK_KHR_descriptor_update_template", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdResetEvent", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdResetEvent), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdResetEvent2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdResetEvent2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdResetEvent2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdResetEvent2KHR), 0, "VK_KHR_synchronization2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdResetQueryPool", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdResetQueryPool), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdResolveImage", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdResolveImage), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdResolveImage2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdResolveImage2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdResolveImage2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdResolveImage2KHR), 0, "VK_KHR_copy_commands2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetAlphaToCoverageEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetAlphaToCoverageEnableEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetAlphaToOneEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetAlphaToOneEnableEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetAttachmentFeedbackLoopEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetAttachmentFeedbackLoopEnableEXT), 0, "VK_EXT_attachment_feedback_loop_dynamic_state", CommandLevel::kDevice, 0},
    {"vkCmdSetBlendConstants", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetBlendConstants), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetCheckpointNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCheckpointNV), 0, "VK_NV_device_diagnostic_checkpoints", CommandLevel::kDevice, 0},
    {"vkCmdSetCoarseSampleOrderNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCoarseSampleOrderNV), 0, "VK_NV_shading_rate_image", CommandLevel::kDevice, 0},
    {"vkCmdSetColorBlendAdvancedEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetColorBlendAdvancedEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetColorBlendEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetColorBlendEnableEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetColorBlendEquationEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetColorBlendEquationEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetColorWriteEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetColorWriteEnableEXT), 0, "VK_EXT_color_write_enable", CommandLevel::kDevice, 0},
    {"vkCmdSetColorWriteMaskEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetColorWriteMaskEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetComputeOccupancyPriorityNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetComputeOccupancyPriorityNV), 0, "VK_NV_compute_occupancy_priority", CommandLevel::kDevice, 0},
    {"vkCmdSetConservativeRasterizationModeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetConservativeRasterizationModeEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetCoverageModulationModeNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCoverageModulationModeNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetCoverageModulationTableEnableNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCoverageModulationTableEnableNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetCoverageModulationTableNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCoverageModulationTableNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetCoverageReductionModeNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCoverageReductionModeNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetCoverageToColorEnableNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCoverageToColorEnableNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetCoverageToColorLocationNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCoverageToColorLocationNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetCullMode", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCullMode), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetCullModeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetCullModeEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetDepthBias", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthBias), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetDepthBias2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthBias2EXT), 0, "VK_EXT_depth_bias_control", CommandLevel::kDevice, 0},
    {"vkCmdSetDepthBiasEnable", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthBiasEnable), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetDepthBiasEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthBiasEnableEXT), 0, "VK_EXT_extended_dynamic_state2,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetDepthBounds", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthBounds), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetDepthBoundsTestEnable", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthBoundsTestEnable), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetDepthBoundsTestEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthBoundsTestEnableEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetDepthClampEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthClampEnableEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetDepthClampRangeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthClampRangeEXT), 0, "VK_EXT_shader_object,VK_EXT_depth_clamp_control", CommandLevel::kDevice, 0},
    {"vkCmdSetDepthClipEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthClipEnableEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetDepthClipNegativeOneToOneEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthClipNegativeOneToOneEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetDepthCompareOp", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthCompareOp), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetDepthCompareOpEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthCompareOpEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetDepthTestEnable", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthTestEnable), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetDepthTestEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthTestEnableEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetDepthWriteEnable", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthWriteEnable), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetDepthWriteEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDepthWriteEnableEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetDescriptorBufferOffsets2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDescriptorBufferOffsets2EXT), 0, "VK_KHR_maintenance6", CommandLevel::kDevice, 0},
    {"vkCmdSetDescriptorBufferOffsetsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDescriptorBufferOffsetsEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkCmdSetDeviceMask", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDeviceMask), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetDeviceMaskKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDeviceMaskKHR), 0, "VK_KHR_device_group", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetDiscardRectangleEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDiscardRectangleEXT), 0, "VK_EXT_discard_rectangles", CommandLevel::kDevice, 0},
    {"vkCmdSetDiscardRectangleEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDiscardRectangleEnableEXT), 0, "VK_EXT_discard_rectangles", CommandLevel::kDevice, 0},
    {"vkCmdSetDiscardRectangleModeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDiscardRectangleModeEXT), 0, "VK_EXT_discard_rectangles", CommandLevel::kDevice, 0},
    {"vkCmdSetDispatchParametersARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetDispatchParametersARM), 0, "VK_ARM_scheduling_controls", CommandLevel::kDevice, 0},
    {"vkCmdSetEvent", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetEvent), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetEvent2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetEvent2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetEvent2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetEvent2KHR), 0, "VK_KHR_synchronization2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetExclusiveScissorEnableNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetExclusiveScissorEnableNV), 0, "VK_NV_scissor_exclusive", CommandLevel::kDevice, 0},
    {"vkCmdSetExclusiveScissorNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetExclusiveScissorNV), 0, "VK_NV_scissor_exclusive", CommandLevel::kDevice, 0},
    {"vkCmdSetExtraPrimitiveOverestimationSizeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetExtraPrimitiveOverestimationSizeEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetFragmentShadingRateEnumNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetFragmentShadingRateEnumNV), 0, "VK_NV_fragment_shading_rate_enums", CommandLevel::kDevice, 0},
    {"vkCmdSetFragmentShadingRateKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetFragmentShadingRateKHR), 0, "VK_KHR_fragment_shading_rate", CommandLevel::kDevice, 0},
    {"vkCmdSetFrontFace", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetFrontFace), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetFrontFaceEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetFrontFaceEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetLineRasterizationModeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetLineRasterizationModeEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetLineStipple", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetLineStipple), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetLineStippleEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetLineStippleEXT), 0, "VK_EXT_line_rasterization", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetLineStippleEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetLineStippleEnableEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetLineStippleKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetLineStippleKHR), 0, "VK_KHR_line_rasterization", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetLineWidth", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetLineWidth), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetLogicOpEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetLogicOpEXT), 0, "VK_EXT_extended_dynamic_state2,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetLogicOpEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetLogicOpEnableEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetPatchControlPointsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPatchControlPointsEXT), 0, "VK_EXT_extended_dynamic_state2,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetPerformanceMarkerINTEL", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPerformanceMarkerINTEL), 0, "VK_INTEL_performance_query", CommandLevel::kDevice, 0},
    {"vkCmdSetPerformanceOverrideINTEL", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPerformanceOverrideINTEL), 0, "VK_INTEL_performance_query", CommandLevel::kDevice, 0},
    {"vkCmdSetPerformanceStreamMarkerINTEL", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPerformanceStreamMarkerINTEL), 0, "VK_INTEL_performance_query", CommandLevel::kDevice, 0},
    {"vkCmdSetPolygonModeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPolygonModeEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetPrimitiveRestartEnable", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPrimitiveRestartEnable), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetPrimitiveRestartEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPrimitiveRestartEnableEXT), 0, "VK_EXT_extended_dynamic_state2,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetPrimitiveRestartIndexEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPrimitiveRestartIndexEXT), 0, "VK_EXT_primitive_restart_index", CommandLevel::kDevice, 0},
    {"vkCmdSetPrimitiveTopology", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPrimitiveTopology), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetPrimitiveTopologyEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetPrimitiveTopologyEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetProvokingVertexModeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetProvokingVertexModeEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetRasterizationSamplesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRasterizationSamplesEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetRasterizationStreamEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRasterizationStreamEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetRasterizerDiscardEnable", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRasterizerDiscardEnable), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetRasterizerDiscardEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRasterizerDiscardEnableEXT), 0, "VK_EXT_extended_dynamic_state2,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetRayTracingPipelineStackSizeKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRayTracingPipelineStackSizeKHR), 0, "VK_KHR_ray_tracing_pipeline", CommandLevel::kDevice, 0},
    {"vkCmdSetRenderingAttachmentLocations", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRenderingAttachmentLocations), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetRenderingAttachmentLocationsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRenderingAttachmentLocationsKHR), 0, "VK_KHR_dynamic_rendering_local_read", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetRenderingInputAttachmentIndices", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRenderingInputAttachmentIndices), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetRenderingInputAttachmentIndicesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRenderingInputAttachmentIndicesKHR), 0, "VK_KHR_dynamic_rendering_local_read", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetRepresentativeFragmentTestEnableNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetRepresentativeFragmentTestEnableNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetSampleLocationsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetSampleLocationsEXT), 0, "VK_EXT_sample_locations", CommandLevel::kDevice, 0},
    {"vkCmdSetSampleLocationsEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetSampleLocationsEnableEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetSampleMaskEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetSampleMaskEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetScissor", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetScissor), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetScissorWithCount", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetScissorWithCount), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetScissorWithCountEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetScissorWithCountEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetShadingRateImageEnableNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetShadingRateImageEnableNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetStencilCompareMask", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetStencilCompareMask), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetStencilOp", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetStencilOp), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetStencilOpEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetStencilOpEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetStencilReference", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetStencilReference), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetStencilTestEnable", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetStencilTestEnable), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetStencilTestEnableEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetStencilTestEnableEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSetStencilWriteMask", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetStencilWriteMask), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetTessellationDomainOriginEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetTessellationDomainOriginEXT), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetVertexInputEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetVertexInputEXT), 0, "VK_EXT_vertex_input_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetViewport", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetViewport), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetViewportShadingRatePaletteNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetViewportShadingRatePaletteNV), 0, "VK_NV_shading_rate_image", CommandLevel::kDevice, 0},
    {"vkCmdSetViewportSwizzleNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetViewportSwizzleNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetViewportWScalingEnableNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetViewportWScalingEnableNV), 0, "VK_EXT_extended_dynamic_state3,VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCmdSetViewportWScalingNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetViewportWScalingNV), 0, "VK_NV_clip_space_w_scaling", CommandLevel::kDevice, 0},
    {"vkCmdSetViewportWithCount", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetViewportWithCount), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdSetViewportWithCountEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSetViewportWithCountEXT), 0, "VK_EXT_extended_dynamic_state,VK_EXT_shader_object", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdSubpassShadingHUAWEI", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdSubpassShadingHUAWEI), 0, "VK_HUAWEI_subpass_shading", CommandLevel::kDevice, 0},
    {"vkCmdTraceRaysIndirect2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdTraceRaysIndirect2KHR), 0, "VK_KHR_ray_tracing_maintenance1", CommandLevel::kDevice, 0},
    {"vkCmdTraceRaysIndirectKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdTraceRaysIndirectKHR), 0, "VK_KHR_ray_tracing_pipeline", CommandLevel::kDevice, 0},
    {"vkCmdTraceRaysKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdTraceRaysKHR), 0, "VK_KHR_ray_tracing_pipeline", CommandLevel::kDevice, 0},
    {"vkCmdTraceRaysNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdTraceRaysNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkCmdUpdateBuffer", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdUpdateBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdUpdateMemoryKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdUpdateMemoryKHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdUpdatePipelineIndirectBufferNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdUpdatePipelineIndirectBufferNV), 0, "VK_NV_device_generated_commands_compute", CommandLevel::kDevice, 0},
    {"vkCmdWaitEvents", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWaitEvents), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdWaitEvents2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWaitEvents2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdWaitEvents2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWaitEvents2KHR), 0, "VK_KHR_synchronization2", CommandLevel::kDevice, kCommandAlias},
    {"vkCmdWriteAccelerationStructuresPropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWriteAccelerationStructuresPropertiesKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCmdWriteAccelerationStructuresPropertiesNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWriteAccelerationStructuresPropertiesNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkCmdWriteBufferMarker2AMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWriteBufferMarker2AMD), 0, "VK_AMD_buffer_marker", CommandLevel::kDevice, 0},
    {"vkCmdWriteBufferMarkerAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWriteBufferMarkerAMD), 0, "VK_AMD_buffer_marker", CommandLevel::kDevice, 0},
    {"vkCmdWriteMarkerToMemoryAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWriteMarkerToMemoryAMD), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCmdWriteMicromapsPropertiesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWriteMicromapsPropertiesEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkCmdWriteTimestamp", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWriteTimestamp), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdWriteTimestamp2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWriteTimestamp2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCmdWriteTimestamp2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCmdWriteTimestamp2KHR), 0, "VK_KHR_synchronization2", CommandLevel::kDevice, kCommandAlias},
    {"vkCompileDeferredNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCompileDeferredNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkConvertCooperativeVectorMatrixNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkConvertCooperativeVectorMatrixNV), 0, "VK_NV_cooperative_vector", CommandLevel::kDevice, 0},
    {"vkCopyAccelerationStructureKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyAccelerationStructureKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCopyAccelerationStructureToMemoryKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyAccelerationStructureToMemoryKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCopyImageToImage", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyImageToImage), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore},
    {"vkCopyImageToImageEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyImageToImageEXT), 0, "VK_EXT_host_image_copy", CommandLevel::kDevice, kCommandAlias},
    {"vkCopyImageToMemory", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyImageToMemory), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCopyImageToMemoryEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyImageToMemoryEXT), 0, "VK_EXT_host_image_copy", CommandLevel::kDevice, kCommandAlias},
    {"vkCopyMemoryToAccelerationStructureKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyMemoryToAccelerationStructureKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCopyMemoryToImage", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyMemoryToImage), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCopyMemoryToImageEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyMemoryToImageEXT), 0, "VK_EXT_host_image_copy", CommandLevel::kDevice, kCommandAlias},
    {"vkCopyMemoryToMicromapEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyMemoryToMicromapEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkCopyMicromapEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyMicromapEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkCopyMicromapToMemoryEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCopyMicromapToMemoryEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkCreateAccelerationStructure2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateAccelerationStructure2KHR), 0, "VK_KHR_device_address_commands", CommandLevel::kDevice, 0},
    {"vkCreateAccelerationStructureKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateAccelerationStructureKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkCreateAccelerationStructureNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateAccelerationStructureNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkCreateAndroidSurfaceKHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkCreateAndroidSurfaceKHR), 0, "VK_KHR_android_surface", CommandLevel::kInstance, kCommandIntercepted | kCommandAndroid | kCommandLibvulkanExport},
    {"vkCreateBuffer", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateBufferView", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateBufferView), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateCommandPool", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateCommandPool), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateComputePipelines", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateComputePipelines), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateCuFunctionNVX", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateCuFunctionNVX), 0, "VK_NVX_binary_import", CommandLevel::kDevice, 0},
    {"vkCreateCuModuleNVX", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateCuModuleNVX), 0, "VK_NVX_binary_import", CommandLevel::kDevice, 0},
    {"vkCreateCudaFunctionNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateCudaFunctionNV), 0, "VK_NV_cuda_kernel_launch", CommandLevel::kDevice, kCommandProvisional},
    {"vkCreateCudaModuleNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateCudaModuleNV), 0, "VK_NV_cuda_kernel_launch", CommandLevel::kDevice, kCommandProvisional},
    {"vkCreateDataGraphPipelineSessionARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateDataGraphPipelineSessionARM), 0, "VK_ARM_data_graph", CommandLevel::kDevice, 0},
    {"vkCreateDataGraphPipelinesARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateDataGraphPipelinesARM), 0, "VK_ARM_data_graph", CommandLevel::kDevice, 0},
    {"vkCreateDebugReportCallbackEXT", ELIXIR_VK_PROC(elixir_vk_intercept_vkCreateDebugReportCallbackEXT), 0, "VK_EXT_debug_report", CommandLevel::kInstance, kCommandIntercepted},
    {"vkCreateDebugUtilsMessengerEXT", ELIXIR_VK_PROC(elixir_vk_intercept_vkCreateDebugUtilsMessengerEXT), 0, "VK_EXT_debug_utils", CommandLevel::kInstance, kCommandIntercepted},
    {"vkCreateDeferredOperationKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateDeferredOperationKHR), 0, "VK_KHR_deferred_host_operations", CommandLevel::kDevice, 0},
    {"vkCreateDescriptorPool", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateDescriptorPool), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateDescriptorSetLayout", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateDescriptorSetLayout), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateDescriptorUpdateTemplate", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateDescriptorUpdateTemplate), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateDescriptorUpdateTemplateKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateDescriptorUpdateTemplateKHR), 0, "VK_KHR_descriptor_update_template", CommandLevel::kDevice, kCommandAlias},
    {"vkCreateDevice", ELIXIR_VK_PROC(elixir_vk_intercept_vkCreateDevice), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCreateDisplayModeKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateDisplayModeKHR), 0, "VK_KHR_display", CommandLevel::kInstance, 0},
    {"vkCreateDisplayPlaneSurfaceKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateDisplayPlaneSurfaceKHR), 0, "VK_KHR_display", CommandLevel::kInstance, 0},
    {"vkCreateEvent", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateEvent), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateExecutionGraphPipelinesAMDX", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateExecutionGraphPipelinesAMDX), 0, "VK_AMDX_shader_enqueue", CommandLevel::kDevice, kCommandProvisional},
    {"vkCreateExternalComputeQueueNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateExternalComputeQueueNV), 0, "VK_NV_external_compute_queue", CommandLevel::kDevice, 0},
    {"vkCreateFence", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateFence), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateFramebuffer", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateFramebuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateGpaSessionAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateGpaSessionAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkCreateGraphicsPipelines", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateGraphicsPipelines), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateHeadlessSurfaceEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateHeadlessSurfaceEXT), 0, "VK_EXT_headless_surface", CommandLevel::kInstance, 0},
    {"vkCreateImage", ELIXIR_VK_PROC(elixir_vk_intercept_vkCreateImage), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCreateImageView", ELIXIR_VK_PROC(elixir_vk_intercept_vkCreateImageView), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCreateIndirectCommandsLayoutEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateIndirectCommandsLayoutEXT), 0, "VK_EXT_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkCreateIndirectCommandsLayoutNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateIndirectCommandsLayoutNV), 0, "VK_NV_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkCreateIndirectExecutionSetEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateIndirectExecutionSetEXT), 0, "VK_EXT_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkCreateInstance", ELIXIR_VK_PROC(elixir_vk_intercept_vkCreateInstance), VK_API_VERSION_1_0, nullptr, CommandLevel::kGlobal, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCreateMicromapEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateMicromapEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkCreateOpticalFlowSessionNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateOpticalFlowSessionNV), 0, "VK_NV_optical_flow", CommandLevel::kDevice, 0},
    {"vkCreatePipelineBinariesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreatePipelineBinariesKHR), 0, "VK_KHR_pipeline_binary", CommandLevel::kDevice, 0},
    {"vkCreatePipelineCache", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreatePipelineCache), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreatePipelineLayout", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreatePipelineLayout), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreatePrivateDataSlot", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreatePrivateDataSlot), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreatePrivateDataSlotEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreatePrivateDataSlotEXT), 0, "VK_EXT_private_data", CommandLevel::kDevice, kCommandAlias},
    {"vkCreateQueryPool", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateQueryPool), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateRayTracingPipelinesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateRayTracingPipelinesKHR), 0, "VK_KHR_ray_tracing_pipeline", CommandLevel::kDevice, 0},
    {"vkCreateRayTracingPipelinesNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateRayTracingPipelinesNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkCreateRenderPass", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateRenderPass), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateRenderPass2", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateRenderPass2), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateRenderPass2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateRenderPass2KHR), 0, "VK_KHR_create_renderpass2", CommandLevel::kDevice, kCommandAlias},
    {"vkCreateSampler", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateSampler), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateSamplerYcbcrConversion", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateSamplerYcbcrConversion), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateSamplerYcbcrConversionKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateSamplerYcbcrConversionKHR), 0, "VK_KHR_sampler_ycbcr_conversion", CommandLevel::kDevice, kCommandAlias},
    {"vkCreateSemaphore", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateSemaphore), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateShaderInstrumentationARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateShaderInstrumentationARM), 0, "VK_ARM_shader_instrumentation", CommandLevel::kDevice, 0},
    {"vkCreateShaderModule", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateShaderModule), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkCreateShadersEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateShadersEXT), 0, "VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkCreateSharedSwapchainsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateSharedSwapchainsKHR), 0, "VK_KHR_display_swapchain", CommandLevel::kDevice, 0},
    {"vkCreateSwapchainKHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkCreateSwapchainKHR), 0, "VK_KHR_swapchain", CommandLevel::kDevice, kCommandIntercepted | kCommandLibvulkanExport},
    {"vkCreateTensorARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateTensorARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkCreateTensorViewARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateTensorViewARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkCreateValidationCacheEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateValidationCacheEXT), 0, "VK_EXT_validation_cache", CommandLevel::kDevice, 0},
    {"vkCreateVideoSessionKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateVideoSessionKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkCreateVideoSessionParametersKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkCreateVideoSessionParametersKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkCreateWin32SurfaceKHR", nullptr, 0, "VK_KHR_win32_surface", CommandLevel::kInstance, kCommandHostOnly},
    {"vkDebugMarkerSetObjectNameEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDebugMarkerSetObjectNameEXT), 0, "VK_EXT_debug_marker", CommandLevel::kDevice, 0},
    {"vkDebugMarkerSetObjectTagEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDebugMarkerSetObjectTagEXT), 0, "VK_EXT_debug_marker", CommandLevel::kDevice, 0},
    {"vkDebugReportMessageEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDebugReportMessageEXT), 0, "VK_EXT_debug_report", CommandLevel::kInstance, 0},
    {"vkDeferredOperationJoinKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDeferredOperationJoinKHR), 0, "VK_KHR_deferred_host_operations", CommandLevel::kDevice, 0},
    {"vkDestroyAccelerationStructureKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyAccelerationStructureKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkDestroyAccelerationStructureNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyAccelerationStructureNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkDestroyBuffer", ELIXIR_VK_PROC(elixir_vk_intercept_vkDestroyBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkDestroyBufferView", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyBufferView), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyCommandPool", ELIXIR_VK_PROC(elixir_vk_intercept_vkDestroyCommandPool), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkDestroyCuFunctionNVX", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyCuFunctionNVX), 0, "VK_NVX_binary_import", CommandLevel::kDevice, 0},
    {"vkDestroyCuModuleNVX", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyCuModuleNVX), 0, "VK_NVX_binary_import", CommandLevel::kDevice, 0},
    {"vkDestroyCudaFunctionNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyCudaFunctionNV), 0, "VK_NV_cuda_kernel_launch", CommandLevel::kDevice, kCommandProvisional},
    {"vkDestroyCudaModuleNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyCudaModuleNV), 0, "VK_NV_cuda_kernel_launch", CommandLevel::kDevice, kCommandProvisional},
    {"vkDestroyDataGraphPipelineSessionARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyDataGraphPipelineSessionARM), 0, "VK_ARM_data_graph", CommandLevel::kDevice, 0},
    {"vkDestroyDebugReportCallbackEXT", ELIXIR_VK_PROC(elixir_vk_intercept_vkDestroyDebugReportCallbackEXT), 0, "VK_EXT_debug_report", CommandLevel::kInstance, kCommandIntercepted},
    {"vkDestroyDebugUtilsMessengerEXT", ELIXIR_VK_PROC(elixir_vk_intercept_vkDestroyDebugUtilsMessengerEXT), 0, "VK_EXT_debug_utils", CommandLevel::kInstance, kCommandIntercepted},
    {"vkDestroyDeferredOperationKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyDeferredOperationKHR), 0, "VK_KHR_deferred_host_operations", CommandLevel::kDevice, 0},
    {"vkDestroyDescriptorPool", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyDescriptorPool), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyDescriptorSetLayout", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyDescriptorSetLayout), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyDescriptorUpdateTemplate", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyDescriptorUpdateTemplate), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyDescriptorUpdateTemplateKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyDescriptorUpdateTemplateKHR), 0, "VK_KHR_descriptor_update_template", CommandLevel::kDevice, kCommandAlias},
    {"vkDestroyDevice", ELIXIR_VK_PROC(elixir_vk_intercept_vkDestroyDevice), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkDestroyEvent", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyEvent), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyExternalComputeQueueNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyExternalComputeQueueNV), 0, "VK_NV_external_compute_queue", CommandLevel::kDevice, 0},
    {"vkDestroyFence", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyFence), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyFramebuffer", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyFramebuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyGpaSessionAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyGpaSessionAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkDestroyImage", ELIXIR_VK_PROC(elixir_vk_intercept_vkDestroyImage), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkDestroyImageView", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyImageView), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyIndirectCommandsLayoutEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyIndirectCommandsLayoutEXT), 0, "VK_EXT_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkDestroyIndirectCommandsLayoutNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyIndirectCommandsLayoutNV), 0, "VK_NV_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkDestroyIndirectExecutionSetEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyIndirectExecutionSetEXT), 0, "VK_EXT_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkDestroyInstance", ELIXIR_VK_PROC(elixir_vk_intercept_vkDestroyInstance), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkDestroyMicromapEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyMicromapEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkDestroyOpticalFlowSessionNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyOpticalFlowSessionNV), 0, "VK_NV_optical_flow", CommandLevel::kDevice, 0},
    {"vkDestroyPipeline", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyPipeline), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyPipelineBinaryKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyPipelineBinaryKHR), 0, "VK_KHR_pipeline_binary", CommandLevel::kDevice, 0},
    {"vkDestroyPipelineCache", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyPipelineCache), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyPipelineLayout", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyPipelineLayout), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyPrivateDataSlot", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyPrivateDataSlot), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyPrivateDataSlotEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyPrivateDataSlotEXT), 0, "VK_EXT_private_data", CommandLevel::kDevice, kCommandAlias},
    {"vkDestroyQueryPool", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyQueryPool), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyRenderPass", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyRenderPass), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroySampler", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroySampler), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroySamplerYcbcrConversion", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroySamplerYcbcrConversion), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroySamplerYcbcrConversionKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroySamplerYcbcrConversionKHR), 0, "VK_KHR_sampler_ycbcr_conversion", CommandLevel::kDevice, kCommandAlias},
    {"vkDestroySemaphore", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroySemaphore), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroyShaderEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyShaderEXT), 0, "VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkDestroyShaderInstrumentationARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyShaderInstrumentationARM), 0, "VK_ARM_shader_instrumentation", CommandLevel::kDevice, 0},
    {"vkDestroyShaderModule", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyShaderModule), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDestroySurfaceKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroySurfaceKHR), 0, "VK_KHR_surface", CommandLevel::kInstance, kCommandLibvulkanExport},
    {"vkDestroySwapchainKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroySwapchainKHR), 0, "VK_KHR_swapchain", CommandLevel::kDevice, kCommandLibvulkanExport},
    {"vkDestroyTensorARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyTensorARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkDestroyTensorViewARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyTensorViewARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkDestroyValidationCacheEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyValidationCacheEXT), 0, "VK_EXT_validation_cache", CommandLevel::kDevice, 0},
    {"vkDestroyVideoSessionKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyVideoSessionKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkDestroyVideoSessionParametersKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkDestroyVideoSessionParametersKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkDeviceWaitIdle", ELIXIR_VK_PROC(elixir_vk_sysv_vkDeviceWaitIdle), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkDisplayPowerControlEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkDisplayPowerControlEXT), 0, "VK_EXT_display_control", CommandLevel::kDevice, 0},
    {"vkEndCommandBuffer", ELIXIR_VK_PROC(elixir_vk_sysv_vkEndCommandBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkEnumerateDeviceExtensionProperties", ELIXIR_VK_PROC(elixir_vk_intercept_vkEnumerateDeviceExtensionProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkEnumerateDeviceLayerProperties", ELIXIR_VK_PROC(elixir_vk_sysv_vkEnumerateDeviceLayerProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkEnumerateInstanceExtensionProperties", ELIXIR_VK_PROC(elixir_vk_intercept_vkEnumerateInstanceExtensionProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kGlobal, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkEnumerateInstanceLayerProperties", ELIXIR_VK_PROC(elixir_vk_intercept_vkEnumerateInstanceLayerProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kGlobal, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkEnumerateInstanceVersion", ELIXIR_VK_PROC(elixir_vk_intercept_vkEnumerateInstanceVersion), VK_API_VERSION_1_1, nullptr, CommandLevel::kGlobal, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkEnumeratePhysicalDeviceGroups", ELIXIR_VK_PROC(elixir_vk_intercept_vkEnumeratePhysicalDeviceGroups), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkEnumeratePhysicalDeviceGroupsKHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkEnumeratePhysicalDeviceGroupsKHR), 0, "VK_KHR_device_group_creation", CommandLevel::kInstance, kCommandIntercepted | kCommandAlias},
    {"vkEnumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkEnumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM), 0, "VK_ARM_performance_counters_by_region", CommandLevel::kInstance, 0},
    {"vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR), 0, "VK_KHR_performance_query", CommandLevel::kInstance, 0},
    {"vkEnumeratePhysicalDeviceShaderInstrumentationMetricsARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkEnumeratePhysicalDeviceShaderInstrumentationMetricsARM), 0, "VK_ARM_shader_instrumentation", CommandLevel::kInstance, 0},
    {"vkEnumeratePhysicalDevices", ELIXIR_VK_PROC(elixir_vk_intercept_vkEnumeratePhysicalDevices), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkFlushMappedMemoryRanges", ELIXIR_VK_PROC(elixir_vk_sysv_vkFlushMappedMemoryRanges), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkFreeCommandBuffers", ELIXIR_VK_PROC(elixir_vk_intercept_vkFreeCommandBuffers), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkFreeDescriptorSets", ELIXIR_VK_PROC(elixir_vk_sysv_vkFreeDescriptorSets), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkFreeMemory", ELIXIR_VK_PROC(elixir_vk_intercept_vkFreeMemory), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetAccelerationStructureBuildSizesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetAccelerationStructureBuildSizesKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkGetAccelerationStructureDeviceAddressKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetAccelerationStructureDeviceAddressKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkGetAccelerationStructureHandleNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetAccelerationStructureHandleNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkGetAccelerationStructureMemoryRequirementsNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetAccelerationStructureMemoryRequirementsNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, 0},
    {"vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkGetAndroidHardwareBufferPropertiesANDROID", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetAndroidHardwareBufferPropertiesANDROID), 0, "VK_ANDROID_external_memory_android_hardware_buffer", CommandLevel::kDevice, kCommandIntercepted | kCommandAndroid | kCommandLibvulkanExport},
    {"vkGetBufferDeviceAddress", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetBufferDeviceAddress), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetBufferDeviceAddressEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetBufferDeviceAddressEXT), 0, "VK_EXT_buffer_device_address", CommandLevel::kDevice, kCommandAlias},
    {"vkGetBufferDeviceAddressKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetBufferDeviceAddressKHR), 0, "VK_KHR_buffer_device_address", CommandLevel::kDevice, kCommandAlias},
    {"vkGetBufferMemoryRequirements", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetBufferMemoryRequirements), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetBufferMemoryRequirements2", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetBufferMemoryRequirements2), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetBufferMemoryRequirements2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetBufferMemoryRequirements2KHR), 0, "VK_KHR_get_memory_requirements2", CommandLevel::kDevice, kCommandAlias},
    {"vkGetBufferOpaqueCaptureAddress", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetBufferOpaqueCaptureAddress), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetBufferOpaqueCaptureAddressKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetBufferOpaqueCaptureAddressKHR), 0, "VK_KHR_buffer_device_address", CommandLevel::kDevice, kCommandAlias},
    {"vkGetBufferOpaqueCaptureDescriptorDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetBufferOpaqueCaptureDescriptorDataEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkGetCalibratedTimestampsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetCalibratedTimestampsEXT), 0, "VK_EXT_calibrated_timestamps", CommandLevel::kDevice, kCommandAlias},
    {"vkGetCalibratedTimestampsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetCalibratedTimestampsKHR), 0, "VK_KHR_calibrated_timestamps", CommandLevel::kDevice, 0},
    {"vkGetClusterAccelerationStructureBuildSizesNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetClusterAccelerationStructureBuildSizesNV), 0, "VK_NV_cluster_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkGetCudaModuleCacheNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetCudaModuleCacheNV), 0, "VK_NV_cuda_kernel_launch", CommandLevel::kDevice, kCommandProvisional},
    {"vkGetDataGraphPipelineAvailablePropertiesARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDataGraphPipelineAvailablePropertiesARM), 0, "VK_ARM_data_graph", CommandLevel::kDevice, 0},
    {"vkGetDataGraphPipelinePropertiesARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDataGraphPipelinePropertiesARM), 0, "VK_ARM_data_graph", CommandLevel::kDevice, 0},
    {"vkGetDataGraphPipelineSessionBindPointRequirementsARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDataGraphPipelineSessionBindPointRequirementsARM), 0, "VK_ARM_data_graph", CommandLevel::kDevice, 0},
    {"vkGetDataGraphPipelineSessionMemoryRequirementsARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDataGraphPipelineSessionMemoryRequirementsARM), 0, "VK_ARM_data_graph", CommandLevel::kDevice, 0},
    {"vkGetDeferredOperationMaxConcurrencyKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeferredOperationMaxConcurrencyKHR), 0, "VK_KHR_deferred_host_operations", CommandLevel::kDevice, 0},
    {"vkGetDeferredOperationResultKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeferredOperationResultKHR), 0, "VK_KHR_deferred_host_operations", CommandLevel::kDevice, 0},
    {"vkGetDescriptorEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDescriptorEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkGetDescriptorSetHostMappingVALVE", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDescriptorSetHostMappingVALVE), 0, "VK_VALVE_descriptor_set_host_mapping", CommandLevel::kDevice, 0},
    {"vkGetDescriptorSetLayoutBindingOffsetEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDescriptorSetLayoutBindingOffsetEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkGetDescriptorSetLayoutHostMappingInfoVALVE", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDescriptorSetLayoutHostMappingInfoVALVE), 0, "VK_VALVE_descriptor_set_host_mapping", CommandLevel::kDevice, 0},
    {"vkGetDescriptorSetLayoutSizeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDescriptorSetLayoutSizeEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkGetDescriptorSetLayoutSupport", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDescriptorSetLayoutSupport), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetDescriptorSetLayoutSupportKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDescriptorSetLayoutSupportKHR), 0, "VK_KHR_maintenance3", CommandLevel::kDevice, kCommandAlias},
    {"vkGetDeviceAccelerationStructureCompatibilityKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceAccelerationStructureCompatibilityKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkGetDeviceBufferMemoryRequirements", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceBufferMemoryRequirements), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetDeviceBufferMemoryRequirementsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceBufferMemoryRequirementsKHR), 0, "VK_KHR_maintenance4", CommandLevel::kDevice, kCommandAlias},
    {"vkGetDeviceCombinedImageSamplerIndexNVX", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceCombinedImageSamplerIndexNVX), 0, "VK_NVX_image_view_handle", CommandLevel::kDevice, 0},
    {"vkGetDeviceFaultDebugInfoKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceFaultDebugInfoKHR), 0, "VK_KHR_device_fault", CommandLevel::kDevice, 0},
    {"vkGetDeviceFaultInfoEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceFaultInfoEXT), 0, "VK_EXT_device_fault", CommandLevel::kDevice, 0},
    {"vkGetDeviceFaultReportsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceFaultReportsKHR), 0, "VK_KHR_device_fault", CommandLevel::kDevice, 0},
    {"vkGetDeviceGroupPeerMemoryFeatures", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceGroupPeerMemoryFeatures), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetDeviceGroupPeerMemoryFeaturesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceGroupPeerMemoryFeaturesKHR), 0, "VK_KHR_device_group", CommandLevel::kDevice, kCommandAlias},
    {"vkGetDeviceGroupPresentCapabilitiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceGroupPresentCapabilitiesKHR), 0, "VK_KHR_swapchain,VK_KHR_device_group", CommandLevel::kDevice, kCommandLibvulkanExport},
    {"vkGetDeviceGroupSurfacePresentModes2EXT", nullptr, 0, "VK_EXT_full_screen_exclusive", CommandLevel::kDevice, kCommandHostOnly},
    {"vkGetDeviceGroupSurfacePresentModesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceGroupSurfacePresentModesKHR), 0, "VK_KHR_swapchain,VK_KHR_device_group", CommandLevel::kDevice, kCommandLibvulkanExport},
    {"vkGetDeviceImageMemoryRequirements", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetDeviceImageMemoryRequirements), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetDeviceImageMemoryRequirementsKHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetDeviceImageMemoryRequirementsKHR), 0, "VK_KHR_maintenance4", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkGetDeviceImageSparseMemoryRequirements", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetDeviceImageSparseMemoryRequirements), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetDeviceImageSparseMemoryRequirementsKHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetDeviceImageSparseMemoryRequirementsKHR), 0, "VK_KHR_maintenance4", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkGetDeviceImageSubresourceLayout", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetDeviceImageSubresourceLayout), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetDeviceImageSubresourceLayoutKHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetDeviceImageSubresourceLayoutKHR), 0, "VK_KHR_maintenance5", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkGetDeviceMemoryCommitment", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceMemoryCommitment), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetDeviceMemoryOpaqueCaptureAddress", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceMemoryOpaqueCaptureAddress), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetDeviceMemoryOpaqueCaptureAddressKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceMemoryOpaqueCaptureAddressKHR), 0, "VK_KHR_buffer_device_address", CommandLevel::kDevice, kCommandAlias},
    {"vkGetDeviceMicromapCompatibilityEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceMicromapCompatibilityEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkGetDeviceProcAddr", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetDeviceProcAddr), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetDeviceQueue", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceQueue), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetDeviceQueue2", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceQueue2), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI), 0, "VK_HUAWEI_subpass_shading", CommandLevel::kDevice, 0},
    {"vkGetDeviceTensorMemoryRequirementsARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDeviceTensorMemoryRequirementsARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkGetDisplayModeProperties2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDisplayModeProperties2KHR), 0, "VK_KHR_get_display_properties2", CommandLevel::kInstance, 0},
    {"vkGetDisplayModePropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDisplayModePropertiesKHR), 0, "VK_KHR_display", CommandLevel::kInstance, 0},
    {"vkGetDisplayPlaneCapabilities2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDisplayPlaneCapabilities2KHR), 0, "VK_KHR_get_display_properties2", CommandLevel::kInstance, 0},
    {"vkGetDisplayPlaneCapabilitiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDisplayPlaneCapabilitiesKHR), 0, "VK_KHR_display", CommandLevel::kInstance, 0},
    {"vkGetDisplayPlaneSupportedDisplaysKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDisplayPlaneSupportedDisplaysKHR), 0, "VK_KHR_display", CommandLevel::kInstance, 0},
    {"vkGetDrmDisplayEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDrmDisplayEXT), 0, "VK_EXT_acquire_drm_display", CommandLevel::kInstance, 0},
    {"vkGetDynamicRenderingTilePropertiesQCOM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetDynamicRenderingTilePropertiesQCOM), 0, "VK_QCOM_tile_properties", CommandLevel::kDevice, 0},
    {"vkGetEncodedVideoSessionParametersKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetEncodedVideoSessionParametersKHR), 0, "VK_KHR_video_encode_queue", CommandLevel::kDevice, 0},
    {"vkGetEventStatus", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetEventStatus), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetExecutionGraphPipelineNodeIndexAMDX", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetExecutionGraphPipelineNodeIndexAMDX), 0, "VK_AMDX_shader_enqueue", CommandLevel::kDevice, kCommandProvisional},
    {"vkGetExecutionGraphPipelineScratchSizeAMDX", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetExecutionGraphPipelineScratchSizeAMDX), 0, "VK_AMDX_shader_enqueue", CommandLevel::kDevice, kCommandProvisional},
    {"vkGetExternalComputeQueueDataNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetExternalComputeQueueDataNV), 0, "VK_NV_external_compute_queue", CommandLevel::kDevice, 0},
    {"vkGetFenceFdKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetFenceFdKHR), 0, "VK_KHR_external_fence_fd", CommandLevel::kDevice, 0},
    {"vkGetFenceStatus", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetFenceStatus), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetFenceWin32HandleKHR", nullptr, 0, "VK_KHR_external_fence_win32", CommandLevel::kDevice, kCommandHostOnly},
    {"vkGetFramebufferTilePropertiesQCOM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetFramebufferTilePropertiesQCOM), 0, "VK_QCOM_tile_properties", CommandLevel::kDevice, 0},
    {"vkGetGeneratedCommandsMemoryRequirementsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetGeneratedCommandsMemoryRequirementsEXT), 0, "VK_EXT_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkGetGeneratedCommandsMemoryRequirementsNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetGeneratedCommandsMemoryRequirementsNV), 0, "VK_NV_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkGetGpaDeviceClockInfoAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetGpaDeviceClockInfoAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkGetGpaSessionResultsAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetGpaSessionResultsAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkGetGpaSessionStatusAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetGpaSessionStatusAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkGetImageDrmFormatModifierPropertiesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageDrmFormatModifierPropertiesEXT), 0, "VK_EXT_image_drm_format_modifier", CommandLevel::kDevice, 0},
    {"vkGetImageMemoryRequirements", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageMemoryRequirements), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetImageMemoryRequirements2", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageMemoryRequirements2), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetImageMemoryRequirements2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageMemoryRequirements2KHR), 0, "VK_KHR_get_memory_requirements2", CommandLevel::kDevice, kCommandAlias},
    {"vkGetImageOpaqueCaptureDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageOpaqueCaptureDataEXT), 0, "VK_EXT_descriptor_heap", CommandLevel::kDevice, 0},
    {"vkGetImageOpaqueCaptureDescriptorDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageOpaqueCaptureDescriptorDataEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkGetImageSparseMemoryRequirements", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageSparseMemoryRequirements), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetImageSparseMemoryRequirements2", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageSparseMemoryRequirements2), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetImageSparseMemoryRequirements2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageSparseMemoryRequirements2KHR), 0, "VK_KHR_get_memory_requirements2", CommandLevel::kDevice, kCommandAlias},
    {"vkGetImageSubresourceLayout", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageSubresourceLayout), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetImageSubresourceLayout2", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageSubresourceLayout2), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetImageSubresourceLayout2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageSubresourceLayout2EXT), 0, "VK_EXT_host_image_copy,VK_EXT_image_compression_control", CommandLevel::kDevice, kCommandAlias | kCommandLibvulkanExport},
    {"vkGetImageSubresourceLayout2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageSubresourceLayout2KHR), 0, "VK_KHR_maintenance5", CommandLevel::kDevice, kCommandAlias},
    {"vkGetImageViewAddressNVX", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageViewAddressNVX), 0, "VK_NVX_image_view_handle", CommandLevel::kDevice, 0},
    {"vkGetImageViewHandle64NVX", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageViewHandle64NVX), 0, "VK_NVX_image_view_handle", CommandLevel::kDevice, 0},
    {"vkGetImageViewHandleNVX", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageViewHandleNVX), 0, "VK_NVX_image_view_handle", CommandLevel::kDevice, 0},
    {"vkGetImageViewOpaqueCaptureDescriptorDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetImageViewOpaqueCaptureDescriptorDataEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkGetInstanceProcAddr", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetInstanceProcAddr), VK_API_VERSION_1_0, nullptr, CommandLevel::kGlobal, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetLatencyTimingsNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetLatencyTimingsNV), 0, "VK_NV_low_latency2", CommandLevel::kDevice, 0},
    {"vkGetMemoryAndroidHardwareBufferANDROID", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetMemoryAndroidHardwareBufferANDROID), 0, "VK_ANDROID_external_memory_android_hardware_buffer", CommandLevel::kDevice, kCommandIntercepted | kCommandAndroid | kCommandLibvulkanExport},
    {"vkGetMemoryFdKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetMemoryFdKHR), 0, "VK_KHR_external_memory_fd", CommandLevel::kDevice, 0},
    {"vkGetMemoryFdPropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetMemoryFdPropertiesKHR), 0, "VK_KHR_external_memory_fd", CommandLevel::kDevice, 0},
    {"vkGetMemoryHostPointerPropertiesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetMemoryHostPointerPropertiesEXT), 0, "VK_EXT_external_memory_host", CommandLevel::kDevice, 0},
    {"vkGetMemoryRemoteAddressNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetMemoryRemoteAddressNV), 0, "VK_NV_external_memory_rdma", CommandLevel::kDevice, 0},
    {"vkGetMemoryWin32HandleKHR", nullptr, 0, "VK_KHR_external_memory_win32", CommandLevel::kDevice, kCommandHostOnly},
    {"vkGetMemoryWin32HandleNV", nullptr, 0, "VK_NV_external_memory_win32", CommandLevel::kDevice, kCommandHostOnly},
    {"vkGetMemoryWin32HandlePropertiesKHR", nullptr, 0, "VK_KHR_external_memory_win32", CommandLevel::kDevice, kCommandHostOnly},
    {"vkGetMicromapBuildSizesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetMicromapBuildSizesEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkGetPartitionedAccelerationStructuresBuildSizesNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPartitionedAccelerationStructuresBuildSizesNV), 0, "VK_NV_partitioned_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkGetPastPresentationTimingEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPastPresentationTimingEXT), 0, "VK_EXT_present_timing", CommandLevel::kDevice, 0},
    {"vkGetPastPresentationTimingGOOGLE", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPastPresentationTimingGOOGLE), 0, "VK_GOOGLE_display_timing", CommandLevel::kDevice, 0},
    {"vkGetPerformanceParameterINTEL", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPerformanceParameterINTEL), 0, "VK_INTEL_performance_query", CommandLevel::kDevice, 0},
    {"vkGetPhysicalDeviceCalibrateableTimeDomainsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT), 0, "VK_EXT_calibrated_timestamps", CommandLevel::kInstance, kCommandAlias},
    {"vkGetPhysicalDeviceCalibrateableTimeDomainsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR), 0, "VK_KHR_calibrated_timestamps", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV), 0, "VK_NV_cooperative_matrix2", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR), 0, "VK_KHR_cooperative_matrix", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceCooperativeMatrixPropertiesNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV), 0, "VK_NV_cooperative_matrix", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceCooperativeVectorPropertiesNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceCooperativeVectorPropertiesNV), 0, "VK_NV_cooperative_vector", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceDescriptorSizeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceDescriptorSizeEXT), 0, "VK_EXT_descriptor_heap", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceDisplayPlaneProperties2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceDisplayPlaneProperties2KHR), 0, "VK_KHR_get_display_properties2", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceDisplayPlanePropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceDisplayPlanePropertiesKHR), 0, "VK_KHR_display", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceDisplayProperties2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceDisplayProperties2KHR), 0, "VK_KHR_get_display_properties2", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceDisplayPropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceDisplayPropertiesKHR), 0, "VK_KHR_display", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceExternalBufferProperties", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceExternalBufferProperties), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceExternalBufferPropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceExternalBufferPropertiesKHR), 0, "VK_KHR_external_memory_capabilities", CommandLevel::kInstance, kCommandAlias},
    {"vkGetPhysicalDeviceExternalFenceProperties", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceExternalFenceProperties), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceExternalFencePropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceExternalFencePropertiesKHR), 0, "VK_KHR_external_fence_capabilities", CommandLevel::kInstance, kCommandAlias},
    {"vkGetPhysicalDeviceExternalImageFormatPropertiesNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceExternalImageFormatPropertiesNV), 0, "VK_NV_external_memory_capabilities", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceExternalSemaphoreProperties", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceExternalSemaphoreProperties), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceExternalSemaphorePropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR), 0, "VK_KHR_external_semaphore_capabilities", CommandLevel::kInstance, kCommandAlias},
    {"vkGetPhysicalDeviceExternalTensorPropertiesARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceExternalTensorPropertiesARM), 0, "VK_ARM_tensors", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceFeatures", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceFeatures), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceFeatures2", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceFeatures2), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceFeatures2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceFeatures2KHR), 0, "VK_KHR_get_physical_device_properties2", CommandLevel::kInstance, kCommandIntercepted | kCommandAlias},
    {"vkGetPhysicalDeviceFormatProperties", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceFormatProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceFormatProperties2", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceFormatProperties2), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceFormatProperties2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceFormatProperties2KHR), 0, "VK_KHR_get_physical_device_properties2", CommandLevel::kInstance, kCommandIntercepted | kCommandAlias},
    {"vkGetPhysicalDeviceFragmentShadingRatesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceFragmentShadingRatesKHR), 0, "VK_KHR_fragment_shading_rate", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceImageFormatProperties", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceImageFormatProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceImageFormatProperties2", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceImageFormatProperties2), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceImageFormatProperties2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceImageFormatProperties2KHR), 0, "VK_KHR_get_physical_device_properties2", CommandLevel::kInstance, kCommandIntercepted | kCommandAlias},
    {"vkGetPhysicalDeviceMemoryProperties", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceMemoryProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceMemoryProperties2", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceMemoryProperties2), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceMemoryProperties2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceMemoryProperties2KHR), 0, "VK_KHR_get_physical_device_properties2", CommandLevel::kInstance, kCommandAlias},
    {"vkGetPhysicalDeviceMultisamplePropertiesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceMultisamplePropertiesEXT), 0, "VK_EXT_sample_locations", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceOpticalFlowImageFormatsNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceOpticalFlowImageFormatsNV), 0, "VK_NV_optical_flow", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDevicePresentRectanglesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDevicePresentRectanglesKHR), 0, "VK_KHR_swapchain,VK_KHR_device_group", CommandLevel::kInstance, kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceProperties", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceProperties2", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceProperties2), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceProperties2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceProperties2KHR), 0, "VK_KHR_get_physical_device_properties2", CommandLevel::kInstance, kCommandIntercepted | kCommandAlias},
    {"vkGetPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM), 0, "VK_ARM_data_graph_instruction_set_tosa,VK_ARM_data_graph_optical_flow", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM), 0, "VK_ARM_data_graph_optical_flow", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM), 0, "VK_ARM_data_graph", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM), 0, "VK_ARM_data_graph", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR), 0, "VK_KHR_performance_query", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceQueueFamilyProperties", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceQueueFamilyProperties2", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyProperties2), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceQueueFamilyProperties2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceQueueFamilyProperties2KHR), 0, "VK_KHR_get_physical_device_properties2", CommandLevel::kInstance, kCommandAlias},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSparseImageFormatProperties), VK_API_VERSION_1_0, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSparseImageFormatProperties2), VK_API_VERSION_1_1, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSparseImageFormatProperties2KHR), 0, "VK_KHR_get_physical_device_properties2", CommandLevel::kInstance, kCommandAlias},
    {"vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV), 0, "VK_NV_coverage_reduction_mode", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceSurfaceCapabilities2EXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSurfaceCapabilities2EXT), 0, "VK_EXT_display_surface_counter", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceSurfaceCapabilities2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSurfaceCapabilities2KHR), 0, "VK_KHR_get_surface_capabilities2", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSurfaceCapabilitiesKHR), 0, "VK_KHR_surface", CommandLevel::kInstance, kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceSurfaceFormats2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSurfaceFormats2KHR), 0, "VK_KHR_get_surface_capabilities2", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSurfaceFormatsKHR), 0, "VK_KHR_surface", CommandLevel::kInstance, kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceSurfacePresentModes2EXT", nullptr, 0, "VK_EXT_full_screen_exclusive", CommandLevel::kInstance, kCommandHostOnly},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkGetPhysicalDeviceSurfacePresentModesKHR), 0, "VK_KHR_surface", CommandLevel::kInstance, kCommandIntercepted | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceSurfaceSupportKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceSurfaceSupportKHR), 0, "VK_KHR_surface", CommandLevel::kInstance, kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceToolProperties", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceToolProperties), VK_API_VERSION_1_3, nullptr, CommandLevel::kInstance, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPhysicalDeviceToolPropertiesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceToolPropertiesEXT), 0, "VK_EXT_tooling_info", CommandLevel::kInstance, kCommandAlias},
    {"vkGetPhysicalDeviceVideoCapabilitiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceVideoCapabilitiesKHR), 0, "VK_KHR_video_queue", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR), 0, "VK_KHR_video_encode_queue", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceVideoFormatPropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPhysicalDeviceVideoFormatPropertiesKHR), 0, "VK_KHR_video_queue", CommandLevel::kInstance, 0},
    {"vkGetPhysicalDeviceWin32PresentationSupportKHR", nullptr, 0, "VK_KHR_win32_surface", CommandLevel::kInstance, kCommandHostOnly},
    {"vkGetPipelineBinaryDataKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPipelineBinaryDataKHR), 0, "VK_KHR_pipeline_binary", CommandLevel::kDevice, 0},
    {"vkGetPipelineCacheData", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPipelineCacheData), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPipelineExecutableInternalRepresentationsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPipelineExecutableInternalRepresentationsKHR), 0, "VK_KHR_pipeline_executable_properties", CommandLevel::kDevice, 0},
    {"vkGetPipelineExecutablePropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPipelineExecutablePropertiesKHR), 0, "VK_KHR_pipeline_executable_properties", CommandLevel::kDevice, 0},
    {"vkGetPipelineExecutableStatisticsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPipelineExecutableStatisticsKHR), 0, "VK_KHR_pipeline_executable_properties", CommandLevel::kDevice, 0},
    {"vkGetPipelineIndirectDeviceAddressNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPipelineIndirectDeviceAddressNV), 0, "VK_NV_device_generated_commands_compute", CommandLevel::kDevice, 0},
    {"vkGetPipelineIndirectMemoryRequirementsNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPipelineIndirectMemoryRequirementsNV), 0, "VK_NV_device_generated_commands_compute", CommandLevel::kDevice, 0},
    {"vkGetPipelineKeyKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPipelineKeyKHR), 0, "VK_KHR_pipeline_binary", CommandLevel::kDevice, 0},
    {"vkGetPipelinePropertiesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPipelinePropertiesEXT), 0, "VK_EXT_pipeline_properties", CommandLevel::kDevice, 0},
    {"vkGetPrivateData", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPrivateData), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetPrivateDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetPrivateDataEXT), 0, "VK_EXT_private_data", CommandLevel::kDevice, kCommandAlias},
    {"vkGetQueryPoolResults", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetQueryPoolResults), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetQueueCheckpointData2NV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetQueueCheckpointData2NV), 0, "VK_NV_device_diagnostic_checkpoints", CommandLevel::kDevice, 0},
    {"vkGetQueueCheckpointDataNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetQueueCheckpointDataNV), 0, "VK_NV_device_diagnostic_checkpoints", CommandLevel::kDevice, 0},
    {"vkGetRayTracingCaptureReplayShaderGroupHandlesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR), 0, "VK_KHR_ray_tracing_pipeline", CommandLevel::kDevice, 0},
    {"vkGetRayTracingShaderGroupHandlesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetRayTracingShaderGroupHandlesKHR), 0, "VK_KHR_ray_tracing_pipeline", CommandLevel::kDevice, 0},
    {"vkGetRayTracingShaderGroupHandlesNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetRayTracingShaderGroupHandlesNV), 0, "VK_NV_ray_tracing", CommandLevel::kDevice, kCommandAlias},
    {"vkGetRayTracingShaderGroupStackSizeKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetRayTracingShaderGroupStackSizeKHR), 0, "VK_KHR_ray_tracing_pipeline", CommandLevel::kDevice, 0},
    {"vkGetRefreshCycleDurationGOOGLE", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetRefreshCycleDurationGOOGLE), 0, "VK_GOOGLE_display_timing", CommandLevel::kDevice, 0},
    {"vkGetRenderAreaGranularity", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetRenderAreaGranularity), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetRenderingAreaGranularity", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetRenderingAreaGranularity), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetRenderingAreaGranularityKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetRenderingAreaGranularityKHR), 0, "VK_KHR_maintenance5", CommandLevel::kDevice, kCommandAlias},
    {"vkGetSamplerOpaqueCaptureDescriptorDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetSamplerOpaqueCaptureDescriptorDataEXT), 0, "VK_EXT_descriptor_buffer", CommandLevel::kDevice, 0},
    {"vkGetSemaphoreCounterValue", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetSemaphoreCounterValue), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkGetSemaphoreCounterValueKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetSemaphoreCounterValueKHR), 0, "VK_KHR_timeline_semaphore", CommandLevel::kDevice, kCommandAlias},
    {"vkGetSemaphoreFdKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetSemaphoreFdKHR), 0, "VK_KHR_external_semaphore_fd", CommandLevel::kDevice, 0},
    {"vkGetSemaphoreWin32HandleKHR", nullptr, 0, "VK_KHR_external_semaphore_win32", CommandLevel::kDevice, kCommandHostOnly},
    {"vkGetShaderBinaryDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetShaderBinaryDataEXT), 0, "VK_EXT_shader_object", CommandLevel::kDevice, 0},
    {"vkGetShaderInfoAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetShaderInfoAMD), 0, "VK_AMD_shader_info", CommandLevel::kDevice, 0},
    {"vkGetShaderInstrumentationValuesARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetShaderInstrumentationValuesARM), 0, "VK_ARM_shader_instrumentation", CommandLevel::kDevice, 0},
    {"vkGetShaderModuleCreateInfoIdentifierEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetShaderModuleCreateInfoIdentifierEXT), 0, "VK_EXT_shader_module_identifier", CommandLevel::kDevice, 0},
    {"vkGetShaderModuleIdentifierEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetShaderModuleIdentifierEXT), 0, "VK_EXT_shader_module_identifier", CommandLevel::kDevice, 0},
    {"vkGetSwapchainCounterEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetSwapchainCounterEXT), 0, "VK_EXT_display_control", CommandLevel::kDevice, 0},
    {"vkGetSwapchainImagesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetSwapchainImagesKHR), 0, "VK_KHR_swapchain", CommandLevel::kDevice, kCommandLibvulkanExport},
    {"vkGetSwapchainStatusKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetSwapchainStatusKHR), 0, "VK_KHR_shared_presentable_image", CommandLevel::kDevice, 0},
    {"vkGetSwapchainTimeDomainPropertiesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetSwapchainTimeDomainPropertiesEXT), 0, "VK_EXT_present_timing", CommandLevel::kDevice, 0},
    {"vkGetSwapchainTimingPropertiesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetSwapchainTimingPropertiesEXT), 0, "VK_EXT_present_timing", CommandLevel::kDevice, 0},
    {"vkGetTensorMemoryRequirementsARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetTensorMemoryRequirementsARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkGetTensorOpaqueCaptureDataARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetTensorOpaqueCaptureDataARM), 0, "VK_EXT_descriptor_heap", CommandLevel::kDevice, 0},
    {"vkGetTensorOpaqueCaptureDescriptorDataARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetTensorOpaqueCaptureDescriptorDataARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkGetTensorViewOpaqueCaptureDescriptorDataARM", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetTensorViewOpaqueCaptureDescriptorDataARM), 0, "VK_ARM_tensors", CommandLevel::kDevice, 0},
    {"vkGetValidationCacheDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetValidationCacheDataEXT), 0, "VK_EXT_validation_cache", CommandLevel::kDevice, 0},
    {"vkGetVideoSessionMemoryRequirementsKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkGetVideoSessionMemoryRequirementsKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkGetWinrtDisplayNV", nullptr, 0, "VK_NV_acquire_winrt_display", CommandLevel::kInstance, kCommandHostOnly},
    {"vkImportFenceFdKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkImportFenceFdKHR), 0, "VK_KHR_external_fence_fd", CommandLevel::kDevice, 0},
    {"vkImportFenceWin32HandleKHR", nullptr, 0, "VK_KHR_external_fence_win32", CommandLevel::kDevice, kCommandHostOnly},
    {"vkImportSemaphoreFdKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkImportSemaphoreFdKHR), 0, "VK_KHR_external_semaphore_fd", CommandLevel::kDevice, 0},
    {"vkImportSemaphoreWin32HandleKHR", nullptr, 0, "VK_KHR_external_semaphore_win32", CommandLevel::kDevice, kCommandHostOnly},
    {"vkInitializePerformanceApiINTEL", ELIXIR_VK_PROC(elixir_vk_sysv_vkInitializePerformanceApiINTEL), 0, "VK_INTEL_performance_query", CommandLevel::kDevice, 0},
    {"vkInvalidateMappedMemoryRanges", ELIXIR_VK_PROC(elixir_vk_sysv_vkInvalidateMappedMemoryRanges), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkLatencySleepNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkLatencySleepNV), 0, "VK_NV_low_latency2", CommandLevel::kDevice, 0},
    {"vkMapMemory", ELIXIR_VK_PROC(elixir_vk_intercept_vkMapMemory), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkMapMemory2", ELIXIR_VK_PROC(elixir_vk_intercept_vkMapMemory2), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkMapMemory2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkMapMemory2KHR), 0, "VK_KHR_map_memory2", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkMergePipelineCaches", ELIXIR_VK_PROC(elixir_vk_sysv_vkMergePipelineCaches), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkMergeValidationCachesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkMergeValidationCachesEXT), 0, "VK_EXT_validation_cache", CommandLevel::kDevice, 0},
    {"vkQueueBeginDebugUtilsLabelEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkQueueBeginDebugUtilsLabelEXT), 0, "VK_EXT_debug_utils", CommandLevel::kDevice, 0},
    {"vkQueueBindSparse", ELIXIR_VK_PROC(elixir_vk_sysv_vkQueueBindSparse), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkQueueEndDebugUtilsLabelEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkQueueEndDebugUtilsLabelEXT), 0, "VK_EXT_debug_utils", CommandLevel::kDevice, 0},
    {"vkQueueInsertDebugUtilsLabelEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkQueueInsertDebugUtilsLabelEXT), 0, "VK_EXT_debug_utils", CommandLevel::kDevice, 0},
    {"vkQueueNotifyOutOfBandNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkQueueNotifyOutOfBandNV), 0, "VK_NV_low_latency2", CommandLevel::kDevice, 0},
    {"vkQueuePresentKHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkQueuePresentKHR), 0, "VK_KHR_swapchain", CommandLevel::kDevice, kCommandIntercepted | kCommandLibvulkanExport},
    {"vkQueueSetPerfHintQCOM", ELIXIR_VK_PROC(elixir_vk_sysv_vkQueueSetPerfHintQCOM), 0, "VK_QCOM_queue_perf_hint", CommandLevel::kDevice, 0},
    {"vkQueueSetPerformanceConfigurationINTEL", ELIXIR_VK_PROC(elixir_vk_sysv_vkQueueSetPerformanceConfigurationINTEL), 0, "VK_INTEL_performance_query", CommandLevel::kDevice, 0},
    {"vkQueueSubmit", ELIXIR_VK_PROC(elixir_vk_intercept_vkQueueSubmit), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkQueueSubmit2", ELIXIR_VK_PROC(elixir_vk_intercept_vkQueueSubmit2), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkQueueSubmit2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkQueueSubmit2KHR), 0, "VK_KHR_synchronization2", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkQueueWaitIdle", ELIXIR_VK_PROC(elixir_vk_sysv_vkQueueWaitIdle), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkRegisterCustomBorderColorEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkRegisterCustomBorderColorEXT), 0, "VK_EXT_descriptor_heap", CommandLevel::kDevice, 0},
    {"vkRegisterDeviceEventEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkRegisterDeviceEventEXT), 0, "VK_EXT_display_control", CommandLevel::kDevice, 0},
    {"vkRegisterDisplayEventEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkRegisterDisplayEventEXT), 0, "VK_EXT_display_control", CommandLevel::kDevice, 0},
    {"vkReleaseCapturedPipelineDataKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkReleaseCapturedPipelineDataKHR), 0, "VK_KHR_pipeline_binary", CommandLevel::kDevice, 0},
    {"vkReleaseDisplayEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkReleaseDisplayEXT), 0, "VK_EXT_direct_mode_display", CommandLevel::kInstance, 0},
    {"vkReleaseFullScreenExclusiveModeEXT", nullptr, 0, "VK_EXT_full_screen_exclusive", CommandLevel::kDevice, kCommandHostOnly},
    {"vkReleasePerformanceConfigurationINTEL", ELIXIR_VK_PROC(elixir_vk_sysv_vkReleasePerformanceConfigurationINTEL), 0, "VK_INTEL_performance_query", CommandLevel::kDevice, 0},
    {"vkReleaseProfilingLockKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkReleaseProfilingLockKHR), 0, "VK_KHR_performance_query", CommandLevel::kDevice, 0},
    {"vkReleaseSwapchainImagesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkReleaseSwapchainImagesEXT), 0, "VK_EXT_swapchain_maintenance1", CommandLevel::kDevice, kCommandAlias},
    {"vkReleaseSwapchainImagesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkReleaseSwapchainImagesKHR), 0, "VK_KHR_swapchain_maintenance1", CommandLevel::kDevice, 0},
    {"vkResetCommandBuffer", ELIXIR_VK_PROC(elixir_vk_intercept_vkResetCommandBuffer), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkResetCommandPool", ELIXIR_VK_PROC(elixir_vk_intercept_vkResetCommandPool), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkResetDescriptorPool", ELIXIR_VK_PROC(elixir_vk_sysv_vkResetDescriptorPool), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkResetEvent", ELIXIR_VK_PROC(elixir_vk_sysv_vkResetEvent), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkResetFences", ELIXIR_VK_PROC(elixir_vk_sysv_vkResetFences), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkResetGpaSessionAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkResetGpaSessionAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkResetQueryPool", ELIXIR_VK_PROC(elixir_vk_sysv_vkResetQueryPool), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkResetQueryPoolEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkResetQueryPoolEXT), 0, "VK_EXT_host_query_reset", CommandLevel::kDevice, kCommandAlias},
    {"vkSetDebugUtilsObjectNameEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetDebugUtilsObjectNameEXT), 0, "VK_EXT_debug_utils", CommandLevel::kDevice, 0},
    {"vkSetDebugUtilsObjectTagEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetDebugUtilsObjectTagEXT), 0, "VK_EXT_debug_utils", CommandLevel::kDevice, 0},
    {"vkSetDeviceMemoryPriorityEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetDeviceMemoryPriorityEXT), 0, "VK_EXT_pageable_device_local_memory", CommandLevel::kDevice, 0},
    {"vkSetEvent", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetEvent), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkSetGpaDeviceClockModeAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetGpaDeviceClockModeAMD), 0, "VK_AMD_gpa_interface", CommandLevel::kDevice, 0},
    {"vkSetHdrMetadataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetHdrMetadataEXT), 0, "VK_EXT_hdr_metadata", CommandLevel::kDevice, 0},
    {"vkSetLatencyMarkerNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetLatencyMarkerNV), 0, "VK_NV_low_latency2", CommandLevel::kDevice, 0},
    {"vkSetLatencySleepModeNV", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetLatencySleepModeNV), 0, "VK_NV_low_latency2", CommandLevel::kDevice, 0},
    {"vkSetLocalDimmingAMD", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetLocalDimmingAMD), 0, "VK_AMD_display_native_hdr", CommandLevel::kDevice, 0},
    {"vkSetPrivateData", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetPrivateData), VK_API_VERSION_1_3, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkSetPrivateDataEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetPrivateDataEXT), 0, "VK_EXT_private_data", CommandLevel::kDevice, kCommandAlias},
    {"vkSetSwapchainPresentTimingQueueSizeEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkSetSwapchainPresentTimingQueueSizeEXT), 0, "VK_EXT_present_timing", CommandLevel::kDevice, 0},
    {"vkSignalSemaphore", ELIXIR_VK_PROC(elixir_vk_sysv_vkSignalSemaphore), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkSignalSemaphoreKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkSignalSemaphoreKHR), 0, "VK_KHR_timeline_semaphore", CommandLevel::kDevice, kCommandAlias},
    {"vkSubmitDebugUtilsMessageEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkSubmitDebugUtilsMessageEXT), 0, "VK_EXT_debug_utils", CommandLevel::kInstance, 0},
    {"vkTransitionImageLayout", ELIXIR_VK_PROC(elixir_vk_sysv_vkTransitionImageLayout), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkTransitionImageLayoutEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkTransitionImageLayoutEXT), 0, "VK_EXT_host_image_copy", CommandLevel::kDevice, kCommandAlias},
    {"vkTrimCommandPool", ELIXIR_VK_PROC(elixir_vk_sysv_vkTrimCommandPool), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkTrimCommandPoolKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkTrimCommandPoolKHR), 0, "VK_KHR_maintenance1", CommandLevel::kDevice, kCommandAlias},
    {"vkUninitializePerformanceApiINTEL", ELIXIR_VK_PROC(elixir_vk_sysv_vkUninitializePerformanceApiINTEL), 0, "VK_INTEL_performance_query", CommandLevel::kDevice, 0},
    {"vkUnmapMemory", ELIXIR_VK_PROC(elixir_vk_intercept_vkUnmapMemory), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkUnmapMemory2", ELIXIR_VK_PROC(elixir_vk_intercept_vkUnmapMemory2), VK_API_VERSION_1_4, nullptr, CommandLevel::kDevice, kCommandCore | kCommandIntercepted | kCommandLibvulkanExport},
    {"vkUnmapMemory2KHR", ELIXIR_VK_PROC(elixir_vk_intercept_vkUnmapMemory2KHR), 0, "VK_KHR_map_memory2", CommandLevel::kDevice, kCommandIntercepted | kCommandAlias},
    {"vkUnregisterCustomBorderColorEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkUnregisterCustomBorderColorEXT), 0, "VK_EXT_descriptor_heap", CommandLevel::kDevice, 0},
    {"vkUpdateDescriptorSetWithTemplate", ELIXIR_VK_PROC(elixir_vk_sysv_vkUpdateDescriptorSetWithTemplate), VK_API_VERSION_1_1, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkUpdateDescriptorSetWithTemplateKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkUpdateDescriptorSetWithTemplateKHR), 0, "VK_KHR_descriptor_update_template", CommandLevel::kDevice, kCommandAlias},
    {"vkUpdateDescriptorSets", ELIXIR_VK_PROC(elixir_vk_sysv_vkUpdateDescriptorSets), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkUpdateIndirectExecutionSetPipelineEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkUpdateIndirectExecutionSetPipelineEXT), 0, "VK_EXT_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkUpdateIndirectExecutionSetShaderEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkUpdateIndirectExecutionSetShaderEXT), 0, "VK_EXT_device_generated_commands", CommandLevel::kDevice, 0},
    {"vkUpdateVideoSessionParametersKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkUpdateVideoSessionParametersKHR), 0, "VK_KHR_video_queue", CommandLevel::kDevice, 0},
    {"vkWaitForFences", ELIXIR_VK_PROC(elixir_vk_sysv_vkWaitForFences), VK_API_VERSION_1_0, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkWaitForPresent2KHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkWaitForPresent2KHR), 0, "VK_KHR_present_wait2", CommandLevel::kDevice, 0},
    {"vkWaitForPresentKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkWaitForPresentKHR), 0, "VK_KHR_present_wait", CommandLevel::kDevice, 0},
    {"vkWaitSemaphores", ELIXIR_VK_PROC(elixir_vk_sysv_vkWaitSemaphores), VK_API_VERSION_1_2, nullptr, CommandLevel::kDevice, kCommandCore | kCommandLibvulkanExport},
    {"vkWaitSemaphoresKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkWaitSemaphoresKHR), 0, "VK_KHR_timeline_semaphore", CommandLevel::kDevice, kCommandAlias},
    {"vkWriteAccelerationStructuresPropertiesKHR", ELIXIR_VK_PROC(elixir_vk_sysv_vkWriteAccelerationStructuresPropertiesKHR), 0, "VK_KHR_acceleration_structure", CommandLevel::kDevice, 0},
    {"vkWriteMicromapsPropertiesEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkWriteMicromapsPropertiesEXT), 0, "VK_EXT_opacity_micromap", CommandLevel::kDevice, 0},
    {"vkWriteResourceDescriptorsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkWriteResourceDescriptorsEXT), 0, "VK_EXT_descriptor_heap", CommandLevel::kDevice, 0},
    {"vkWriteSamplerDescriptorsEXT", ELIXIR_VK_PROC(elixir_vk_sysv_vkWriteSamplerDescriptorsEXT), 0, "VK_EXT_descriptor_heap", CommandLevel::kDevice, 0},
};

#undef ELIXIR_VK_PROC

const CommandInfo* FindCommand(const char* name) noexcept {
  if (name == nullptr) return nullptr;
  std::size_t low = 0;
  std::size_t high = kCommandCount;
  while (low < high) {
    const std::size_t mid = low + (high - low) / 2;
    const int order = std::strcmp(kCommands[mid].name, name);
    if (order == 0) return &kCommands[mid];
    if (order < 0) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return nullptr;
}

std::size_t StructSizeForSType(VkStructureType type) noexcept {
  switch (type) {
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR:
      return sizeof(VkAccelerationStructureBuildGeometryInfoKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR:
      return sizeof(VkAccelerationStructureBuildSizesInfoKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:
      return sizeof(VkAccelerationStructureCaptureDescriptorDataInfoEXT);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_2_KHR:
      return sizeof(VkAccelerationStructureCreateInfo2KHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR:
      return sizeof(VkAccelerationStructureCreateInfoKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV:
      return sizeof(VkAccelerationStructureCreateInfoNV);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DENSE_GEOMETRY_FORMAT_TRIANGLES_DATA_AMDX:
      return sizeof(VkAccelerationStructureDenseGeometryFormatTrianglesDataAMDX);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR:
      return sizeof(VkAccelerationStructureDeviceAddressInfoKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR:
      return sizeof(VkAccelerationStructureGeometryAabbsDataKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR:
      return sizeof(VkAccelerationStructureGeometryInstancesDataKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR:
      return sizeof(VkAccelerationStructureGeometryKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_LINEAR_SWEPT_SPHERES_DATA_NV:
      return sizeof(VkAccelerationStructureGeometryLinearSweptSpheresDataNV);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_MICROMAP_DATA_KHR:
      return sizeof(VkAccelerationStructureGeometryMicromapDataKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_MOTION_TRIANGLES_DATA_NV:
      return sizeof(VkAccelerationStructureGeometryMotionTrianglesDataNV);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_SPHERES_DATA_NV:
      return sizeof(VkAccelerationStructureGeometrySpheresDataNV);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR:
      return sizeof(VkAccelerationStructureGeometryTrianglesDataKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV:
      return sizeof(VkAccelerationStructureInfoNV);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV:
      return sizeof(VkAccelerationStructureMemoryRequirementsInfoNV);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MOTION_INFO_NV:
      return sizeof(VkAccelerationStructureMotionInfoNV);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_DISPLACEMENT_MICROMAP_NV:
      return sizeof(VkAccelerationStructureTrianglesDisplacementMicromapNV);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT:
      return sizeof(VkAccelerationStructureTrianglesOpacityMicromapEXT);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_KHR:
      return sizeof(VkAccelerationStructureTrianglesOpacityMicromapKHR);
    case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_VERSION_INFO_KHR:
      return sizeof(VkAccelerationStructureVersionInfoKHR);
    case VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR:
      return sizeof(VkAcquireNextImageInfoKHR);
    case VK_STRUCTURE_TYPE_ACQUIRE_PROFILING_LOCK_INFO_KHR:
      return sizeof(VkAcquireProfilingLockInfoKHR);
    case VK_STRUCTURE_TYPE_AMIGO_PROFILING_SUBMIT_INFO_SEC:
      return sizeof(VkAmigoProfilingSubmitInfoSEC);
    case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_2_ANDROID:
      return sizeof(VkAndroidHardwareBufferFormatProperties2ANDROID);
    case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID:
      return sizeof(VkAndroidHardwareBufferFormatPropertiesANDROID);
    case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_RESOLVE_PROPERTIES_ANDROID:
      return sizeof(VkAndroidHardwareBufferFormatResolvePropertiesANDROID);
    case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID:
      return sizeof(VkAndroidHardwareBufferPropertiesANDROID);
    case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID:
      return sizeof(VkAndroidHardwareBufferUsageANDROID);
    case VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR:
      return sizeof(VkAndroidSurfaceCreateInfoKHR);
    case VK_STRUCTURE_TYPE_ANTI_LAG_DATA_AMD:
      return sizeof(VkAntiLagDataAMD);
    case VK_STRUCTURE_TYPE_ANTI_LAG_PRESENTATION_INFO_AMD:
      return sizeof(VkAntiLagPresentationInfoAMD);
    case VK_STRUCTURE_TYPE_APPLICATION_INFO:
      return sizeof(VkApplicationInfo);
    case VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2:
      return sizeof(VkAttachmentDescription2);
    case VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT:
      return sizeof(VkAttachmentDescriptionStencilLayout);
    case VK_STRUCTURE_TYPE_ATTACHMENT_FEEDBACK_LOOP_INFO_EXT:
      return sizeof(VkAttachmentFeedbackLoopInfoEXT);
    case VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2:
      return sizeof(VkAttachmentReference2);
    case VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT:
      return sizeof(VkAttachmentReferenceStencilLayout);
    case VK_STRUCTURE_TYPE_ATTACHMENT_SAMPLE_COUNT_INFO_AMD:
      return sizeof(VkAttachmentSampleCountInfoAMD);
    case VK_STRUCTURE_TYPE_BEGIN_CUSTOM_RESOLVE_INFO_EXT:
      return sizeof(VkBeginCustomResolveInfoEXT);
    case VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV:
      return sizeof(VkBindAccelerationStructureMemoryInfoNV);
    case VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO:
      return sizeof(VkBindBufferMemoryDeviceGroupInfo);
    case VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO:
      return sizeof(VkBindBufferMemoryInfo);
    case VK_STRUCTURE_TYPE_BIND_DATA_GRAPH_PIPELINE_SESSION_MEMORY_INFO_ARM:
      return sizeof(VkBindDataGraphPipelineSessionMemoryInfoARM);
    case VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_BUFFER_EMBEDDED_SAMPLERS_INFO_EXT:
      return sizeof(VkBindDescriptorBufferEmbeddedSamplersInfoEXT);
    case VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO:
      return sizeof(VkBindDescriptorSetsInfo);
    case VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT:
      return sizeof(VkBindHeapInfoEXT);
    case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO:
      return sizeof(VkBindImageMemoryDeviceGroupInfo);
    case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO:
      return sizeof(VkBindImageMemoryInfo);
    case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR:
      return sizeof(VkBindImageMemorySwapchainInfoKHR);
    case VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO:
      return sizeof(VkBindImagePlaneMemoryInfo);
    case VK_STRUCTURE_TYPE_BIND_INDEX_BUFFER_3_INFO_KHR:
      return sizeof(VkBindIndexBuffer3InfoKHR);
    case VK_STRUCTURE_TYPE_BIND_MEMORY_STATUS:
      return sizeof(VkBindMemoryStatus);
    case VK_STRUCTURE_TYPE_BIND_SPARSE_INFO:
      return sizeof(VkBindSparseInfo);
    case VK_STRUCTURE_TYPE_BIND_TENSOR_MEMORY_INFO_ARM:
      return sizeof(VkBindTensorMemoryInfoARM);
    case VK_STRUCTURE_TYPE_BIND_TRANSFORM_FEEDBACK_BUFFER_2_INFO_EXT:
      return sizeof(VkBindTransformFeedbackBuffer2InfoEXT);
    case VK_STRUCTURE_TYPE_BIND_VERTEX_BUFFER_3_INFO_KHR:
      return sizeof(VkBindVertexBuffer3InfoKHR);
    case VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR:
      return sizeof(VkBindVideoSessionMemoryInfoKHR);
    case VK_STRUCTURE_TYPE_BLIT_IMAGE_CUBIC_WEIGHTS_INFO_QCOM:
      return sizeof(VkBlitImageCubicWeightsInfoQCOM);
    case VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2:
      return sizeof(VkBlitImageInfo2);
    case VK_STRUCTURE_TYPE_BUFFER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:
      return sizeof(VkBufferCaptureDescriptorDataInfoEXT);
    case VK_STRUCTURE_TYPE_BUFFER_COPY_2:
      return sizeof(VkBufferCopy2);
    case VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO:
      return sizeof(VkBufferCreateInfo);
    case VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT:
      return sizeof(VkBufferDeviceAddressCreateInfoEXT);
    case VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO:
      return sizeof(VkBufferDeviceAddressInfo);
    case VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2:
      return sizeof(VkBufferImageCopy2);
    case VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER:
      return sizeof(VkBufferMemoryBarrier);
    case VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2:
      return sizeof(VkBufferMemoryBarrier2);
    case VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2:
      return sizeof(VkBufferMemoryRequirementsInfo2);
    case VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO:
      return sizeof(VkBufferOpaqueCaptureAddressCreateInfo);
    case VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO:
      return sizeof(VkBufferUsageFlags2CreateInfo);
    case VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO:
      return sizeof(VkBufferViewCreateInfo);
    case VK_STRUCTURE_TYPE_BUILD_PARTITIONED_ACCELERATION_STRUCTURE_INFO_NV:
      return sizeof(VkBuildPartitionedAccelerationStructureInfoNV);
    case VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR:
      return sizeof(VkCalibratedTimestampInfoKHR);
    case VK_STRUCTURE_TYPE_CHECKPOINT_DATA_2_NV:
      return sizeof(VkCheckpointData2NV);
    case VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV:
      return sizeof(VkCheckpointDataNV);
    case VK_STRUCTURE_TYPE_CLUSTER_ACCELERATION_STRUCTURE_CLUSTERS_BOTTOM_LEVEL_INPUT_NV:
      return sizeof(VkClusterAccelerationStructureClustersBottomLevelInputNV);
    case VK_STRUCTURE_TYPE_CLUSTER_ACCELERATION_STRUCTURE_COMMANDS_INFO_NV:
      return sizeof(VkClusterAccelerationStructureCommandsInfoNV);
    case VK_STRUCTURE_TYPE_CLUSTER_ACCELERATION_STRUCTURE_INPUT_INFO_NV:
      return sizeof(VkClusterAccelerationStructureInputInfoNV);
    case VK_STRUCTURE_TYPE_CLUSTER_ACCELERATION_STRUCTURE_MOVE_OBJECTS_INPUT_NV:
      return sizeof(VkClusterAccelerationStructureMoveObjectsInputNV);
    case VK_STRUCTURE_TYPE_CLUSTER_ACCELERATION_STRUCTURE_TRIANGLE_CLUSTER_INPUT_NV:
      return sizeof(VkClusterAccelerationStructureTriangleClusterInputNV);
    case VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO:
      return sizeof(VkCommandBufferAllocateInfo);
    case VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO:
      return sizeof(VkCommandBufferBeginInfo);
    case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT:
      return sizeof(VkCommandBufferInheritanceConditionalRenderingInfoEXT);
    case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_DESCRIPTOR_HEAP_INFO_EXT:
      return sizeof(VkCommandBufferInheritanceDescriptorHeapInfoEXT);
    case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO:
      return sizeof(VkCommandBufferInheritanceInfo);
    case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM:
      return sizeof(VkCommandBufferInheritanceRenderPassTransformInfoQCOM);
    case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO:
      return sizeof(VkCommandBufferInheritanceRenderingInfo);
    case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_VIEWPORT_SCISSOR_INFO_NV:
      return sizeof(VkCommandBufferInheritanceViewportScissorInfoNV);
    case VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO:
      return sizeof(VkCommandBufferSubmitInfo);
    case VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO:
      return sizeof(VkCommandPoolCreateInfo);
    case VK_STRUCTURE_TYPE_COMPUTE_OCCUPANCY_PRIORITY_PARAMETERS_NV:
      return sizeof(VkComputeOccupancyPriorityParametersNV);
    case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO:
      return sizeof(VkComputePipelineCreateInfo);
    case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_INDIRECT_BUFFER_INFO_NV:
      return sizeof(VkComputePipelineIndirectBufferInfoNV);
    case VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_2_EXT:
      return sizeof(VkConditionalRenderingBeginInfo2EXT);
    case VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT:
      return sizeof(VkConditionalRenderingBeginInfoEXT);
    case VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV:
      return sizeof(VkConvertCooperativeVectorMatrixInfoNV);
    case VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_FLEXIBLE_DIMENSIONS_PROPERTIES_NV:
      return sizeof(VkCooperativeMatrixFlexibleDimensionsPropertiesNV);
    case VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR:
      return sizeof(VkCooperativeMatrixPropertiesKHR);
    case VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_NV:
      return sizeof(VkCooperativeMatrixPropertiesNV);
    case VK_STRUCTURE_TYPE_COOPERATIVE_VECTOR_PROPERTIES_NV:
      return sizeof(VkCooperativeVectorPropertiesNV);
    case VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR:
      return sizeof(VkCopyAccelerationStructureInfoKHR);
    case VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR:
      return sizeof(VkCopyAccelerationStructureToMemoryInfoKHR);
    case VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2:
      return sizeof(VkCopyBufferInfo2);
    case VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2:
      return sizeof(VkCopyBufferToImageInfo2);
    case VK_STRUCTURE_TYPE_COPY_COMMAND_TRANSFORM_INFO_QCOM:
      return sizeof(VkCopyCommandTransformInfoQCOM);
    case VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET:
      return sizeof(VkCopyDescriptorSet);
    case VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_IMAGE_INFO_KHR:
      return sizeof(VkCopyDeviceMemoryImageInfoKHR);
    case VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_INFO_KHR:
      return sizeof(VkCopyDeviceMemoryInfoKHR);
    case VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2:
      return sizeof(VkCopyImageInfo2);
    case VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2:
      return sizeof(VkCopyImageToBufferInfo2);
    case VK_STRUCTURE_TYPE_COPY_IMAGE_TO_IMAGE_INFO:
      return sizeof(VkCopyImageToImageInfo);
    case VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO:
      return sizeof(VkCopyImageToMemoryInfo);
    case VK_STRUCTURE_TYPE_COPY_MEMORY_INDIRECT_INFO_KHR:
      return sizeof(VkCopyMemoryIndirectInfoKHR);
    case VK_STRUCTURE_TYPE_COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR:
      return sizeof(VkCopyMemoryToAccelerationStructureInfoKHR);
    case VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INDIRECT_INFO_KHR:
      return sizeof(VkCopyMemoryToImageIndirectInfoKHR);
    case VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO:
      return sizeof(VkCopyMemoryToImageInfo);
    case VK_STRUCTURE_TYPE_COPY_MEMORY_TO_MICROMAP_INFO_EXT:
      return sizeof(VkCopyMemoryToMicromapInfoEXT);
    case VK_STRUCTURE_TYPE_COPY_MICROMAP_INFO_EXT:
      return sizeof(VkCopyMicromapInfoEXT);
    case VK_STRUCTURE_TYPE_COPY_MICROMAP_TO_MEMORY_INFO_EXT:
      return sizeof(VkCopyMicromapToMemoryInfoEXT);
    case VK_STRUCTURE_TYPE_COPY_TENSOR_INFO_ARM:
      return sizeof(VkCopyTensorInfoARM);
    case VK_STRUCTURE_TYPE_CU_FUNCTION_CREATE_INFO_NVX:
      return sizeof(VkCuFunctionCreateInfoNVX);
    case VK_STRUCTURE_TYPE_CU_LAUNCH_INFO_NVX:
      return sizeof(VkCuLaunchInfoNVX);
    case VK_STRUCTURE_TYPE_CU_MODULE_CREATE_INFO_NVX:
      return sizeof(VkCuModuleCreateInfoNVX);
    case VK_STRUCTURE_TYPE_CU_MODULE_TEXTURING_MODE_CREATE_INFO_NVX:
      return sizeof(VkCuModuleTexturingModeCreateInfoNVX);
    case VK_STRUCTURE_TYPE_CUDA_FUNCTION_CREATE_INFO_NV:
      return sizeof(VkCudaFunctionCreateInfoNV);
    case VK_STRUCTURE_TYPE_CUDA_LAUNCH_INFO_NV:
      return sizeof(VkCudaLaunchInfoNV);
    case VK_STRUCTURE_TYPE_CUDA_MODULE_CREATE_INFO_NV:
      return sizeof(VkCudaModuleCreateInfoNV);
    case VK_STRUCTURE_TYPE_CUSTOM_RESOLVE_CREATE_INFO_EXT:
      return sizeof(VkCustomResolveCreateInfoEXT);
    case VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR:
      return sizeof(VkD3D12FenceSubmitInfoKHR);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_OPTICAL_FLOW_IMAGE_FORMAT_INFO_ARM:
      return sizeof(VkDataGraphOpticalFlowImageFormatInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_OPTICAL_FLOW_IMAGE_FORMAT_PROPERTIES_ARM:
      return sizeof(VkDataGraphOpticalFlowImageFormatPropertiesARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_BUILTIN_MODEL_CREATE_INFO_QCOM:
      return sizeof(VkDataGraphPipelineBuiltinModelCreateInfoQCOM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_COMPILER_CONTROL_CREATE_INFO_ARM:
      return sizeof(VkDataGraphPipelineCompilerControlCreateInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_CONSTANT_ARM:
      return sizeof(VkDataGraphPipelineConstantARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_CONSTANT_TENSOR_SEMI_STRUCTURED_SPARSITY_INFO_ARM:
      return sizeof(VkDataGraphPipelineConstantTensorSemiStructuredSparsityInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_CREATE_INFO_ARM:
      return sizeof(VkDataGraphPipelineCreateInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_DISPATCH_INFO_ARM:
      return sizeof(VkDataGraphPipelineDispatchInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_IDENTIFIER_CREATE_INFO_ARM:
      return sizeof(VkDataGraphPipelineIdentifierCreateInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_INFO_ARM:
      return sizeof(VkDataGraphPipelineInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_NEURAL_STATISTICS_CREATE_INFO_ARM:
      return sizeof(VkDataGraphPipelineNeuralStatisticsCreateInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_OPTICAL_FLOW_CREATE_INFO_ARM:
      return sizeof(VkDataGraphPipelineOpticalFlowCreateInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_OPTICAL_FLOW_DISPATCH_INFO_ARM:
      return sizeof(VkDataGraphPipelineOpticalFlowDispatchInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_PROPERTY_QUERY_RESULT_ARM:
      return sizeof(VkDataGraphPipelinePropertyQueryResultARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_RESOURCE_INFO_ARM:
      return sizeof(VkDataGraphPipelineResourceInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_RESOURCE_INFO_IMAGE_LAYOUT_ARM:
      return sizeof(VkDataGraphPipelineResourceInfoImageLayoutARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_REQUIREMENT_ARM:
      return sizeof(VkDataGraphPipelineSessionBindPointRequirementARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_REQUIREMENTS_INFO_ARM:
      return sizeof(VkDataGraphPipelineSessionBindPointRequirementsInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SESSION_CREATE_INFO_ARM:
      return sizeof(VkDataGraphPipelineSessionCreateInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SESSION_MEMORY_REQUIREMENTS_INFO_ARM:
      return sizeof(VkDataGraphPipelineSessionMemoryRequirementsInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SESSION_NEURAL_STATISTICS_CREATE_INFO_ARM:
      return sizeof(VkDataGraphPipelineSessionNeuralStatisticsCreateInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SHADER_MODULE_CREATE_INFO_ARM:
      return sizeof(VkDataGraphPipelineShaderModuleCreateInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SINGLE_NODE_CONNECTION_ARM:
      return sizeof(VkDataGraphPipelineSingleNodeConnectionARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SINGLE_NODE_CREATE_INFO_ARM:
      return sizeof(VkDataGraphPipelineSingleNodeCreateInfoARM);
    case VK_STRUCTURE_TYPE_DATA_GRAPH_PROCESSING_ENGINE_CREATE_INFO_ARM:
      return sizeof(VkDataGraphProcessingEngineCreateInfoARM);
    case VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT:
      return sizeof(VkDebugMarkerMarkerInfoEXT);
    case VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT:
      return sizeof(VkDebugMarkerObjectNameInfoEXT);
    case VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT:
      return sizeof(VkDebugMarkerObjectTagInfoEXT);
    case VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT:
      return sizeof(VkDebugReportCallbackCreateInfoEXT);
    case VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT:
      return sizeof(VkDebugUtilsLabelEXT);
    case VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT:
      return sizeof(VkDebugUtilsMessengerCallbackDataEXT);
    case VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT:
      return sizeof(VkDebugUtilsMessengerCreateInfoEXT);
    case VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT:
      return sizeof(VkDebugUtilsObjectNameInfoEXT);
    case VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT:
      return sizeof(VkDebugUtilsObjectTagInfoEXT);
    case VK_STRUCTURE_TYPE_DECOMPRESS_MEMORY_INFO_EXT:
      return sizeof(VkDecompressMemoryInfoEXT);
    case VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV:
      return sizeof(VkDedicatedAllocationBufferCreateInfoNV);
    case VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV:
      return sizeof(VkDedicatedAllocationImageCreateInfoNV);
    case VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV:
      return sizeof(VkDedicatedAllocationMemoryAllocateInfoNV);
    case VK_STRUCTURE_TYPE_DEPENDENCY_INFO:
      return sizeof(VkDependencyInfo);
    case VK_STRUCTURE_TYPE_DEPTH_BIAS_INFO_EXT:
      return sizeof(VkDepthBiasInfoEXT);
    case VK_STRUCTURE_TYPE_DEPTH_BIAS_REPRESENTATION_INFO_EXT:
      return sizeof(VkDepthBiasRepresentationInfoEXT);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT:
      return sizeof(VkDescriptorAddressInfoEXT);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT:
      return sizeof(VkDescriptorBufferBindingInfoEXT);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_PUSH_DESCRIPTOR_BUFFER_HANDLE_EXT:
      return sizeof(VkDescriptorBufferBindingPushDescriptorBufferHandleEXT);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT:
      return sizeof(VkDescriptorGetInfoEXT);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_GET_TENSOR_INFO_ARM:
      return sizeof(VkDescriptorGetTensorInfoARM);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO:
      return sizeof(VkDescriptorPoolCreateInfo);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO:
      return sizeof(VkDescriptorPoolInlineUniformBlockCreateInfo);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO:
      return sizeof(VkDescriptorSetAllocateInfo);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT:
      return sizeof(VkDescriptorSetAndBindingMappingEXT);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_BINDING_REFERENCE_VALVE:
      return sizeof(VkDescriptorSetBindingReferenceVALVE);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
      return sizeof(VkDescriptorSetLayoutBindingFlagsCreateInfo);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO:
      return sizeof(VkDescriptorSetLayoutCreateInfo);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_HOST_MAPPING_INFO_VALVE:
      return sizeof(VkDescriptorSetLayoutHostMappingInfoVALVE);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT:
      return sizeof(VkDescriptorSetLayoutSupport);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO:
      return sizeof(VkDescriptorSetVariableDescriptorCountAllocateInfo);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT:
      return sizeof(VkDescriptorSetVariableDescriptorCountLayoutSupport);
    case VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO:
      return sizeof(VkDescriptorUpdateTemplateCreateInfo);
    case VK_STRUCTURE_TYPE_DEVICE_ADDRESS_BINDING_CALLBACK_DATA_EXT:
      return sizeof(VkDeviceAddressBindingCallbackDataEXT);
    case VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS:
      return sizeof(VkDeviceBufferMemoryRequirements);
    case VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO:
      return sizeof(VkDeviceCreateInfo);
    case VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT:
      return sizeof(VkDeviceDeviceMemoryReportCreateInfoEXT);
    case VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV:
      return sizeof(VkDeviceDiagnosticsConfigCreateInfoNV);
    case VK_STRUCTURE_TYPE_DEVICE_EVENT_INFO_EXT:
      return sizeof(VkDeviceEventInfoEXT);
    case VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT:
      return sizeof(VkDeviceFaultCountsEXT);
    case VK_STRUCTURE_TYPE_DEVICE_FAULT_DEBUG_INFO_KHR:
      return sizeof(VkDeviceFaultDebugInfoKHR);
    case VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT:
      return sizeof(VkDeviceFaultInfoEXT);
    case VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_KHR:
      return sizeof(VkDeviceFaultInfoKHR);
    case VK_STRUCTURE_TYPE_DEVICE_FAULT_SHADER_ABORT_MESSAGE_INFO_KHR:
      return sizeof(VkDeviceFaultShaderAbortMessageInfoKHR);
    case VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO:
      return sizeof(VkDeviceGroupBindSparseInfo);
    case VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO:
      return sizeof(VkDeviceGroupCommandBufferBeginInfo);
    case VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO:
      return sizeof(VkDeviceGroupDeviceCreateInfo);
    case VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR:
      return sizeof(VkDeviceGroupPresentCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR:
      return sizeof(VkDeviceGroupPresentInfoKHR);
    case VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO:
      return sizeof(VkDeviceGroupRenderPassBeginInfo);
    case VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO:
      return sizeof(VkDeviceGroupSubmitInfo);
    case VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR:
      return sizeof(VkDeviceGroupSwapchainCreateInfoKHR);
    case VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS:
      return sizeof(VkDeviceImageMemoryRequirements);
    case VK_STRUCTURE_TYPE_DEVICE_IMAGE_SUBRESOURCE_INFO:
      return sizeof(VkDeviceImageSubresourceInfo);
    case VK_STRUCTURE_TYPE_DEVICE_MEMORY_COPY_KHR:
      return sizeof(VkDeviceMemoryCopyKHR);
    case VK_STRUCTURE_TYPE_DEVICE_MEMORY_IMAGE_COPY_KHR:
      return sizeof(VkDeviceMemoryImageCopyKHR);
    case VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO:
      return sizeof(VkDeviceMemoryOpaqueCaptureAddressInfo);
    case VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD:
      return sizeof(VkDeviceMemoryOverallocationCreateInfoAMD);
    case VK_STRUCTURE_TYPE_DEVICE_MEMORY_REPORT_CALLBACK_DATA_EXT:
      return sizeof(VkDeviceMemoryReportCallbackDataEXT);
    case VK_STRUCTURE_TYPE_DEVICE_PIPELINE_BINARY_INTERNAL_CACHE_CONTROL_KHR:
      return sizeof(VkDevicePipelineBinaryInternalCacheControlKHR);
    case VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO:
      return sizeof(VkDevicePrivateDataCreateInfo);
    case VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO:
      return sizeof(VkDeviceQueueCreateInfo);
    case VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO:
      return sizeof(VkDeviceQueueGlobalPriorityCreateInfo);
    case VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2:
      return sizeof(VkDeviceQueueInfo2);
    case VK_STRUCTURE_TYPE_DEVICE_QUEUE_SHADER_CORE_CONTROL_CREATE_INFO_ARM:
      return sizeof(VkDeviceQueueShaderCoreControlCreateInfoARM);
    case VK_STRUCTURE_TYPE_DEVICE_TENSOR_MEMORY_REQUIREMENTS_ARM:
      return sizeof(VkDeviceTensorMemoryRequirementsARM);
    case VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_INFO_LUNARG:
      return sizeof(VkDirectDriverLoadingInfoLUNARG);
    case VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_LIST_LUNARG:
      return sizeof(VkDirectDriverLoadingListLUNARG);
    case VK_STRUCTURE_TYPE_DISPATCH_INDIRECT_2_INFO_KHR:
      return sizeof(VkDispatchIndirect2InfoKHR);
    case VK_STRUCTURE_TYPE_DISPATCH_PARAMETERS_ARM:
      return sizeof(VkDispatchParametersARM);
    case VK_STRUCTURE_TYPE_DISPATCH_TILE_INFO_QCOM:
      return sizeof(VkDispatchTileInfoQCOM);
    case VK_STRUCTURE_TYPE_DISPLAY_EVENT_INFO_EXT:
      return sizeof(VkDisplayEventInfoEXT);
    case VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR:
      return sizeof(VkDisplayModeCreateInfoKHR);
    case VK_STRUCTURE_TYPE_DISPLAY_MODE_PROPERTIES_2_KHR:
      return sizeof(VkDisplayModeProperties2KHR);
    case VK_STRUCTURE_TYPE_DISPLAY_MODE_STEREO_PROPERTIES_NV:
      return sizeof(VkDisplayModeStereoPropertiesNV);
    case VK_STRUCTURE_TYPE_DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD:
      return sizeof(VkDisplayNativeHdrSurfaceCapabilitiesAMD);
    case VK_STRUCTURE_TYPE_DISPLAY_PLANE_CAPABILITIES_2_KHR:
      return sizeof(VkDisplayPlaneCapabilities2KHR);
    case VK_STRUCTURE_TYPE_DISPLAY_PLANE_INFO_2_KHR:
      return sizeof(VkDisplayPlaneInfo2KHR);
    case VK_STRUCTURE_TYPE_DISPLAY_PLANE_PROPERTIES_2_KHR:
      return sizeof(VkDisplayPlaneProperties2KHR);
    case VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT:
      return sizeof(VkDisplayPowerInfoEXT);
    case VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR:
      return sizeof(VkDisplayPresentInfoKHR);
    case VK_STRUCTURE_TYPE_DISPLAY_PROPERTIES_2_KHR:
      return sizeof(VkDisplayProperties2KHR);
    case VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR:
      return sizeof(VkDisplaySurfaceCreateInfoKHR);
    case VK_STRUCTURE_TYPE_DISPLAY_SURFACE_STEREO_CREATE_INFO_NV:
      return sizeof(VkDisplaySurfaceStereoCreateInfoNV);
    case VK_STRUCTURE_TYPE_DRAW_INDIRECT_2_INFO_KHR:
      return sizeof(VkDrawIndirect2InfoKHR);
    case VK_STRUCTURE_TYPE_DRAW_INDIRECT_COUNT_2_INFO_KHR:
      return sizeof(VkDrawIndirectCount2InfoKHR);
    case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT:
      return sizeof(VkDrmFormatModifierPropertiesList2EXT);
    case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT:
      return sizeof(VkDrmFormatModifierPropertiesListEXT);
    case VK_STRUCTURE_TYPE_EVENT_CREATE_INFO:
      return sizeof(VkEventCreateInfo);
    case VK_STRUCTURE_TYPE_EXECUTION_GRAPH_PIPELINE_CREATE_INFO_AMDX:
      return sizeof(VkExecutionGraphPipelineCreateInfoAMDX);
    case VK_STRUCTURE_TYPE_EXECUTION_GRAPH_PIPELINE_SCRATCH_SIZE_AMDX:
      return sizeof(VkExecutionGraphPipelineScratchSizeAMDX);
    case VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO:
      return sizeof(VkExportFenceCreateInfo);
    case VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR:
      return sizeof(VkExportFenceWin32HandleInfoKHR);
    case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO:
      return sizeof(VkExportMemoryAllocateInfo);
    case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV:
      return sizeof(VkExportMemoryAllocateInfoNV);
    case VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
      return sizeof(VkExportMemoryWin32HandleInfoKHR);
    case VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV:
      return sizeof(VkExportMemoryWin32HandleInfoNV);
    case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO:
      return sizeof(VkExportSemaphoreCreateInfo);
    case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
      return sizeof(VkExportSemaphoreWin32HandleInfoKHR);
    case VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES:
      return sizeof(VkExternalBufferProperties);
    case VK_STRUCTURE_TYPE_EXTERNAL_COMPUTE_QUEUE_CREATE_INFO_NV:
      return sizeof(VkExternalComputeQueueCreateInfoNV);
    case VK_STRUCTURE_TYPE_EXTERNAL_COMPUTE_QUEUE_DATA_PARAMS_NV:
      return sizeof(VkExternalComputeQueueDataParamsNV);
    case VK_STRUCTURE_TYPE_EXTERNAL_COMPUTE_QUEUE_DEVICE_CREATE_INFO_NV:
      return sizeof(VkExternalComputeQueueDeviceCreateInfoNV);
    case VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES:
      return sizeof(VkExternalFenceProperties);
    case VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID:
      return sizeof(VkExternalFormatANDROID);
    case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES:
      return sizeof(VkExternalImageFormatProperties);
    case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_ACQUIRE_UNMODIFIED_EXT:
      return sizeof(VkExternalMemoryAcquireUnmodifiedEXT);
    case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO:
      return sizeof(VkExternalMemoryBufferCreateInfo);
    case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO:
      return sizeof(VkExternalMemoryImageCreateInfo);
    case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV:
      return sizeof(VkExternalMemoryImageCreateInfoNV);
    case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_TENSOR_CREATE_INFO_ARM:
      return sizeof(VkExternalMemoryTensorCreateInfoARM);
    case VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES:
      return sizeof(VkExternalSemaphoreProperties);
    case VK_STRUCTURE_TYPE_EXTERNAL_TENSOR_PROPERTIES_ARM:
      return sizeof(VkExternalTensorPropertiesARM);
    case VK_STRUCTURE_TYPE_FENCE_CREATE_INFO:
      return sizeof(VkFenceCreateInfo);
    case VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR:
      return sizeof(VkFenceGetFdInfoKHR);
    case VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR:
      return sizeof(VkFenceGetWin32HandleInfoKHR);
    case VK_STRUCTURE_TYPE_FILTER_CUBIC_IMAGE_VIEW_IMAGE_FORMAT_PROPERTIES_EXT:
      return sizeof(VkFilterCubicImageViewImageFormatPropertiesEXT);
    case VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2:
      return sizeof(VkFormatProperties2);
    case VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3:
      return sizeof(VkFormatProperties3);
    case VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_4_KHR:
      return sizeof(VkFormatProperties4KHR);
    case VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR:
      return sizeof(VkFragmentShadingRateAttachmentInfoKHR);
    case VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT:
      return sizeof(VkFrameBoundaryEXT);
    case VK_STRUCTURE_TYPE_FRAME_BOUNDARY_TENSORS_ARM:
      return sizeof(VkFrameBoundaryTensorsARM);
    case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO:
      return sizeof(VkFramebufferAttachmentImageInfo);
    case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO:
      return sizeof(VkFramebufferAttachmentsCreateInfo);
    case VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO:
      return sizeof(VkFramebufferCreateInfo);
    case VK_STRUCTURE_TYPE_FRAMEBUFFER_MIXED_SAMPLES_COMBINATION_NV:
      return sizeof(VkFramebufferMixedSamplesCombinationNV);
    case VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_EXT:
      return sizeof(VkGeneratedCommandsInfoEXT);
    case VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV:
      return sizeof(VkGeneratedCommandsInfoNV);
    case VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_EXT:
      return sizeof(VkGeneratedCommandsMemoryRequirementsInfoEXT);
    case VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV:
      return sizeof(VkGeneratedCommandsMemoryRequirementsInfoNV);
    case VK_STRUCTURE_TYPE_GENERATED_COMMANDS_PIPELINE_INFO_EXT:
      return sizeof(VkGeneratedCommandsPipelineInfoEXT);
    case VK_STRUCTURE_TYPE_GENERATED_COMMANDS_SHADER_INFO_EXT:
      return sizeof(VkGeneratedCommandsShaderInfoEXT);
    case VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV:
      return sizeof(VkGeometryAABBNV);
    case VK_STRUCTURE_TYPE_GEOMETRY_NV:
      return sizeof(VkGeometryNV);
    case VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV:
      return sizeof(VkGeometryTrianglesNV);
    case VK_STRUCTURE_TYPE_GET_LATENCY_MARKER_INFO_NV:
      return sizeof(VkGetLatencyMarkerInfoNV);
    case VK_STRUCTURE_TYPE_GPA_DEVICE_CLOCK_MODE_INFO_AMD:
      return sizeof(VkGpaDeviceClockModeInfoAMD);
    case VK_STRUCTURE_TYPE_GPA_DEVICE_GET_CLOCK_INFO_AMD:
      return sizeof(VkGpaDeviceGetClockInfoAMD);
    case VK_STRUCTURE_TYPE_GPA_SAMPLE_BEGIN_INFO_AMD:
      return sizeof(VkGpaSampleBeginInfoAMD);
    case VK_STRUCTURE_TYPE_GPA_SESSION_CREATE_INFO_AMD:
      return sizeof(VkGpaSessionCreateInfoAMD);
    case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO:
      return sizeof(VkGraphicsPipelineCreateInfo);
    case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT:
      return sizeof(VkGraphicsPipelineLibraryCreateInfoEXT);
    case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV:
      return sizeof(VkGraphicsPipelineShaderGroupsCreateInfoNV);
    case VK_STRUCTURE_TYPE_GRAPHICS_SHADER_GROUP_CREATE_INFO_NV:
      return sizeof(VkGraphicsShaderGroupCreateInfoNV);
    case VK_STRUCTURE_TYPE_HDR_METADATA_EXT:
      return sizeof(VkHdrMetadataEXT);
    case VK_STRUCTURE_TYPE_HDR_VIVID_DYNAMIC_METADATA_HUAWEI:
      return sizeof(VkHdrVividDynamicMetadataHUAWEI);
    case VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT:
      return sizeof(VkHeadlessSurfaceCreateInfoEXT);
    case VK_STRUCTURE_TYPE_HOST_IMAGE_COPY_DEVICE_PERFORMANCE_QUERY:
      return sizeof(VkHostImageCopyDevicePerformanceQuery);
    case VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO:
      return sizeof(VkHostImageLayoutTransitionInfo);
    case VK_STRUCTURE_TYPE_IMAGE_ALIGNMENT_CONTROL_CREATE_INFO_MESA:
      return sizeof(VkImageAlignmentControlCreateInfoMESA);
    case VK_STRUCTURE_TYPE_IMAGE_BLIT_2:
      return sizeof(VkImageBlit2);
    case VK_STRUCTURE_TYPE_IMAGE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:
      return sizeof(VkImageCaptureDescriptorDataInfoEXT);
    case VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT:
      return sizeof(VkImageCompressionControlEXT);
    case VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT:
      return sizeof(VkImageCompressionPropertiesEXT);
    case VK_STRUCTURE_TYPE_IMAGE_COPY_2:
      return sizeof(VkImageCopy2);
    case VK_STRUCTURE_TYPE_IMAGE_CREATE_FLAGS_2_CREATE_INFO_KHR:
      return sizeof(VkImageCreateFlags2CreateInfoKHR);
    case VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO:
      return sizeof(VkImageCreateInfo);
    case VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT:
      return sizeof(VkImageDescriptorInfoEXT);
    case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT:
      return sizeof(VkImageDrmFormatModifierExplicitCreateInfoEXT);
    case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT:
      return sizeof(VkImageDrmFormatModifierListCreateInfoEXT);
    case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT:
      return sizeof(VkImageDrmFormatModifierPropertiesEXT);
    case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO:
      return sizeof(VkImageFormatListCreateInfo);
    case VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2:
      return sizeof(VkImageFormatProperties2);
    case VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER:
      return sizeof(VkImageMemoryBarrier);
    case VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2:
      return sizeof(VkImageMemoryBarrier2);
    case VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2:
      return sizeof(VkImageMemoryRequirementsInfo2);
    case VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO:
      return sizeof(VkImagePlaneMemoryRequirementsInfo);
    case VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2:
      return sizeof(VkImageResolve2);
    case VK_STRUCTURE_TYPE_IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2:
      return sizeof(VkImageSparseMemoryRequirementsInfo2);
    case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_2_CREATE_INFO_KHR:
      return sizeof(VkImageStencilUsage2CreateInfoKHR);
    case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO:
      return sizeof(VkImageStencilUsageCreateInfo);
    case VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2:
      return sizeof(VkImageSubresource2);
    case VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR:
      return sizeof(VkImageSwapchainCreateInfoKHR);
    case VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY:
      return sizeof(VkImageToMemoryCopy);
    case VK_STRUCTURE_TYPE_IMAGE_USAGE_FLAGS_2_CREATE_INFO_KHR:
      return sizeof(VkImageUsageFlags2CreateInfoKHR);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT:
      return sizeof(VkImageViewASTCDecodeModeEXT);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_ADDRESS_PROPERTIES_NVX:
      return sizeof(VkImageViewAddressPropertiesNVX);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:
      return sizeof(VkImageViewCaptureDescriptorDataInfoEXT);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO:
      return sizeof(VkImageViewCreateInfo);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_HANDLE_INFO_NVX:
      return sizeof(VkImageViewHandleInfoNVX);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT:
      return sizeof(VkImageViewMinLodCreateInfoEXT);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_SAMPLE_WEIGHT_CREATE_INFO_QCOM:
      return sizeof(VkImageViewSampleWeightCreateInfoQCOM);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_SLICED_CREATE_INFO_EXT:
      return sizeof(VkImageViewSlicedCreateInfoEXT);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_2_CREATE_INFO_KHR:
      return sizeof(VkImageViewUsage2CreateInfoKHR);
    case VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO:
      return sizeof(VkImageViewUsageCreateInfo);
    case VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
      return sizeof(VkImportAndroidHardwareBufferInfoANDROID);
    case VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR:
      return sizeof(VkImportFenceFdInfoKHR);
    case VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR:
      return sizeof(VkImportFenceWin32HandleInfoKHR);
    case VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR:
      return sizeof(VkImportMemoryFdInfoKHR);
    case VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT:
      return sizeof(VkImportMemoryHostPointerInfoEXT);
    case VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
      return sizeof(VkImportMemoryWin32HandleInfoKHR);
    case VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV:
      return sizeof(VkImportMemoryWin32HandleInfoNV);
    case VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR:
      return sizeof(VkImportSemaphoreFdInfoKHR);
    case VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
      return sizeof(VkImportSemaphoreWin32HandleInfoKHR);
    case VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_EXT:
      return sizeof(VkIndirectCommandsLayoutCreateInfoEXT);
    case VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV:
      return sizeof(VkIndirectCommandsLayoutCreateInfoNV);
    case VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_PUSH_DATA_TOKEN_NV:
      return sizeof(VkIndirectCommandsLayoutPushDataTokenNV);
    case VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT:
      return sizeof(VkIndirectCommandsLayoutTokenEXT);
    case VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV:
      return sizeof(VkIndirectCommandsLayoutTokenNV);
    case VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_CREATE_INFO_EXT:
      return sizeof(VkIndirectExecutionSetCreateInfoEXT);
    case VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_PIPELINE_INFO_EXT:
      return sizeof(VkIndirectExecutionSetPipelineInfoEXT);
    case VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_SHADER_INFO_EXT:
      return sizeof(VkIndirectExecutionSetShaderInfoEXT);
    case VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_SHADER_LAYOUT_INFO_EXT:
      return sizeof(VkIndirectExecutionSetShaderLayoutInfoEXT);
    case VK_STRUCTURE_TYPE_INITIALIZE_PERFORMANCE_API_INFO_INTEL:
      return sizeof(VkInitializePerformanceApiInfoINTEL);
    case VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO:
      return sizeof(VkInstanceCreateInfo);
    case VK_STRUCTURE_TYPE_LATENCY_SLEEP_INFO_NV:
      return sizeof(VkLatencySleepInfoNV);
    case VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV:
      return sizeof(VkLatencySleepModeInfoNV);
    case VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV:
      return sizeof(VkLatencySubmissionPresentIdNV);
    case VK_STRUCTURE_TYPE_LATENCY_SURFACE_CAPABILITIES_NV:
      return sizeof(VkLatencySurfaceCapabilitiesNV);
    case VK_STRUCTURE_TYPE_LATENCY_TIMINGS_FRAME_REPORT_NV:
      return sizeof(VkLatencyTimingsFrameReportNV);
    case VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT:
      return sizeof(VkLayerSettingsCreateInfoEXT);
    case VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE:
      return sizeof(VkMappedMemoryRange);
    case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO:
      return sizeof(VkMemoryAllocateFlagsInfo);
    case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO:
      return sizeof(VkMemoryAllocateInfo);
    case VK_STRUCTURE_TYPE_MEMORY_BARRIER:
      return sizeof(VkMemoryBarrier);
    case VK_STRUCTURE_TYPE_MEMORY_BARRIER_2:
      return sizeof(VkMemoryBarrier2);
    case VK_STRUCTURE_TYPE_MEMORY_BARRIER_ACCESS_FLAGS_3_KHR:
      return sizeof(VkMemoryBarrierAccessFlags3KHR);
    case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO:
      return sizeof(VkMemoryDedicatedAllocateInfo);
    case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_TENSOR_ARM:
      return sizeof(VkMemoryDedicatedAllocateInfoTensorARM);
    case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS:
      return sizeof(VkMemoryDedicatedRequirements);
    case VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR:
      return sizeof(VkMemoryFdPropertiesKHR);
    case VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
      return sizeof(VkMemoryGetAndroidHardwareBufferInfoANDROID);
    case VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR:
      return sizeof(VkMemoryGetFdInfoKHR);
    case VK_STRUCTURE_TYPE_MEMORY_GET_REMOTE_ADDRESS_INFO_NV:
      return sizeof(VkMemoryGetRemoteAddressInfoNV);
    case VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR:
      return sizeof(VkMemoryGetWin32HandleInfoKHR);
    case VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT:
      return sizeof(VkMemoryHostPointerPropertiesEXT);
    case VK_STRUCTURE_TYPE_MEMORY_MAP_INFO:
      return sizeof(VkMemoryMapInfo);
    case VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT:
      return sizeof(VkMemoryMapPlacedInfoEXT);
    case VK_STRUCTURE_TYPE_MEMORY_MARKER_INFO_AMD:
      return sizeof(VkMemoryMarkerInfoAMD);
    case VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO:
      return sizeof(VkMemoryOpaqueCaptureAddressAllocateInfo);
    case VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT:
      return sizeof(VkMemoryPriorityAllocateInfoEXT);
    case VK_STRUCTURE_TYPE_MEMORY_RANGE_BARRIER_KHR:
      return sizeof(VkMemoryRangeBarrierKHR);
    case VK_STRUCTURE_TYPE_MEMORY_RANGE_BARRIERS_INFO_KHR:
      return sizeof(VkMemoryRangeBarriersInfoKHR);
    case VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2:
      return sizeof(VkMemoryRequirements2);
    case VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY:
      return sizeof(VkMemoryToImageCopy);
    case VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO:
      return sizeof(VkMemoryUnmapInfo);
    case VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR:
      return sizeof(VkMemoryWin32HandlePropertiesKHR);
    case VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT:
      return sizeof(VkMicromapBuildInfoEXT);
    case VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT:
      return sizeof(VkMicromapBuildSizesInfoEXT);
    case VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT:
      return sizeof(VkMicromapCreateInfoEXT);
    case VK_STRUCTURE_TYPE_MICROMAP_VERSION_INFO_EXT:
      return sizeof(VkMicromapVersionInfoEXT);
    case VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT:
      return sizeof(VkMultisamplePropertiesEXT);
    case VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT:
      return sizeof(VkMultisampledRenderToSingleSampledInfoEXT);
    case VK_STRUCTURE_TYPE_MULTIVIEW_PER_VIEW_ATTRIBUTES_INFO_NVX:
      return sizeof(VkMultiviewPerViewAttributesInfoNVX);
    case VK_STRUCTURE_TYPE_MULTIVIEW_PER_VIEW_RENDER_AREAS_RENDER_PASS_BEGIN_INFO_QCOM:
      return sizeof(VkMultiviewPerViewRenderAreasRenderPassBeginInfoQCOM);
    case VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT:
      return sizeof(VkMutableDescriptorTypeCreateInfoEXT);
    case VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DATA_CREATE_INFO_EXT:
      return sizeof(VkOpaqueCaptureDataCreateInfoEXT);
    case VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DESCRIPTOR_DATA_CREATE_INFO_EXT:
      return sizeof(VkOpaqueCaptureDescriptorDataCreateInfoEXT);
    case VK_STRUCTURE_TYPE_OPTICAL_FLOW_EXECUTE_INFO_NV:
      return sizeof(VkOpticalFlowExecuteInfoNV);
    case VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_INFO_NV:
      return sizeof(VkOpticalFlowImageFormatInfoNV);
    case VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_PROPERTIES_NV:
      return sizeof(VkOpticalFlowImageFormatPropertiesNV);
    case VK_STRUCTURE_TYPE_OPTICAL_FLOW_SESSION_CREATE_INFO_NV:
      return sizeof(VkOpticalFlowSessionCreateInfoNV);
    case VK_STRUCTURE_TYPE_OPTICAL_FLOW_SESSION_CREATE_PRIVATE_DATA_INFO_NV:
      return sizeof(VkOpticalFlowSessionCreatePrivateDataInfoNV);
    case VK_STRUCTURE_TYPE_OUT_OF_BAND_QUEUE_TYPE_INFO_NV:
      return sizeof(VkOutOfBandQueueTypeInfoNV);
    case VK_STRUCTURE_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_FLAGS_NV:
      return sizeof(VkPartitionedAccelerationStructureFlagsNV);
    case VK_STRUCTURE_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_INSTANCES_INPUT_NV:
      return sizeof(VkPartitionedAccelerationStructureInstancesInputNV);
    case VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_EXT:
      return sizeof(VkPastPresentationTimingEXT);
    case VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_INFO_EXT:
      return sizeof(VkPastPresentationTimingInfoEXT);
    case VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_PROPERTIES_EXT:
      return sizeof(VkPastPresentationTimingPropertiesEXT);
    case VK_STRUCTURE_TYPE_PER_TILE_BEGIN_INFO_QCOM:
      return sizeof(VkPerTileBeginInfoQCOM);
    case VK_STRUCTURE_TYPE_PER_TILE_END_INFO_QCOM:
      return sizeof(VkPerTileEndInfoQCOM);
    case VK_STRUCTURE_TYPE_PERF_HINT_INFO_QCOM:
      return sizeof(VkPerfHintInfoQCOM);
    case VK_STRUCTURE_TYPE_PERFORMANCE_CONFIGURATION_ACQUIRE_INFO_INTEL:
      return sizeof(VkPerformanceConfigurationAcquireInfoINTEL);
    case VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_ARM:
      return sizeof(VkPerformanceCounterARM);
    case VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_DESCRIPTION_ARM:
      return sizeof(VkPerformanceCounterDescriptionARM);
    case VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_DESCRIPTION_KHR:
      return sizeof(VkPerformanceCounterDescriptionKHR);
    case VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_KHR:
      return sizeof(VkPerformanceCounterKHR);
    case VK_STRUCTURE_TYPE_PERFORMANCE_MARKER_INFO_INTEL:
      return sizeof(VkPerformanceMarkerInfoINTEL);
    case VK_STRUCTURE_TYPE_PERFORMANCE_OVERRIDE_INFO_INTEL:
      return sizeof(VkPerformanceOverrideInfoINTEL);
    case VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR:
      return sizeof(VkPerformanceQuerySubmitInfoKHR);
    case VK_STRUCTURE_TYPE_PERFORMANCE_STREAM_MARKER_INFO_INTEL:
      return sizeof(VkPerformanceStreamMarkerInfoINTEL);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
      return sizeof(VkPhysicalDevice16BitStorageFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT:
      return sizeof(VkPhysicalDevice4444FormatsFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
      return sizeof(VkPhysicalDevice8BitStorageFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceASTCDecodeFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceAccelerationStructureFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceAccelerationStructurePropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ADDRESS_BINDING_REPORT_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceAddressBindingReportFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_AMIGO_PROFILING_FEATURES_SEC:
      return sizeof(VkPhysicalDeviceAmigoProfilingFeaturesSEC);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ANTI_LAG_FEATURES_AMD:
      return sizeof(VkPhysicalDeviceAntiLagFeaturesAMD);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_DYNAMIC_STATE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceBorderColorSwizzleFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
      return sizeof(VkPhysicalDeviceBufferDeviceAddressFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceBufferDeviceAddressFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_ACCELERATION_STRUCTURE_FEATURES_NV:
      return sizeof(VkPhysicalDeviceClusterAccelerationStructureFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_ACCELERATION_STRUCTURE_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceClusterAccelerationStructurePropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_FEATURES_HUAWEI:
      return sizeof(VkPhysicalDeviceClusterCullingShaderFeaturesHUAWEI);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_PROPERTIES_HUAWEI:
      return sizeof(VkPhysicalDeviceClusterCullingShaderPropertiesHUAWEI);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_VRS_FEATURES_HUAWEI:
      return sizeof(VkPhysicalDeviceClusterCullingShaderVrsFeaturesHUAWEI);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD:
      return sizeof(VkPhysicalDeviceCoherentMemoryFeaturesAMD);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceColorWriteEnableFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMMAND_BUFFER_INHERITANCE_FEATURES_NV:
      return sizeof(VkPhysicalDeviceCommandBufferInheritanceFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_OCCUPANCY_PRIORITY_FEATURES_NV:
      return sizeof(VkPhysicalDeviceComputeOccupancyPriorityFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceComputeShaderDerivativesPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceConditionalRenderingFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceConservativeRasterizationPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_2_FEATURES_NV:
      return sizeof(VkPhysicalDeviceCooperativeMatrix2FeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_2_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceCooperativeMatrix2PropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_CONVERSION_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceCooperativeMatrixConversionFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_DECODE_VECTOR_FEATURES_NV:
      return sizeof(VkPhysicalDeviceCooperativeMatrixDecodeVectorFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceCooperativeMatrixFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV:
      return sizeof(VkPhysicalDeviceCooperativeMatrixFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceCooperativeMatrixPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceCooperativeMatrixPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_VECTOR_FEATURES_NV:
      return sizeof(VkPhysicalDeviceCooperativeVectorFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_VECTOR_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceCooperativeVectorPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceCopyMemoryIndirectFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_FEATURES_NV:
      return sizeof(VkPhysicalDeviceCopyMemoryIndirectFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceCopyMemoryIndirectPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV:
      return sizeof(VkPhysicalDeviceCornerSampledImageFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV:
      return sizeof(VkPhysicalDeviceCoverageReductionModeFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_CLAMP_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceCubicClampFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_WEIGHTS_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceCubicWeightsFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUDA_KERNEL_LAUNCH_FEATURES_NV:
      return sizeof(VkPhysicalDeviceCudaKernelLaunchFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUDA_KERNEL_LAUNCH_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceCudaKernelLaunchPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceCustomBorderColorFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceCustomBorderColorPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_RESOLVE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceCustomResolveFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DATA_GRAPH_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceDataGraphFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DATA_GRAPH_MODEL_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceDataGraphModelFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DATA_GRAPH_NEURAL_ACCELERATOR_STATISTICS_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceDataGraphNeuralAcceleratorStatisticsFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DATA_GRAPH_OPTICAL_FLOW_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceDataGraphOpticalFlowFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV:
      return sizeof(VkPhysicalDeviceDedicatedAllocationImageAliasingFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DENSE_GEOMETRY_FORMAT_FEATURES_AMDX:
      return sizeof(VkPhysicalDeviceDenseGeometryFormatFeaturesAMDX);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceDepthBiasControlFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_CONTROL_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceDepthClampControlFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_ZERO_ONE_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceDepthClampZeroOneFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceDepthClipControlFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceDepthClipEnableFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES:
      return sizeof(VkPhysicalDeviceDepthStencilResolveProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_DENSITY_MAP_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceDescriptorBufferDensityMapPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceDescriptorBufferFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceDescriptorBufferPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_TENSOR_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceDescriptorBufferTensorFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_TENSOR_PROPERTIES_ARM:
      return sizeof(VkPhysicalDeviceDescriptorBufferTensorPropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceDescriptorHeapFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceDescriptorHeapPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_TENSOR_PROPERTIES_ARM:
      return sizeof(VkPhysicalDeviceDescriptorHeapTensorPropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
      return sizeof(VkPhysicalDeviceDescriptorIndexingFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES:
      return sizeof(VkPhysicalDeviceDescriptorIndexingProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_POOL_OVERALLOCATION_FEATURES_NV:
      return sizeof(VkPhysicalDeviceDescriptorPoolOverallocationFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_SET_HOST_MAPPING_FEATURES_VALVE:
      return sizeof(VkPhysicalDeviceDescriptorSetHostMappingFeaturesVALVE);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_COMPUTE_FEATURES_NV:
      return sizeof(VkPhysicalDeviceDeviceGeneratedCommandsComputeFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV:
      return sizeof(VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceDeviceMemoryReportFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV:
      return sizeof(VkPhysicalDeviceDiagnosticsConfigFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceDiscardRectanglePropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_FEATURES_NV:
      return sizeof(VkPhysicalDeviceDisplacementMicromapFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceDisplacementMicromapPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES:
      return sizeof(VkPhysicalDeviceDriverProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceDrmPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES:
      return sizeof(VkPhysicalDeviceDynamicRenderingFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES:
      return sizeof(VkPhysicalDeviceDynamicRenderingLocalReadFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ELAPSED_TIMER_QUERY_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceElapsedTimerQueryFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV:
      return sizeof(VkPhysicalDeviceExclusiveScissorFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceExtendedDynamicState2FeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceExtendedDynamicState3FeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceExtendedDynamicState3PropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceExtendedDynamicStateFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_FLAGS_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceExtendedFlagsFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_SPARSE_ADDRESS_SPACE_FEATURES_NV:
      return sizeof(VkPhysicalDeviceExtendedSparseAddressSpaceFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_SPARSE_ADDRESS_SPACE_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceExtendedSparseAddressSpacePropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO:
      return sizeof(VkPhysicalDeviceExternalBufferInfo);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_COMPUTE_QUEUE_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceExternalComputeQueuePropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO:
      return sizeof(VkPhysicalDeviceExternalFenceInfo);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_FEATURES_ANDROID:
      return sizeof(VkPhysicalDeviceExternalFormatResolveFeaturesANDROID);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_PROPERTIES_ANDROID:
      return sizeof(VkPhysicalDeviceExternalFormatResolvePropertiesANDROID);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO:
      return sizeof(VkPhysicalDeviceExternalImageFormatInfo);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceExternalMemoryHostPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_RDMA_FEATURES_NV:
      return sizeof(VkPhysicalDeviceExternalMemoryRDMAFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO:
      return sizeof(VkPhysicalDeviceExternalSemaphoreInfo);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_TENSOR_INFO_ARM:
      return sizeof(VkPhysicalDeviceExternalTensorInfoARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceFaultFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceFaultFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceFaultPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
      return sizeof(VkPhysicalDeviceFeatures2);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES:
      return sizeof(VkPhysicalDeviceFloatControlsProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FORMAT_PACK_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceFormatPackFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceFragmentDensityMap2FeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceFragmentDensityMap2PropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceFragmentDensityMapFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_LAYERED_FEATURES_VALVE:
      return sizeof(VkPhysicalDeviceFragmentDensityMapLayeredFeaturesVALVE);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_LAYERED_PROPERTIES_VALVE:
      return sizeof(VkPhysicalDeviceFragmentDensityMapLayeredPropertiesVALVE);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceFragmentDensityMapOffsetFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceFragmentDensityMapOffsetPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceFragmentDensityMapPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceFragmentShaderBarycentricPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_FEATURES_NV:
      return sizeof(VkPhysicalDeviceFragmentShadingRateEnumsFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceFragmentShadingRateEnumsPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceFragmentShadingRateFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR:
      return sizeof(VkPhysicalDeviceFragmentShadingRateKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceFragmentShadingRatePropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAME_BOUNDARY_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceFrameBoundaryFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES:
      return sizeof(VkPhysicalDeviceGlobalPriorityQueryFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GPA_FEATURES_AMD:
      return sizeof(VkPhysicalDeviceGpaFeaturesAMD);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GPA_PROPERTIES_2_AMD:
      return sizeof(VkPhysicalDeviceGpaProperties2AMD);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GPA_PROPERTIES_AMD:
      return sizeof(VkPhysicalDeviceGpaPropertiesAMD);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES:
      return sizeof(VkPhysicalDeviceGroupProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HDR_VIVID_FEATURES_HUAWEI:
      return sizeof(VkPhysicalDeviceHdrVividFeaturesHUAWEI);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES:
      return sizeof(VkPhysicalDeviceHostImageCopyFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES:
      return sizeof(VkPhysicalDeviceHostImageCopyProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
      return sizeof(VkPhysicalDeviceHostQueryResetFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES:
      return sizeof(VkPhysicalDeviceIDProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceImage2DViewOf3DFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ALIGNMENT_CONTROL_FEATURES_MESA:
      return sizeof(VkPhysicalDeviceImageAlignmentControlFeaturesMESA);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ALIGNMENT_CONTROL_PROPERTIES_MESA:
      return sizeof(VkPhysicalDeviceImageAlignmentControlPropertiesMESA);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceImageCompressionControlFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_SWAPCHAIN_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceImageCompressionControlSwapchainFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT:
      return sizeof(VkPhysicalDeviceImageDrmFormatModifierInfoEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2:
      return sizeof(VkPhysicalDeviceImageFormatInfo2);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_2_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceImageProcessing2FeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_2_PROPERTIES_QCOM:
      return sizeof(VkPhysicalDeviceImageProcessing2PropertiesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_3_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceImageProcessing3FeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceImageProcessingFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_PROPERTIES_QCOM:
      return sizeof(VkPhysicalDeviceImageProcessingPropertiesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES:
      return sizeof(VkPhysicalDeviceImageRobustnessFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_SLICED_VIEW_OF_3D_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceImageSlicedViewOf3DFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT:
      return sizeof(VkPhysicalDeviceImageViewImageFormatInfoEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceImageViewMinLodFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
      return sizeof(VkPhysicalDeviceImagelessFramebufferFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES:
      return sizeof(VkPhysicalDeviceIndexTypeUint8Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INHERITED_VIEWPORT_SCISSOR_FEATURES_NV:
      return sizeof(VkPhysicalDeviceInheritedViewportScissorFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES:
      return sizeof(VkPhysicalDeviceInlineUniformBlockFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES:
      return sizeof(VkPhysicalDeviceInlineUniformBlockProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INTERNALLY_SYNCHRONIZED_QUEUES_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceInternallySynchronizedQueuesFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INVOCATION_MASK_FEATURES_HUAWEI:
      return sizeof(VkPhysicalDeviceInvocationMaskFeaturesHUAWEI);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceLayeredApiPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_PROPERTIES_LIST_KHR:
      return sizeof(VkPhysicalDeviceLayeredApiPropertiesListKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_VULKAN_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceLayeredApiVulkanPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_DRIVER_PROPERTIES_MSFT:
      return sizeof(VkPhysicalDeviceLayeredDriverPropertiesMSFT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_DITHERING_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceLegacyDitheringFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_VERTEX_ATTRIBUTES_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceLegacyVertexAttributesFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_VERTEX_ATTRIBUTES_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceLegacyVertexAttributesPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES:
      return sizeof(VkPhysicalDeviceLineRasterizationFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES:
      return sizeof(VkPhysicalDeviceLineRasterizationProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINEAR_COLOR_ATTACHMENT_FEATURES_NV:
      return sizeof(VkPhysicalDeviceLinearColorAttachmentFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_10_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceMaintenance10FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_10_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceMaintenance10PropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_11_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceMaintenance11FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES:
      return sizeof(VkPhysicalDeviceMaintenance3Properties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES:
      return sizeof(VkPhysicalDeviceMaintenance4Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES:
      return sizeof(VkPhysicalDeviceMaintenance4Properties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES:
      return sizeof(VkPhysicalDeviceMaintenance5Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES:
      return sizeof(VkPhysicalDeviceMaintenance5Properties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES:
      return sizeof(VkPhysicalDeviceMaintenance6Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES:
      return sizeof(VkPhysicalDeviceMaintenance6Properties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_7_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceMaintenance7FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_7_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceMaintenance7PropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_8_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceMaintenance8FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_9_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceMaintenance9FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_9_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceMaintenance9PropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceMapMemoryPlacedFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceMapMemoryPlacedPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceMemoryBudgetPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceMemoryDecompressionFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceMemoryDecompressionPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceMemoryPriorityFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2:
      return sizeof(VkPhysicalDeviceMemoryProperties2);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceMeshShaderFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV:
      return sizeof(VkPhysicalDeviceMeshShaderFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceMeshShaderPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceMeshShaderPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceMultiDrawFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceMultiDrawPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SWAPCHAIN_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceMultisampledRenderToSwapchainFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
      return sizeof(VkPhysicalDeviceMultiviewFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX:
      return sizeof(VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_RENDER_AREAS_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_VIEWPORTS_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES:
      return sizeof(VkPhysicalDeviceMultiviewProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceNestedCommandBufferFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceNestedCommandBufferPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceOpacityMicromapFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceOpacityMicromapFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceOpacityMicromapPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceOpacityMicromapPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_FEATURES_NV:
      return sizeof(VkPhysicalDeviceOpticalFlowFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceOpticalFlowPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT:
      return sizeof(VkPhysicalDevicePCIBusInfoPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT:
      return sizeof(VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PARTITIONED_ACCELERATION_STRUCTURE_FEATURES_NV:
      return sizeof(VkPhysicalDevicePartitionedAccelerationStructureFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PARTITIONED_ACCELERATION_STRUCTURE_PROPERTIES_NV:
      return sizeof(VkPhysicalDevicePartitionedAccelerationStructurePropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PER_STAGE_DESCRIPTOR_SET_FEATURES_NV:
      return sizeof(VkPhysicalDevicePerStageDescriptorSetFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_COUNTERS_BY_REGION_FEATURES_ARM:
      return sizeof(VkPhysicalDevicePerformanceCountersByRegionFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_COUNTERS_BY_REGION_PROPERTIES_ARM:
      return sizeof(VkPhysicalDevicePerformanceCountersByRegionPropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR:
      return sizeof(VkPhysicalDevicePerformanceQueryFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR:
      return sizeof(VkPhysicalDevicePerformanceQueryPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_BINARY_FEATURES_KHR:
      return sizeof(VkPhysicalDevicePipelineBinaryFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_BINARY_PROPERTIES_KHR:
      return sizeof(VkPhysicalDevicePipelineBinaryPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CACHE_INCREMENTAL_MODE_FEATURES_SEC:
      return sizeof(VkPhysicalDevicePipelineCacheIncrementalModeFeaturesSEC);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES:
      return sizeof(VkPhysicalDevicePipelineCreationCacheControlFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR:
      return sizeof(VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_LIBRARY_GROUP_HANDLES_FEATURES_EXT:
      return sizeof(VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_OPACITY_MICROMAP_FEATURES_ARM:
      return sizeof(VkPhysicalDevicePipelineOpacityMicromapFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROPERTIES_FEATURES_EXT:
      return sizeof(VkPhysicalDevicePipelinePropertiesFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROTECTED_ACCESS_FEATURES:
      return sizeof(VkPhysicalDevicePipelineProtectedAccessFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES:
      return sizeof(VkPhysicalDevicePipelineRobustnessFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_PROPERTIES:
      return sizeof(VkPhysicalDevicePipelineRobustnessProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES:
      return sizeof(VkPhysicalDevicePointClippingProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR:
      return sizeof(VkPhysicalDevicePortabilitySubsetFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR:
      return sizeof(VkPhysicalDevicePortabilitySubsetPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_BARRIER_FEATURES_NV:
      return sizeof(VkPhysicalDevicePresentBarrierFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR:
      return sizeof(VkPhysicalDevicePresentId2FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR:
      return sizeof(VkPhysicalDevicePresentIdFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_METERING_FEATURES_NV:
      return sizeof(VkPhysicalDevicePresentMeteringFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR:
      return sizeof(VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT:
      return sizeof(VkPhysicalDevicePresentTimingFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_2_FEATURES_KHR:
      return sizeof(VkPhysicalDevicePresentWait2FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR:
      return sizeof(VkPhysicalDevicePresentWaitFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_RESTART_INDEX_FEATURES_EXT:
      return sizeof(VkPhysicalDevicePrimitiveRestartIndexFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT:
      return sizeof(VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT:
      return sizeof(VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES:
      return sizeof(VkPhysicalDevicePrivateDataFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2:
      return sizeof(VkPhysicalDeviceProperties2);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
      return sizeof(VkPhysicalDeviceProtectedMemoryFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES:
      return sizeof(VkPhysicalDeviceProtectedMemoryProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceProvokingVertexFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceProvokingVertexPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_CONSTANT_BANK_FEATURES_NV:
      return sizeof(VkPhysicalDevicePushConstantBankFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_CONSTANT_BANK_PROPERTIES_NV:
      return sizeof(VkPhysicalDevicePushConstantBankPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES:
      return sizeof(VkPhysicalDevicePushDescriptorProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_QUEUE_FAMILY_DATA_GRAPH_PROCESSING_ENGINE_INFO_ARM:
      return sizeof(VkPhysicalDeviceQueueFamilyDataGraphProcessingEngineInfoARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_QUEUE_PERF_HINT_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceQueuePerfHintFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_QUEUE_PERF_HINT_PROPERTIES_QCOM:
      return sizeof(VkPhysicalDeviceQueuePerfHintPropertiesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RGBA10X6_FORMATS_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAW_ACCESS_CHAINS_FEATURES_NV:
      return sizeof(VkPhysicalDeviceRawAccessChainsFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceRayQueryFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceRayTracingInvocationReorderFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV:
      return sizeof(VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceRayTracingInvocationReorderPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceRayTracingInvocationReorderPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_LINEAR_SWEPT_SPHERES_FEATURES_NV:
      return sizeof(VkPhysicalDeviceRayTracingLinearSweptSpheresFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV:
      return sizeof(VkPhysicalDeviceRayTracingMotionBlurFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceRayTracingPipelineFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceRayTracingPipelinePropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceRayTracingPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV:
      return sizeof(VkPhysicalDeviceRayTracingValidationFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RELAXED_LINE_RASTERIZATION_FEATURES_IMG:
      return sizeof(VkPhysicalDeviceRelaxedLineRasterizationFeaturesIMG);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RENDER_PASS_STRIPED_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceRenderPassStripedFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RENDER_PASS_STRIPED_PROPERTIES_ARM:
      return sizeof(VkPhysicalDeviceRenderPassStripedPropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV:
      return sizeof(VkPhysicalDeviceRepresentativeFragmentTestFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceRobustness2FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceRobustness2PropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceSampleLocationsPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES:
      return sizeof(VkPhysicalDeviceSamplerFilterMinmaxProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
      return sizeof(VkPhysicalDeviceSamplerYcbcrConversionFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
      return sizeof(VkPhysicalDeviceScalarBlockLayoutFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_DISPATCH_PARAMETERS_PROPERTIES_ARM:
      return sizeof(VkPhysicalDeviceSchedulingControlsDispatchParametersPropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceSchedulingControlsFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_PROPERTIES_ARM:
      return sizeof(VkPhysicalDeviceSchedulingControlsPropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
      return sizeof(VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_64_BIT_INDEXING_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShader64BitIndexingFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ABORT_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderAbortFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ABORT_PROPERTIES_KHR:
      return sizeof(VkPhysicalDeviceShaderAbortPropertiesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT16_VECTOR_FEATURES_NV:
      return sizeof(VkPhysicalDeviceShaderAtomicFloat16VectorFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderAtomicFloatFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
      return sizeof(VkPhysicalDeviceShaderAtomicInt64Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_BFLOAT16_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderBfloat16FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderClockFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CONSTANT_DATA_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderConstantDataFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_BUILTINS_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceShaderCoreBuiltinsFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_BUILTINS_PROPERTIES_ARM:
      return sizeof(VkPhysicalDeviceShaderCoreBuiltinsPropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD:
      return sizeof(VkPhysicalDeviceShaderCoreProperties2AMD);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD:
      return sizeof(VkPhysicalDeviceShaderCorePropertiesAMD);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_ARM:
      return sizeof(VkPhysicalDeviceShaderCorePropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES:
      return sizeof(VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
      return sizeof(VkPhysicalDeviceShaderDrawParametersFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EARLY_AND_LATE_FRAGMENT_TESTS_FEATURES_AMD:
      return sizeof(VkPhysicalDeviceShaderEarlyAndLateFragmentTestsFeaturesAMD);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ENQUEUE_FEATURES_AMDX:
      return sizeof(VkPhysicalDeviceShaderEnqueueFeaturesAMDX);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ENQUEUE_PROPERTIES_AMDX:
      return sizeof(VkPhysicalDeviceShaderEnqueuePropertiesAMDX);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EXPECT_ASSUME_FEATURES:
      return sizeof(VkPhysicalDeviceShaderExpectAssumeFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
      return sizeof(VkPhysicalDeviceShaderFloat16Int8Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT8_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderFloat8FeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT_CONTROLS_2_FEATURES:
      return sizeof(VkPhysicalDeviceShaderFloatControls2Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FMA_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderFmaFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV:
      return sizeof(VkPhysicalDeviceShaderImageFootprintFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INSTRUMENTATION_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceShaderInstrumentationFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INSTRUMENTATION_PROPERTIES_ARM:
      return sizeof(VkPhysicalDeviceShaderInstrumentationPropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES:
      return sizeof(VkPhysicalDeviceShaderIntegerDotProductFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES:
      return sizeof(VkPhysicalDeviceShaderIntegerDotProductProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL:
      return sizeof(VkPhysicalDeviceShaderIntegerFunctions2FeaturesINTEL);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_LONG_VECTOR_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderLongVectorFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_LONG_VECTOR_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceShaderLongVectorPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MAXIMAL_RECONVERGENCE_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderMaximalReconvergenceFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MIXED_FLOAT_DOT_PRODUCT_FEATURES_VALVE:
      return sizeof(VkPhysicalDeviceShaderMixedFloatDotProductFeaturesVALVE);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceShaderModuleIdentifierPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MULTIPLE_WAIT_QUEUES_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceShaderMultipleWaitQueuesFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MULTIPLE_WAIT_QUEUES_PROPERTIES_QCOM:
      return sizeof(VkPhysicalDeviceShaderMultipleWaitQueuesPropertiesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OCP_MICROSCALING_TYPES_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderOCPMicroscalingTypesFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderObjectFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceShaderObjectPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_QUAD_CONTROL_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderQuadControlFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_REPLICATED_COMPOSITES_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderReplicatedCompositesFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV:
      return sizeof(VkPhysicalDeviceShaderSMBuiltinsFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceShaderSMBuiltinsPropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SPLIT_BARRIER_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderSplitBarrierFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SPLIT_BARRIER_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceShaderSplitBarrierPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
      return sizeof(VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_PARTITIONED_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderSubgroupPartitionedFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_ROTATE_FEATURES:
      return sizeof(VkPhysicalDeviceShaderSubgroupRotateFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES:
      return sizeof(VkPhysicalDeviceShaderTerminateInvocationFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderTileImageFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceShaderTileImagePropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNIFORM_BUFFER_UNSIZED_ARRAY_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceShaderUniformBufferUnsizedArrayFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceShaderUntypedPointersFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV:
      return sizeof(VkPhysicalDeviceShadingRateImageFeaturesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_PROPERTIES_NV:
      return sizeof(VkPhysicalDeviceShadingRateImagePropertiesNV);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2:
      return sizeof(VkPhysicalDeviceSparseImageFormatInfo2);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES:
      return sizeof(VkPhysicalDeviceSubgroupProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES:
      return sizeof(VkPhysicalDeviceSubgroupSizeControlFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES:
      return sizeof(VkPhysicalDeviceSubgroupSizeControlProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_MERGE_FEEDBACK_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceSubpassMergeFeedbackFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_SHADING_FEATURES_HUAWEI:
      return sizeof(VkPhysicalDeviceSubpassShadingFeaturesHUAWEI);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_SHADING_PROPERTIES_HUAWEI:
      return sizeof(VkPhysicalDeviceSubpassShadingPropertiesHUAWEI);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR:
      return sizeof(VkPhysicalDeviceSurfaceInfo2KHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES:
      return sizeof(VkPhysicalDeviceSynchronization2Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TENSOR_FEATURES_ARM:
      return sizeof(VkPhysicalDeviceTensorFeaturesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TENSOR_PROPERTIES_ARM:
      return sizeof(VkPhysicalDeviceTensorPropertiesARM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES:
      return sizeof(VkPhysicalDeviceTexelBufferAlignmentProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_3D_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceTextureCompressionASTC3DFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES:
      return sizeof(VkPhysicalDeviceTextureCompressionASTCHDRFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_THROTTLE_HINT_FEATURES_SEC:
      return sizeof(VkPhysicalDeviceThrottleHintFeaturesSEC);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_MEMORY_HEAP_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceTileMemoryHeapFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_MEMORY_HEAP_PROPERTIES_QCOM:
      return sizeof(VkPhysicalDeviceTileMemoryHeapPropertiesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceTilePropertiesFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_SHADING_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceTileShadingFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_SHADING_PROPERTIES_QCOM:
      return sizeof(VkPhysicalDeviceTileShadingPropertiesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
      return sizeof(VkPhysicalDeviceTimelineSemaphoreFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES:
      return sizeof(VkPhysicalDeviceTimelineSemaphoreProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES:
      return sizeof(VkPhysicalDeviceToolProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceTransformFeedbackFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceTransformFeedbackPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
      return sizeof(VkPhysicalDeviceUniformBufferStandardLayoutFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
      return sizeof(VkPhysicalDeviceVariablePointersFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES:
      return sizeof(VkPhysicalDeviceVertexAttributeDivisorFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES:
      return sizeof(VkPhysicalDeviceVertexAttributeDivisorProperties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT:
      return sizeof(VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_ROBUSTNESS_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceVertexAttributeRobustnessFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_DECODE_VP9_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceVideoDecodeVP9FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_AV1_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceVideoEncodeAV1FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_FEEDBACK_2_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceVideoEncodeFeedback2FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_INTRA_REFRESH_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceVideoEncodeIntraRefreshFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR:
      return sizeof(VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_QUANTIZATION_MAP_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceVideoEncodeQuantizationMapFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_RGB_CONVERSION_FEATURES_VALVE:
      return sizeof(VkPhysicalDeviceVideoEncodeRgbConversionFeaturesVALVE);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR:
      return sizeof(VkPhysicalDeviceVideoFormatInfoKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_MAINTENANCE_1_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceVideoMaintenance1FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_MAINTENANCE_2_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceVideoMaintenance2FeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
      return sizeof(VkPhysicalDeviceVulkan11Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES:
      return sizeof(VkPhysicalDeviceVulkan11Properties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
      return sizeof(VkPhysicalDeviceVulkan12Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES:
      return sizeof(VkPhysicalDeviceVulkan12Properties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
      return sizeof(VkPhysicalDeviceVulkan13Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES:
      return sizeof(VkPhysicalDeviceVulkan13Properties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES:
      return sizeof(VkPhysicalDeviceVulkan14Features);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES:
      return sizeof(VkPhysicalDeviceVulkan14Properties);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
      return sizeof(VkPhysicalDeviceVulkanMemoryModelFeatures);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR:
      return sizeof(VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_2_PLANE_444_FORMATS_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_DEGAMMA_FEATURES_QCOM:
      return sizeof(VkPhysicalDeviceYcbcrDegammaFeaturesQCOM);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceYcbcrImageArraysFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_DEVICE_MEMORY_FEATURES_EXT:
      return sizeof(VkPhysicalDeviceZeroInitializeDeviceMemoryFeaturesEXT);
    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES:
      return sizeof(VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures);
    case VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR:
      return sizeof(VkPipelineBinaryCreateInfoKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_BINARY_DATA_INFO_KHR:
      return sizeof(VkPipelineBinaryDataInfoKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR:
      return sizeof(VkPipelineBinaryHandlesInfoKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR:
      return sizeof(VkPipelineBinaryInfoKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR:
      return sizeof(VkPipelineBinaryKeyKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO:
      return sizeof(VkPipelineCacheCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT:
      return sizeof(VkPipelineColorBlendAdvancedStateCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO:
      return sizeof(VkPipelineColorBlendStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT:
      return sizeof(VkPipelineColorWriteCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_COMPILER_CONTROL_CREATE_INFO_AMD:
      return sizeof(VkPipelineCompilerControlCreateInfoAMD);
    case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineCoverageModulationStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_REDUCTION_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineCoverageReductionStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineCoverageToColorStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO:
      return sizeof(VkPipelineCreateFlags2CreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_CREATE_INFO_KHR:
      return sizeof(VkPipelineCreateInfoKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO:
      return sizeof(VkPipelineCreationFeedbackCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO:
      return sizeof(VkPipelineDepthStencilStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT:
      return sizeof(VkPipelineDiscardRectangleStateCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO:
      return sizeof(VkPipelineDynamicStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR:
      return sizeof(VkPipelineExecutableInfoKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR:
      return sizeof(VkPipelineExecutableInternalRepresentationKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR:
      return sizeof(VkPipelineExecutablePropertiesKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR:
      return sizeof(VkPipelineExecutableStatisticKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_DENSITY_MAP_LAYERED_CREATE_INFO_VALVE:
      return sizeof(VkPipelineFragmentDensityMapLayeredCreateInfoVALVE);
    case VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_ENUM_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineFragmentShadingRateEnumStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR:
      return sizeof(VkPipelineFragmentShadingRateStateCreateInfoKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_INDIRECT_DEVICE_ADDRESS_INFO_NV:
      return sizeof(VkPipelineIndirectDeviceAddressInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR:
      return sizeof(VkPipelineInfoKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO:
      return sizeof(VkPipelineInputAssemblyStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO:
      return sizeof(VkPipelineLayoutCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR:
      return sizeof(VkPipelineLibraryCreateInfoKHR);
    case VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO:
      return sizeof(VkPipelineMultisampleStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_PROPERTIES_IDENTIFIER_EXT:
      return sizeof(VkPipelinePropertiesIdentifierEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT:
      return sizeof(VkPipelineRasterizationConservativeStateCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
      return sizeof(VkPipelineRasterizationDepthClipStateCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO:
      return sizeof(VkPipelineRasterizationLineStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT:
      return sizeof(VkPipelineRasterizationProvokingVertexStateCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO:
      return sizeof(VkPipelineRasterizationStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD:
      return sizeof(VkPipelineRasterizationStateRasterizationOrderAMD);
    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT:
      return sizeof(VkPipelineRasterizationStateStreamCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO:
      return sizeof(VkPipelineRenderingCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_REPRESENTATIVE_FRAGMENT_TEST_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineRepresentativeFragmentTestStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO:
      return sizeof(VkPipelineRobustnessCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT:
      return sizeof(VkPipelineSampleLocationsStateCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO:
      return sizeof(VkPipelineShaderStageCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT:
      return sizeof(VkPipelineShaderStageModuleIdentifierCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_NODE_CREATE_INFO_AMDX:
      return sizeof(VkPipelineShaderStageNodeCreateInfoAMDX);
    case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO:
      return sizeof(VkPipelineShaderStageRequiredSubgroupSizeCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO:
      return sizeof(VkPipelineTessellationDomainOriginStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO:
      return sizeof(VkPipelineTessellationStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO:
      return sizeof(VkPipelineVertexInputDivisorStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO:
      return sizeof(VkPipelineVertexInputStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineViewportCoarseSampleOrderStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLAMP_CONTROL_CREATE_INFO_EXT:
      return sizeof(VkPipelineViewportDepthClampControlCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT:
      return sizeof(VkPipelineViewportDepthClipControlCreateInfoEXT);
    case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineViewportExclusiveScissorStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineViewportShadingRateImageStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO:
      return sizeof(VkPipelineViewportStateCreateInfo);
    case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineViewportSwizzleStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV:
      return sizeof(VkPipelineViewportWScalingStateCreateInfoNV);
    case VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR:
      return sizeof(VkPresentId2KHR);
    case VK_STRUCTURE_TYPE_PRESENT_ID_KHR:
      return sizeof(VkPresentIdKHR);
    case VK_STRUCTURE_TYPE_PRESENT_INFO_KHR:
      return sizeof(VkPresentInfoKHR);
    case VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR:
      return sizeof(VkPresentRegionsKHR);
    case VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE:
      return sizeof(VkPresentTimesInfoGOOGLE);
    case VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT:
      return sizeof(VkPresentTimingInfoEXT);
    case VK_STRUCTURE_TYPE_PRESENT_TIMING_SURFACE_CAPABILITIES_EXT:
      return sizeof(VkPresentTimingSurfaceCapabilitiesEXT);
    case VK_STRUCTURE_TYPE_PRESENT_TIMINGS_INFO_EXT:
      return sizeof(VkPresentTimingsInfoEXT);
    case VK_STRUCTURE_TYPE_PRESENT_WAIT_2_INFO_KHR:
      return sizeof(VkPresentWait2InfoKHR);
    case VK_STRUCTURE_TYPE_PRIVATE_DATA_SLOT_CREATE_INFO:
      return sizeof(VkPrivateDataSlotCreateInfo);
    case VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO:
      return sizeof(VkProtectedSubmitInfo);
    case VK_STRUCTURE_TYPE_PUSH_CONSTANT_BANK_INFO_NV:
      return sizeof(VkPushConstantBankInfoNV);
    case VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO:
      return sizeof(VkPushConstantsInfo);
    case VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT:
      return sizeof(VkPushDataInfoEXT);
    case VK_STRUCTURE_TYPE_PUSH_DESCRIPTOR_SET_INFO:
      return sizeof(VkPushDescriptorSetInfo);
    case VK_STRUCTURE_TYPE_PUSH_DESCRIPTOR_SET_WITH_TEMPLATE_INFO:
      return sizeof(VkPushDescriptorSetWithTemplateInfo);
    case VK_STRUCTURE_TYPE_QUERY_LOW_LATENCY_SUPPORT_NV:
      return sizeof(VkQueryLowLatencySupportNV);
    case VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO:
      return sizeof(VkQueryPoolCreateInfo);
    case VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR:
      return sizeof(VkQueryPoolPerformanceCreateInfoKHR);
    case VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_QUERY_CREATE_INFO_INTEL:
      return sizeof(VkQueryPoolPerformanceQueryCreateInfoINTEL);
    case VK_STRUCTURE_TYPE_QUERY_POOL_VIDEO_ENCODE_FEEDBACK_CREATE_INFO_KHR:
      return sizeof(VkQueryPoolVideoEncodeFeedbackCreateInfoKHR);
    case VK_STRUCTURE_TYPE_QUERY_POOL_VIDEO_ENCODE_PER_PARTITION_FEEDBACK_CREATE_INFO_KHR:
      return sizeof(VkQueryPoolVideoEncodePerPartitionFeedbackCreateInfoKHR);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_2_NV:
      return sizeof(VkQueueFamilyCheckpointProperties2NV);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_NV:
      return sizeof(VkQueueFamilyCheckpointPropertiesNV);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_DATA_GRAPH_OPTICAL_FLOW_PROPERTIES_ARM:
      return sizeof(VkQueueFamilyDataGraphOpticalFlowPropertiesARM);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_DATA_GRAPH_PROCESSING_ENGINE_PROPERTIES_ARM:
      return sizeof(VkQueueFamilyDataGraphProcessingEnginePropertiesARM);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_DATA_GRAPH_PROPERTIES_ARM:
      return sizeof(VkQueueFamilyDataGraphPropertiesARM);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_DATA_GRAPH_TOSA_PROPERTIES_ARM:
      return sizeof(VkQueueFamilyDataGraphTOSAPropertiesARM);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES:
      return sizeof(VkQueueFamilyGlobalPriorityProperties);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_OPTIMAL_IMAGE_TRANSFER_GRANULARITY_PROPERTIES_KHR:
      return sizeof(VkQueueFamilyOptimalImageTransferGranularityPropertiesKHR);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_OWNERSHIP_TRANSFER_PROPERTIES_KHR:
      return sizeof(VkQueueFamilyOwnershipTransferPropertiesKHR);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2:
      return sizeof(VkQueueFamilyProperties2);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR:
      return sizeof(VkQueueFamilyQueryResultStatusPropertiesKHR);
    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR:
      return sizeof(VkQueueFamilyVideoPropertiesKHR);
    case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CLUSTER_ACCELERATION_STRUCTURE_CREATE_INFO_NV:
      return sizeof(VkRayTracingPipelineClusterAccelerationStructureCreateInfoNV);
    case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR:
      return sizeof(VkRayTracingPipelineCreateInfoKHR);
    case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV:
      return sizeof(VkRayTracingPipelineCreateInfoNV);
    case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR:
      return sizeof(VkRayTracingPipelineInterfaceCreateInfoKHR);
    case VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR:
      return sizeof(VkRayTracingShaderGroupCreateInfoKHR);
    case VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV:
      return sizeof(VkRayTracingShaderGroupCreateInfoNV);
    case VK_STRUCTURE_TYPE_RELEASE_CAPTURED_PIPELINE_DATA_INFO_KHR:
      return sizeof(VkReleaseCapturedPipelineDataInfoKHR);
    case VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_KHR:
      return sizeof(VkReleaseSwapchainImagesInfoKHR);
    case VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO:
      return sizeof(VkRenderPassAttachmentBeginInfo);
    case VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO:
      return sizeof(VkRenderPassBeginInfo);
    case VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO:
      return sizeof(VkRenderPassCreateInfo);
    case VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2:
      return sizeof(VkRenderPassCreateInfo2);
    case VK_STRUCTURE_TYPE_RENDER_PASS_CREATION_CONTROL_EXT:
      return sizeof(VkRenderPassCreationControlEXT);
    case VK_STRUCTURE_TYPE_RENDER_PASS_CREATION_FEEDBACK_CREATE_INFO_EXT:
      return sizeof(VkRenderPassCreationFeedbackCreateInfoEXT);
    case VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT:
      return sizeof(VkRenderPassFragmentDensityMapCreateInfoEXT);
    case VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_OFFSET_END_INFO_EXT:
      return sizeof(VkRenderPassFragmentDensityMapOffsetEndInfoEXT);
    case VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO:
      return sizeof(VkRenderPassInputAttachmentAspectCreateInfo);
    case VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO:
      return sizeof(VkRenderPassMultiviewCreateInfo);
    case VK_STRUCTURE_TYPE_RENDER_PASS_PERFORMANCE_COUNTERS_BY_REGION_BEGIN_INFO_ARM:
      return sizeof(VkRenderPassPerformanceCountersByRegionBeginInfoARM);
    case VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT:
      return sizeof(VkRenderPassSampleLocationsBeginInfoEXT);
    case VK_STRUCTURE_TYPE_RENDER_PASS_STRIPE_BEGIN_INFO_ARM:
      return sizeof(VkRenderPassStripeBeginInfoARM);
    case VK_STRUCTURE_TYPE_RENDER_PASS_STRIPE_INFO_ARM:
      return sizeof(VkRenderPassStripeInfoARM);
    case VK_STRUCTURE_TYPE_RENDER_PASS_STRIPE_SUBMIT_INFO_ARM:
      return sizeof(VkRenderPassStripeSubmitInfoARM);
    case VK_STRUCTURE_TYPE_RENDER_PASS_SUBPASS_FEEDBACK_CREATE_INFO_EXT:
      return sizeof(VkRenderPassSubpassFeedbackCreateInfoEXT);
    case VK_STRUCTURE_TYPE_RENDER_PASS_TILE_SHADING_CREATE_INFO_QCOM:
      return sizeof(VkRenderPassTileShadingCreateInfoQCOM);
    case VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM:
      return sizeof(VkRenderPassTransformBeginInfoQCOM);
    case VK_STRUCTURE_TYPE_RENDERING_AREA_INFO:
      return sizeof(VkRenderingAreaInfo);
    case VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_FLAGS_INFO_KHR:
      return sizeof(VkRenderingAttachmentFlagsInfoKHR);
    case VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO:
      return sizeof(VkRenderingAttachmentInfo);
    case VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO:
      return sizeof(VkRenderingAttachmentLocationInfo);
    case VK_STRUCTURE_TYPE_RENDERING_END_INFO_KHR:
      return sizeof(VkRenderingEndInfoKHR);
    case VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT:
      return sizeof(VkRenderingFragmentDensityMapAttachmentInfoEXT);
    case VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR:
      return sizeof(VkRenderingFragmentShadingRateAttachmentInfoKHR);
    case VK_STRUCTURE_TYPE_RENDERING_INFO:
      return sizeof(VkRenderingInfo);
    case VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO:
      return sizeof(VkRenderingInputAttachmentIndexInfo);
    case VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2:
      return sizeof(VkResolveImageInfo2);
    case VK_STRUCTURE_TYPE_RESOLVE_IMAGE_MODE_INFO_KHR:
      return sizeof(VkResolveImageModeInfoKHR);
    case VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT:
      return sizeof(VkResourceDescriptorInfoEXT);
    case VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT:
      return sizeof(VkSampleLocationsInfoEXT);
    case VK_STRUCTURE_TYPE_SAMPLER_BLOCK_MATCH_WINDOW_CREATE_INFO_QCOM:
      return sizeof(VkSamplerBlockMatchWindowCreateInfoQCOM);
    case VK_STRUCTURE_TYPE_SAMPLER_BORDER_COLOR_COMPONENT_MAPPING_CREATE_INFO_EXT:
      return sizeof(VkSamplerBorderColorComponentMappingCreateInfoEXT);
    case VK_STRUCTURE_TYPE_SAMPLER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:
      return sizeof(VkSamplerCaptureDescriptorDataInfoEXT);
    case VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO:
      return sizeof(VkSamplerCreateInfo);
    case VK_STRUCTURE_TYPE_SAMPLER_CUBIC_WEIGHTS_CREATE_INFO_QCOM:
      return sizeof(VkSamplerCubicWeightsCreateInfoQCOM);
    case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT:
      return sizeof(VkSamplerCustomBorderColorCreateInfoEXT);
    case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_INDEX_CREATE_INFO_EXT:
      return sizeof(VkSamplerCustomBorderColorIndexCreateInfoEXT);
    case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO:
      return sizeof(VkSamplerReductionModeCreateInfo);
    case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO:
      return sizeof(VkSamplerYcbcrConversionCreateInfo);
    case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES:
      return sizeof(VkSamplerYcbcrConversionImageFormatProperties);
    case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
      return sizeof(VkSamplerYcbcrConversionInfo);
    case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_YCBCR_DEGAMMA_CREATE_INFO_QCOM:
      return sizeof(VkSamplerYcbcrConversionYcbcrDegammaCreateInfoQCOM);
    case VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO:
      return sizeof(VkSemaphoreCreateInfo);
    case VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR:
      return sizeof(VkSemaphoreGetFdInfoKHR);
    case VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR:
      return sizeof(VkSemaphoreGetWin32HandleInfoKHR);
    case VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO:
      return sizeof(VkSemaphoreSignalInfo);
    case VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO:
      return sizeof(VkSemaphoreSubmitInfo);
    case VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO:
      return sizeof(VkSemaphoreTypeCreateInfo);
    case VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO:
      return sizeof(VkSemaphoreWaitInfo);
    case VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT:
      return sizeof(VkSetDescriptorBufferOffsetsInfoEXT);
    case VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV:
      return sizeof(VkSetLatencyMarkerInfoNV);
    case VK_STRUCTURE_TYPE_SET_PRESENT_CONFIG_NV:
      return sizeof(VkSetPresentConfigNV);
    case VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT:
      return sizeof(VkShaderCreateInfoEXT);
    case VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT:
      return sizeof(VkShaderDescriptorSetAndBindingMappingInfoEXT);
    case VK_STRUCTURE_TYPE_SHADER_INSTRUMENTATION_CREATE_INFO_ARM:
      return sizeof(VkShaderInstrumentationCreateInfoARM);
    case VK_STRUCTURE_TYPE_SHADER_INSTRUMENTATION_METRIC_DESCRIPTION_ARM:
      return sizeof(VkShaderInstrumentationMetricDescriptionARM);
    case VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO:
      return sizeof(VkShaderModuleCreateInfo);
    case VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT:
      return sizeof(VkShaderModuleIdentifierEXT);
    case VK_STRUCTURE_TYPE_SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT:
      return sizeof(VkShaderModuleValidationCacheCreateInfoEXT);
    case VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_2_KHR:
      return sizeof(VkSharedPresentSurfaceCapabilities2KHR);
    case VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR:
      return sizeof(VkSharedPresentSurfaceCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2:
      return sizeof(VkSparseImageFormatProperties2);
    case VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2:
      return sizeof(VkSparseImageMemoryRequirements2);
    case VK_STRUCTURE_TYPE_SUBMIT_INFO:
      return sizeof(VkSubmitInfo);
    case VK_STRUCTURE_TYPE_SUBMIT_INFO_2:
      return sizeof(VkSubmitInfo2);
    case VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO:
      return sizeof(VkSubpassBeginInfo);
    case VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2:
      return sizeof(VkSubpassDependency2);
    case VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2:
      return sizeof(VkSubpassDescription2);
    case VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE:
      return sizeof(VkSubpassDescriptionDepthStencilResolve);
    case VK_STRUCTURE_TYPE_SUBPASS_END_INFO:
      return sizeof(VkSubpassEndInfo);
    case VK_STRUCTURE_TYPE_SUBPASS_RESOLVE_PERFORMANCE_QUERY_EXT:
      return sizeof(VkSubpassResolvePerformanceQueryEXT);
    case VK_STRUCTURE_TYPE_SUBPASS_SHADING_PIPELINE_CREATE_INFO_HUAWEI:
      return sizeof(VkSubpassShadingPipelineCreateInfoHUAWEI);
    case VK_STRUCTURE_TYPE_SUBRESOURCE_HOST_MEMCPY_SIZE:
      return sizeof(VkSubresourceHostMemcpySize);
    case VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2:
      return sizeof(VkSubresourceLayout2);
    case VK_STRUCTURE_TYPE_SUBSAMPLED_IMAGE_FORMAT_PROPERTIES_EXT:
      return sizeof(VkSubsampledImageFormatPropertiesEXT);
    case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT:
      return sizeof(VkSurfaceCapabilities2EXT);
    case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR:
      return sizeof(VkSurfaceCapabilities2KHR);
    case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT:
      return sizeof(VkSurfaceCapabilitiesFullScreenExclusiveEXT);
    case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_BARRIER_NV:
      return sizeof(VkSurfaceCapabilitiesPresentBarrierNV);
    case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_ID_2_KHR:
      return sizeof(VkSurfaceCapabilitiesPresentId2KHR);
    case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_WAIT_2_KHR:
      return sizeof(VkSurfaceCapabilitiesPresentWait2KHR);
    case VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR:
      return sizeof(VkSurfaceFormat2KHR);
    case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT:
      return sizeof(VkSurfaceFullScreenExclusiveInfoEXT);
    case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT:
      return sizeof(VkSurfaceFullScreenExclusiveWin32InfoEXT);
    case VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_KHR:
      return sizeof(VkSurfacePresentModeCompatibilityKHR);
    case VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_KHR:
      return sizeof(VkSurfacePresentModeKHR);
    case VK_STRUCTURE_TYPE_SURFACE_PRESENT_SCALING_CAPABILITIES_KHR:
      return sizeof(VkSurfacePresentScalingCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR:
      return sizeof(VkSurfaceProtectedCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_CALIBRATED_TIMESTAMP_INFO_EXT:
      return sizeof(VkSwapchainCalibratedTimestampInfoEXT);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT:
      return sizeof(VkSwapchainCounterCreateInfoEXT);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR:
      return sizeof(VkSwapchainCreateInfoKHR);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD:
      return sizeof(VkSwapchainDisplayNativeHdrCreateInfoAMD);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_FLAGS_SURFACE_CAPABILITIES_EXT:
      return sizeof(VkSwapchainFlagsSurfaceCapabilitiesEXT);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_LATENCY_CREATE_INFO_NV:
      return sizeof(VkSwapchainLatencyCreateInfoNV);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_BARRIER_CREATE_INFO_NV:
      return sizeof(VkSwapchainPresentBarrierCreateInfoNV);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR:
      return sizeof(VkSwapchainPresentFenceInfoKHR);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_KHR:
      return sizeof(VkSwapchainPresentModeInfoKHR);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_KHR:
      return sizeof(VkSwapchainPresentModesCreateInfoKHR);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_KHR:
      return sizeof(VkSwapchainPresentScalingCreateInfoKHR);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_TIME_DOMAIN_PROPERTIES_EXT:
      return sizeof(VkSwapchainTimeDomainPropertiesEXT);
    case VK_STRUCTURE_TYPE_SWAPCHAIN_TIMING_PROPERTIES_EXT:
      return sizeof(VkSwapchainTimingPropertiesEXT);
    case VK_STRUCTURE_TYPE_TENSOR_CAPTURE_DESCRIPTOR_DATA_INFO_ARM:
      return sizeof(VkTensorCaptureDescriptorDataInfoARM);
    case VK_STRUCTURE_TYPE_TENSOR_COPY_ARM:
      return sizeof(VkTensorCopyARM);
    case VK_STRUCTURE_TYPE_TENSOR_CREATE_INFO_ARM:
      return sizeof(VkTensorCreateInfoARM);
    case VK_STRUCTURE_TYPE_TENSOR_DEPENDENCY_INFO_ARM:
      return sizeof(VkTensorDependencyInfoARM);
    case VK_STRUCTURE_TYPE_TENSOR_DESCRIPTION_ARM:
      return sizeof(VkTensorDescriptionARM);
    case VK_STRUCTURE_TYPE_TENSOR_EXPLICIT_TILING_FORMAT_PROPERTIES_ARM:
      return sizeof(VkTensorExplicitTilingFormatPropertiesARM);
    case VK_STRUCTURE_TYPE_TENSOR_FORMAT_PROPERTIES_ARM:
      return sizeof(VkTensorFormatPropertiesARM);
    case VK_STRUCTURE_TYPE_TENSOR_MEMORY_BARRIER_ARM:
      return sizeof(VkTensorMemoryBarrierARM);
    case VK_STRUCTURE_TYPE_TENSOR_MEMORY_REQUIREMENTS_INFO_ARM:
      return sizeof(VkTensorMemoryRequirementsInfoARM);
    case VK_STRUCTURE_TYPE_TENSOR_ROLLING_BACKING_CREATE_INFO_ARM:
      return sizeof(VkTensorRollingBackingCreateInfoARM);
    case VK_STRUCTURE_TYPE_TENSOR_VIEW_CAPTURE_DESCRIPTOR_DATA_INFO_ARM:
      return sizeof(VkTensorViewCaptureDescriptorDataInfoARM);
    case VK_STRUCTURE_TYPE_TENSOR_VIEW_CREATE_INFO_ARM:
      return sizeof(VkTensorViewCreateInfoARM);
    case VK_STRUCTURE_TYPE_TEXEL_BUFFER_DESCRIPTOR_INFO_EXT:
      return sizeof(VkTexelBufferDescriptorInfoEXT);
    case VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD:
      return sizeof(VkTextureLODGatherFormatPropertiesAMD);
    case VK_STRUCTURE_TYPE_THROTTLE_HINT_SUBMIT_INFO_SEC:
      return sizeof(VkThrottleHintSubmitInfoSEC);
    case VK_STRUCTURE_TYPE_TILE_MEMORY_BIND_INFO_QCOM:
      return sizeof(VkTileMemoryBindInfoQCOM);
    case VK_STRUCTURE_TYPE_TILE_MEMORY_REQUIREMENTS_QCOM:
      return sizeof(VkTileMemoryRequirementsQCOM);
    case VK_STRUCTURE_TYPE_TILE_MEMORY_SIZE_INFO_QCOM:
      return sizeof(VkTileMemorySizeInfoQCOM);
    case VK_STRUCTURE_TYPE_TILE_PROPERTIES_QCOM:
      return sizeof(VkTilePropertiesQCOM);
    case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO:
      return sizeof(VkTimelineSemaphoreSubmitInfo);
    case VK_STRUCTURE_TYPE_VALIDATION_CACHE_CREATE_INFO_EXT:
      return sizeof(VkValidationCacheCreateInfoEXT);
    case VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT:
      return sizeof(VkValidationFeaturesEXT);
    case VK_STRUCTURE_TYPE_VALIDATION_FLAGS_EXT:
      return sizeof(VkValidationFlagsEXT);
    case VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT:
      return sizeof(VkVertexInputAttributeDescription2EXT);
    case VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT:
      return sizeof(VkVertexInputBindingDescription2EXT);
    case VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR:
      return sizeof(VkVideoBeginCodingInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR:
      return sizeof(VkVideoCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR:
      return sizeof(VkVideoCodingControlInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR:
      return sizeof(VkVideoDecodeAV1CapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_DPB_SLOT_INFO_KHR:
      return sizeof(VkVideoDecodeAV1DpbSlotInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_INLINE_SESSION_PARAMETERS_INFO_KHR:
      return sizeof(VkVideoDecodeAV1InlineSessionParametersInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PICTURE_INFO_KHR:
      return sizeof(VkVideoDecodeAV1PictureInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR:
      return sizeof(VkVideoDecodeAV1ProfileInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR:
      return sizeof(VkVideoDecodeAV1SessionParametersCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR:
      return sizeof(VkVideoDecodeCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR:
      return sizeof(VkVideoDecodeH264CapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR:
      return sizeof(VkVideoDecodeH264DpbSlotInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_INLINE_SESSION_PARAMETERS_INFO_KHR:
      return sizeof(VkVideoDecodeH264InlineSessionParametersInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR:
      return sizeof(VkVideoDecodeH264PictureInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR:
      return sizeof(VkVideoDecodeH264ProfileInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR:
      return sizeof(VkVideoDecodeH264SessionParametersAddInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR:
      return sizeof(VkVideoDecodeH264SessionParametersCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR:
      return sizeof(VkVideoDecodeH265CapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR:
      return sizeof(VkVideoDecodeH265DpbSlotInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_INLINE_SESSION_PARAMETERS_INFO_KHR:
      return sizeof(VkVideoDecodeH265InlineSessionParametersInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR:
      return sizeof(VkVideoDecodeH265PictureInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR:
      return sizeof(VkVideoDecodeH265ProfileInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR:
      return sizeof(VkVideoDecodeH265SessionParametersAddInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR:
      return sizeof(VkVideoDecodeH265SessionParametersCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR:
      return sizeof(VkVideoDecodeInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_USAGE_INFO_KHR:
      return sizeof(VkVideoDecodeUsageInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_CAPABILITIES_KHR:
      return sizeof(VkVideoDecodeVP9CapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_PICTURE_INFO_KHR:
      return sizeof(VkVideoDecodeVP9PictureInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_PROFILE_INFO_KHR:
      return sizeof(VkVideoDecodeVP9ProfileInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeAV1CapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_DPB_SLOT_INFO_KHR:
      return sizeof(VkVideoEncodeAV1DpbSlotInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_GOP_REMAINING_FRAME_INFO_KHR:
      return sizeof(VkVideoEncodeAV1GopRemainingFrameInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_PICTURE_INFO_KHR:
      return sizeof(VkVideoEncodeAV1PictureInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_PROFILE_INFO_KHR:
      return sizeof(VkVideoEncodeAV1ProfileInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_QUALITY_LEVEL_PROPERTIES_KHR:
      return sizeof(VkVideoEncodeAV1QualityLevelPropertiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_QUANTIZATION_MAP_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeAV1QuantizationMapCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_RATE_CONTROL_INFO_KHR:
      return sizeof(VkVideoEncodeAV1RateControlInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_RATE_CONTROL_LAYER_INFO_KHR:
      return sizeof(VkVideoEncodeAV1RateControlLayerInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_SESSION_CREATE_INFO_KHR:
      return sizeof(VkVideoEncodeAV1SessionCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR:
      return sizeof(VkVideoEncodeAV1SessionParametersCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_FEEDBACK_2_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeFeedback2CapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeH264CapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_DPB_SLOT_INFO_KHR:
      return sizeof(VkVideoEncodeH264DpbSlotInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_GOP_REMAINING_FRAME_INFO_KHR:
      return sizeof(VkVideoEncodeH264GopRemainingFrameInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_NALU_SLICE_INFO_KHR:
      return sizeof(VkVideoEncodeH264NaluSliceInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PICTURE_INFO_KHR:
      return sizeof(VkVideoEncodeH264PictureInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR:
      return sizeof(VkVideoEncodeH264ProfileInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_QUALITY_LEVEL_PROPERTIES_KHR:
      return sizeof(VkVideoEncodeH264QualityLevelPropertiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_QUANTIZATION_MAP_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeH264QuantizationMapCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_KHR:
      return sizeof(VkVideoEncodeH264RateControlInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_LAYER_INFO_KHR:
      return sizeof(VkVideoEncodeH264RateControlLayerInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_CREATE_INFO_KHR:
      return sizeof(VkVideoEncodeH264SessionCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR:
      return sizeof(VkVideoEncodeH264SessionParametersAddInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR:
      return sizeof(VkVideoEncodeH264SessionParametersCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_FEEDBACK_INFO_KHR:
      return sizeof(VkVideoEncodeH264SessionParametersFeedbackInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_GET_INFO_KHR:
      return sizeof(VkVideoEncodeH264SessionParametersGetInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeH265CapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_DPB_SLOT_INFO_KHR:
      return sizeof(VkVideoEncodeH265DpbSlotInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_GOP_REMAINING_FRAME_INFO_KHR:
      return sizeof(VkVideoEncodeH265GopRemainingFrameInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_NALU_SLICE_SEGMENT_INFO_KHR:
      return sizeof(VkVideoEncodeH265NaluSliceSegmentInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PICTURE_INFO_KHR:
      return sizeof(VkVideoEncodeH265PictureInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR:
      return sizeof(VkVideoEncodeH265ProfileInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_QUALITY_LEVEL_PROPERTIES_KHR:
      return sizeof(VkVideoEncodeH265QualityLevelPropertiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_QUANTIZATION_MAP_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeH265QuantizationMapCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_INFO_KHR:
      return sizeof(VkVideoEncodeH265RateControlInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_LAYER_INFO_KHR:
      return sizeof(VkVideoEncodeH265RateControlLayerInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_CREATE_INFO_KHR:
      return sizeof(VkVideoEncodeH265SessionCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR:
      return sizeof(VkVideoEncodeH265SessionParametersAddInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR:
      return sizeof(VkVideoEncodeH265SessionParametersCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_FEEDBACK_INFO_KHR:
      return sizeof(VkVideoEncodeH265SessionParametersFeedbackInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_GET_INFO_KHR:
      return sizeof(VkVideoEncodeH265SessionParametersGetInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR:
      return sizeof(VkVideoEncodeInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_INTRA_REFRESH_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeIntraRefreshCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_INTRA_REFRESH_INFO_KHR:
      return sizeof(VkVideoEncodeIntraRefreshInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_PROFILE_RGB_CONVERSION_INFO_VALVE:
      return sizeof(VkVideoEncodeProfileRgbConversionInfoVALVE);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR:
      return sizeof(VkVideoEncodeQualityLevelInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_PROPERTIES_KHR:
      return sizeof(VkVideoEncodeQualityLevelPropertiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUANTIZATION_MAP_CAPABILITIES_KHR:
      return sizeof(VkVideoEncodeQuantizationMapCapabilitiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUANTIZATION_MAP_INFO_KHR:
      return sizeof(VkVideoEncodeQuantizationMapInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUANTIZATION_MAP_SESSION_PARAMETERS_CREATE_INFO_KHR:
      return sizeof(VkVideoEncodeQuantizationMapSessionParametersCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR:
      return sizeof(VkVideoEncodeRateControlInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_LAYER_INFO_KHR:
      return sizeof(VkVideoEncodeRateControlLayerInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_RGB_CONVERSION_CAPABILITIES_VALVE:
      return sizeof(VkVideoEncodeRgbConversionCapabilitiesVALVE);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_INTRA_REFRESH_CREATE_INFO_KHR:
      return sizeof(VkVideoEncodeSessionIntraRefreshCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_FEEDBACK_INFO_KHR:
      return sizeof(VkVideoEncodeSessionParametersFeedbackInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR:
      return sizeof(VkVideoEncodeSessionParametersGetInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_RGB_CONVERSION_CREATE_INFO_VALVE:
      return sizeof(VkVideoEncodeSessionRgbConversionCreateInfoVALVE);
    case VK_STRUCTURE_TYPE_VIDEO_ENCODE_USAGE_INFO_KHR:
      return sizeof(VkVideoEncodeUsageInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR:
      return sizeof(VkVideoEndCodingInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_FORMAT_AV1_QUANTIZATION_MAP_PROPERTIES_KHR:
      return sizeof(VkVideoFormatAV1QuantizationMapPropertiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_FORMAT_H265_QUANTIZATION_MAP_PROPERTIES_KHR:
      return sizeof(VkVideoFormatH265QuantizationMapPropertiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR:
      return sizeof(VkVideoFormatPropertiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_FORMAT_QUANTIZATION_MAP_PROPERTIES_KHR:
      return sizeof(VkVideoFormatQuantizationMapPropertiesKHR);
    case VK_STRUCTURE_TYPE_VIDEO_INLINE_QUERY_INFO_KHR:
      return sizeof(VkVideoInlineQueryInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR:
      return sizeof(VkVideoPictureResourceInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR:
      return sizeof(VkVideoProfileInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR:
      return sizeof(VkVideoProfileListInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_REFERENCE_INTRA_REFRESH_INFO_KHR:
      return sizeof(VkVideoReferenceIntraRefreshInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR:
      return sizeof(VkVideoReferenceSlotInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR:
      return sizeof(VkVideoSessionCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR:
      return sizeof(VkVideoSessionMemoryRequirementsKHR);
    case VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR:
      return sizeof(VkVideoSessionParametersCreateInfoKHR);
    case VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_UPDATE_INFO_KHR:
      return sizeof(VkVideoSessionParametersUpdateInfoKHR);
    case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR:
      return sizeof(VkWin32KeyedMutexAcquireReleaseInfoKHR);
    case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV:
      return sizeof(VkWin32KeyedMutexAcquireReleaseInfoNV);
    case VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR:
      return sizeof(VkWin32SurfaceCreateInfoKHR);
    case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
      return sizeof(VkWriteDescriptorSet);
    case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR:
      return sizeof(VkWriteDescriptorSetAccelerationStructureKHR);
    case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV:
      return sizeof(VkWriteDescriptorSetAccelerationStructureNV);
    case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK:
      return sizeof(VkWriteDescriptorSetInlineUniformBlock);
    case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_PARTITIONED_ACCELERATION_STRUCTURE_NV:
      return sizeof(VkWriteDescriptorSetPartitionedAccelerationStructureNV);
    case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM:
      return sizeof(VkWriteDescriptorSetTensorARM);
    case VK_STRUCTURE_TYPE_WRITE_INDIRECT_EXECUTION_SET_PIPELINE_EXT:
      return sizeof(VkWriteIndirectExecutionSetPipelineEXT);
    case VK_STRUCTURE_TYPE_WRITE_INDIRECT_EXECUTION_SET_SHADER_EXT:
      return sizeof(VkWriteIndirectExecutionSetShaderEXT);
    default:
      return 0;
  }
}

const char* const kLibvulkanExports[kLibvulkanExportCount] = {
    "vkAcquireNextImage2KHR",
    "vkAcquireNextImageKHR",
    "vkAllocateCommandBuffers",
    "vkAllocateDescriptorSets",
    "vkAllocateMemory",
    "vkBeginCommandBuffer",
    "vkBindBufferMemory",
    "vkBindBufferMemory2",
    "vkBindImageMemory",
    "vkBindImageMemory2",
    "vkCmdBeginQuery",
    "vkCmdBeginRenderPass",
    "vkCmdBeginRenderPass2",
    "vkCmdBeginRendering",
    "vkCmdBindDescriptorSets",
    "vkCmdBindDescriptorSets2",
    "vkCmdBindIndexBuffer",
    "vkCmdBindIndexBuffer2",
    "vkCmdBindPipeline",
    "vkCmdBindVertexBuffers",
    "vkCmdBindVertexBuffers2",
    "vkCmdBlitImage",
    "vkCmdBlitImage2",
    "vkCmdClearAttachments",
    "vkCmdClearColorImage",
    "vkCmdClearDepthStencilImage",
    "vkCmdCopyBuffer",
    "vkCmdCopyBuffer2",
    "vkCmdCopyBufferToImage",
    "vkCmdCopyBufferToImage2",
    "vkCmdCopyImage",
    "vkCmdCopyImage2",
    "vkCmdCopyImageToBuffer",
    "vkCmdCopyImageToBuffer2",
    "vkCmdCopyQueryPoolResults",
    "vkCmdDispatch",
    "vkCmdDispatchBase",
    "vkCmdDispatchIndirect",
    "vkCmdDraw",
    "vkCmdDrawIndexed",
    "vkCmdDrawIndexedIndirect",
    "vkCmdDrawIndexedIndirectCount",
    "vkCmdDrawIndirect",
    "vkCmdDrawIndirectCount",
    "vkCmdEndQuery",
    "vkCmdEndRenderPass",
    "vkCmdEndRenderPass2",
    "vkCmdEndRendering",
    "vkCmdExecuteCommands",
    "vkCmdFillBuffer",
    "vkCmdNextSubpass",
    "vkCmdNextSubpass2",
    "vkCmdPipelineBarrier",
    "vkCmdPipelineBarrier2",
    "vkCmdPushConstants",
    "vkCmdPushConstants2",
    "vkCmdPushDescriptorSet",
    "vkCmdPushDescriptorSet2",
    "vkCmdPushDescriptorSetWithTemplate",
    "vkCmdPushDescriptorSetWithTemplate2",
    "vkCmdResetEvent",
    "vkCmdResetEvent2",
    "vkCmdResetQueryPool",
    "vkCmdResolveImage",
    "vkCmdResolveImage2",
    "vkCmdSetBlendConstants",
    "vkCmdSetCullMode",
    "vkCmdSetDepthBias",
    "vkCmdSetDepthBiasEnable",
    "vkCmdSetDepthBounds",
    "vkCmdSetDepthBoundsTestEnable",
    "vkCmdSetDepthCompareOp",
    "vkCmdSetDepthTestEnable",
    "vkCmdSetDepthWriteEnable",
    "vkCmdSetDeviceMask",
    "vkCmdSetEvent",
    "vkCmdSetEvent2",
    "vkCmdSetFrontFace",
    "vkCmdSetLineStipple",
    "vkCmdSetLineWidth",
    "vkCmdSetPrimitiveRestartEnable",
    "vkCmdSetPrimitiveTopology",
    "vkCmdSetRasterizerDiscardEnable",
    "vkCmdSetRenderingAttachmentLocations",
    "vkCmdSetRenderingInputAttachmentIndices",
    "vkCmdSetScissor",
    "vkCmdSetScissorWithCount",
    "vkCmdSetStencilCompareMask",
    "vkCmdSetStencilOp",
    "vkCmdSetStencilReference",
    "vkCmdSetStencilTestEnable",
    "vkCmdSetStencilWriteMask",
    "vkCmdSetViewport",
    "vkCmdSetViewportWithCount",
    "vkCmdUpdateBuffer",
    "vkCmdWaitEvents",
    "vkCmdWaitEvents2",
    "vkCmdWriteTimestamp",
    "vkCmdWriteTimestamp2",
    "vkCopyImageToMemory",
    "vkCopyMemoryToImage",
    "vkCreateAndroidSurfaceKHR",
    "vkCreateBuffer",
    "vkCreateBufferView",
    "vkCreateCommandPool",
    "vkCreateComputePipelines",
    "vkCreateDescriptorPool",
    "vkCreateDescriptorSetLayout",
    "vkCreateDescriptorUpdateTemplate",
    "vkCreateDevice",
    "vkCreateEvent",
    "vkCreateFence",
    "vkCreateFramebuffer",
    "vkCreateGraphicsPipelines",
    "vkCreateImage",
    "vkCreateImageView",
    "vkCreateInstance",
    "vkCreatePipelineCache",
    "vkCreatePipelineLayout",
    "vkCreatePrivateDataSlot",
    "vkCreateQueryPool",
    "vkCreateRenderPass",
    "vkCreateRenderPass2",
    "vkCreateSampler",
    "vkCreateSamplerYcbcrConversion",
    "vkCreateSemaphore",
    "vkCreateShaderModule",
    "vkCreateSwapchainKHR",
    "vkDestroyBuffer",
    "vkDestroyBufferView",
    "vkDestroyCommandPool",
    "vkDestroyDescriptorPool",
    "vkDestroyDescriptorSetLayout",
    "vkDestroyDescriptorUpdateTemplate",
    "vkDestroyDevice",
    "vkDestroyEvent",
    "vkDestroyFence",
    "vkDestroyFramebuffer",
    "vkDestroyImage",
    "vkDestroyImageView",
    "vkDestroyInstance",
    "vkDestroyPipeline",
    "vkDestroyPipelineCache",
    "vkDestroyPipelineLayout",
    "vkDestroyPrivateDataSlot",
    "vkDestroyQueryPool",
    "vkDestroyRenderPass",
    "vkDestroySampler",
    "vkDestroySamplerYcbcrConversion",
    "vkDestroySemaphore",
    "vkDestroyShaderModule",
    "vkDestroySurfaceKHR",
    "vkDestroySwapchainKHR",
    "vkDeviceWaitIdle",
    "vkEndCommandBuffer",
    "vkEnumerateDeviceExtensionProperties",
    "vkEnumerateDeviceLayerProperties",
    "vkEnumerateInstanceExtensionProperties",
    "vkEnumerateInstanceLayerProperties",
    "vkEnumerateInstanceVersion",
    "vkEnumeratePhysicalDeviceGroups",
    "vkEnumeratePhysicalDevices",
    "vkFlushMappedMemoryRanges",
    "vkFreeCommandBuffers",
    "vkFreeDescriptorSets",
    "vkFreeMemory",
    "vkGetAndroidHardwareBufferPropertiesANDROID",
    "vkGetBufferDeviceAddress",
    "vkGetBufferMemoryRequirements",
    "vkGetBufferMemoryRequirements2",
    "vkGetBufferOpaqueCaptureAddress",
    "vkGetDescriptorSetLayoutSupport",
    "vkGetDeviceBufferMemoryRequirements",
    "vkGetDeviceGroupPeerMemoryFeatures",
    "vkGetDeviceGroupPresentCapabilitiesKHR",
    "vkGetDeviceGroupSurfacePresentModesKHR",
    "vkGetDeviceImageMemoryRequirements",
    "vkGetDeviceImageSparseMemoryRequirements",
    "vkGetDeviceImageSubresourceLayout",
    "vkGetDeviceMemoryCommitment",
    "vkGetDeviceMemoryOpaqueCaptureAddress",
    "vkGetDeviceProcAddr",
    "vkGetDeviceQueue",
    "vkGetDeviceQueue2",
    "vkGetEventStatus",
    "vkGetFenceStatus",
    "vkGetImageMemoryRequirements",
    "vkGetImageMemoryRequirements2",
    "vkGetImageSparseMemoryRequirements",
    "vkGetImageSparseMemoryRequirements2",
    "vkGetImageSubresourceLayout",
    "vkGetImageSubresourceLayout2",
    "vkGetImageSubresourceLayout2EXT",
    "vkGetInstanceProcAddr",
    "vkGetMemoryAndroidHardwareBufferANDROID",
    "vkGetPhysicalDeviceExternalBufferProperties",
    "vkGetPhysicalDeviceExternalFenceProperties",
    "vkGetPhysicalDeviceExternalSemaphoreProperties",
    "vkGetPhysicalDeviceFeatures",
    "vkGetPhysicalDeviceFeatures2",
    "vkGetPhysicalDeviceFormatProperties",
    "vkGetPhysicalDeviceFormatProperties2",
    "vkGetPhysicalDeviceImageFormatProperties",
    "vkGetPhysicalDeviceImageFormatProperties2",
    "vkGetPhysicalDeviceMemoryProperties",
    "vkGetPhysicalDeviceMemoryProperties2",
    "vkGetPhysicalDevicePresentRectanglesKHR",
    "vkGetPhysicalDeviceProperties",
    "vkGetPhysicalDeviceProperties2",
    "vkGetPhysicalDeviceQueueFamilyProperties",
    "vkGetPhysicalDeviceQueueFamilyProperties2",
    "vkGetPhysicalDeviceSparseImageFormatProperties",
    "vkGetPhysicalDeviceSparseImageFormatProperties2",
    "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
    "vkGetPhysicalDeviceSurfaceFormatsKHR",
    "vkGetPhysicalDeviceSurfacePresentModesKHR",
    "vkGetPhysicalDeviceSurfaceSupportKHR",
    "vkGetPhysicalDeviceToolProperties",
    "vkGetPipelineCacheData",
    "vkGetPrivateData",
    "vkGetQueryPoolResults",
    "vkGetRenderAreaGranularity",
    "vkGetRenderingAreaGranularity",
    "vkGetSemaphoreCounterValue",
    "vkGetSwapchainImagesKHR",
    "vkInvalidateMappedMemoryRanges",
    "vkMapMemory",
    "vkMapMemory2",
    "vkMergePipelineCaches",
    "vkQueueBindSparse",
    "vkQueuePresentKHR",
    "vkQueueSubmit",
    "vkQueueSubmit2",
    "vkQueueWaitIdle",
    "vkResetCommandBuffer",
    "vkResetCommandPool",
    "vkResetDescriptorPool",
    "vkResetEvent",
    "vkResetFences",
    "vkResetQueryPool",
    "vkSetEvent",
    "vkSetPrivateData",
    "vkSignalSemaphore",
    "vkTransitionImageLayout",
    "vkTrimCommandPool",
    "vkUnmapMemory",
    "vkUnmapMemory2",
    "vkUpdateDescriptorSetWithTemplate",
    "vkUpdateDescriptorSets",
    "vkWaitForFences",
    "vkWaitSemaphores",
};

const char* const kGuestExtensions[kGuestExtensionCount] = {
    "VK_AMDX_dense_geometry_format",
    "VK_AMDX_shader_enqueue",
    "VK_AMD_anti_lag",
    "VK_AMD_buffer_marker",
    "VK_AMD_device_coherent_memory",
    "VK_AMD_display_native_hdr",
    "VK_AMD_draw_indirect_count",
    "VK_AMD_gcn_shader",
    "VK_AMD_gpa_interface",
    "VK_AMD_gpu_shader_half_float",
    "VK_AMD_gpu_shader_int16",
    "VK_AMD_memory_overallocation_behavior",
    "VK_AMD_mixed_attachment_samples",
    "VK_AMD_negative_viewport_height",
    "VK_AMD_pipeline_compiler_control",
    "VK_AMD_rasterization_order",
    "VK_AMD_shader_ballot",
    "VK_AMD_shader_core_properties",
    "VK_AMD_shader_core_properties2",
    "VK_AMD_shader_early_and_late_fragment_tests",
    "VK_AMD_shader_explicit_vertex_parameter",
    "VK_AMD_shader_fragment_mask",
    "VK_AMD_shader_image_load_store_lod",
    "VK_AMD_shader_info",
    "VK_AMD_shader_trinary_minmax",
    "VK_AMD_texture_gather_bias_lod",
    "VK_ARM_data_graph",
    "VK_ARM_data_graph_instruction_set_tosa",
    "VK_ARM_data_graph_neural_accelerator_statistics",
    "VK_ARM_data_graph_optical_flow",
    "VK_ARM_format_pack",
    "VK_ARM_performance_counters_by_region",
    "VK_ARM_pipeline_opacity_micromap",
    "VK_ARM_rasterization_order_attachment_access",
    "VK_ARM_render_pass_striped",
    "VK_ARM_scheduling_controls",
    "VK_ARM_shader_core_builtins",
    "VK_ARM_shader_core_properties",
    "VK_ARM_shader_instrumentation",
    "VK_ARM_tensor_controls",
    "VK_ARM_tensors",
    "VK_EXT_4444_formats",
    "VK_EXT_acquire_drm_display",
    "VK_EXT_astc_decode_mode",
    "VK_EXT_attachment_feedback_loop_dynamic_state",
    "VK_EXT_attachment_feedback_loop_layout",
    "VK_EXT_blend_operation_advanced",
    "VK_EXT_border_color_swizzle",
    "VK_EXT_buffer_device_address",
    "VK_EXT_calibrated_timestamps",
    "VK_EXT_color_write_enable",
    "VK_EXT_conditional_rendering",
    "VK_EXT_conservative_rasterization",
    "VK_EXT_custom_border_color",
    "VK_EXT_custom_resolve",
    "VK_EXT_debug_marker",
    "VK_EXT_debug_report",
    "VK_EXT_debug_utils",
    "VK_EXT_depth_bias_control",
    "VK_EXT_depth_clamp_control",
    "VK_EXT_depth_clamp_zero_one",
    "VK_EXT_depth_clip_control",
    "VK_EXT_depth_clip_enable",
    "VK_EXT_depth_range_unrestricted",
    "VK_EXT_descriptor_buffer",
    "VK_EXT_descriptor_heap",
    "VK_EXT_descriptor_indexing",
    "VK_EXT_device_address_binding_report",
    "VK_EXT_device_fault",
    "VK_EXT_device_generated_commands",
    "VK_EXT_device_memory_report",
    "VK_EXT_direct_mode_display",
    "VK_EXT_discard_rectangles",
    "VK_EXT_display_control",
    "VK_EXT_display_surface_counter",
    "VK_EXT_dynamic_rendering_unused_attachments",
    "VK_EXT_extended_dynamic_state",
    "VK_EXT_extended_dynamic_state2",
    "VK_EXT_extended_dynamic_state3",
    "VK_EXT_external_memory_acquire_unmodified",
    "VK_EXT_external_memory_dma_buf",
    "VK_EXT_external_memory_host",
    "VK_EXT_filter_cubic",
    "VK_EXT_fragment_density_map",
    "VK_EXT_fragment_density_map2",
    "VK_EXT_fragment_density_map_offset",
    "VK_EXT_fragment_shader_interlock",
    "VK_EXT_frame_boundary",
    "VK_EXT_global_priority",
    "VK_EXT_global_priority_query",
    "VK_EXT_graphics_pipeline_library",
    "VK_EXT_hdr_metadata",
    "VK_EXT_headless_surface",
    "VK_EXT_host_image_copy",
    "VK_EXT_host_query_reset",
    "VK_EXT_image_2d_view_of_3d",
    "VK_EXT_image_compression_control",
    "VK_EXT_image_compression_control_swapchain",
    "VK_EXT_image_drm_format_modifier",
    "VK_EXT_image_robustness",
    "VK_EXT_image_sliced_view_of_3d",
    "VK_EXT_image_view_min_lod",
    "VK_EXT_index_type_uint8",
    "VK_EXT_inline_uniform_block",
    "VK_EXT_layer_settings",
    "VK_EXT_legacy_dithering",
    "VK_EXT_legacy_vertex_attributes",
    "VK_EXT_line_rasterization",
    "VK_EXT_load_store_op_none",
    "VK_EXT_map_memory_placed",
    "VK_EXT_memory_budget",
    "VK_EXT_memory_decompression",
    "VK_EXT_memory_priority",
    "VK_EXT_mesh_shader",
    "VK_EXT_multi_draw",
    "VK_EXT_multisampled_render_to_single_sampled",
    "VK_EXT_multisampled_render_to_swapchain",
    "VK_EXT_mutable_descriptor_type",
    "VK_EXT_nested_command_buffer",
    "VK_EXT_non_seamless_cube_map",
    "VK_EXT_opacity_micromap",
    "VK_EXT_pageable_device_local_memory",
    "VK_EXT_pci_bus_info",
    "VK_EXT_physical_device_drm",
    "VK_EXT_pipeline_creation_cache_control",
    "VK_EXT_pipeline_creation_feedback",
    "VK_EXT_pipeline_library_group_handles",
    "VK_EXT_pipeline_properties",
    "VK_EXT_pipeline_protected_access",
    "VK_EXT_pipeline_robustness",
    "VK_EXT_post_depth_coverage",
    "VK_EXT_present_mode_fifo_latest_ready",
    "VK_EXT_present_timing",
    "VK_EXT_primitive_restart_index",
    "VK_EXT_primitive_topology_list_restart",
    "VK_EXT_primitives_generated_query",
    "VK_EXT_private_data",
    "VK_EXT_provoking_vertex",
    "VK_EXT_queue_family_foreign",
    "VK_EXT_rasterization_order_attachment_access",
    "VK_EXT_ray_tracing_invocation_reorder",
    "VK_EXT_rgba10x6_formats",
    "VK_EXT_robustness2",
    "VK_EXT_sample_locations",
    "VK_EXT_sampler_filter_minmax",
    "VK_EXT_scalar_block_layout",
    "VK_EXT_separate_stencil_usage",
    "VK_EXT_shader_64bit_indexing",
    "VK_EXT_shader_atomic_float",
    "VK_EXT_shader_atomic_float2",
    "VK_EXT_shader_demote_to_helper_invocation",
    "VK_EXT_shader_float8",
    "VK_EXT_shader_image_atomic_int64",
    "VK_EXT_shader_long_vector",
    "VK_EXT_shader_module_identifier",
    "VK_EXT_shader_object",
    "VK_EXT_shader_ocp_microscaling_types",
    "VK_EXT_shader_replicated_composites",
    "VK_EXT_shader_split_barrier",
    "VK_EXT_shader_stencil_export",
    "VK_EXT_shader_subgroup_ballot",
    "VK_EXT_shader_subgroup_partitioned",
    "VK_EXT_shader_subgroup_vote",
    "VK_EXT_shader_tile_image",
    "VK_EXT_shader_uniform_buffer_unsized_array",
    "VK_EXT_shader_viewport_index_layer",
    "VK_EXT_subgroup_size_control",
    "VK_EXT_subpass_merge_feedback",
    "VK_EXT_surface_maintenance1",
    "VK_EXT_swapchain_colorspace",
    "VK_EXT_swapchain_maintenance1",
    "VK_EXT_texel_buffer_alignment",
    "VK_EXT_texture_compression_astc_3d",
    "VK_EXT_texture_compression_astc_hdr",
    "VK_EXT_tooling_info",
    "VK_EXT_transform_feedback",
    "VK_EXT_validation_cache",
    "VK_EXT_validation_features",
    "VK_EXT_validation_flags",
    "VK_EXT_vertex_attribute_divisor",
    "VK_EXT_vertex_attribute_robustness",
    "VK_EXT_vertex_input_dynamic_state",
    "VK_EXT_ycbcr_2plane_444_formats",
    "VK_EXT_ycbcr_image_arrays",
    "VK_EXT_zero_initialize_device_memory",
    "VK_GOOGLE_decorate_string",
    "VK_GOOGLE_display_timing",
    "VK_GOOGLE_hlsl_functionality1",
    "VK_GOOGLE_surfaceless_query",
    "VK_GOOGLE_user_type",
    "VK_HUAWEI_cluster_culling_shader",
    "VK_HUAWEI_hdr_vivid",
    "VK_HUAWEI_invocation_mask",
    "VK_HUAWEI_subpass_shading",
    "VK_IMG_filter_cubic",
    "VK_IMG_filter_linear_2d",
    "VK_IMG_format_pvrtc",
    "VK_IMG_relaxed_line_rasterization",
    "VK_INTEL_performance_query",
    "VK_INTEL_shader_integer_functions2",
    "VK_KHR_16bit_storage",
    "VK_KHR_8bit_storage",
    "VK_KHR_acceleration_structure",
    "VK_KHR_bind_memory2",
    "VK_KHR_buffer_device_address",
    "VK_KHR_calibrated_timestamps",
    "VK_KHR_compute_shader_derivatives",
    "VK_KHR_cooperative_matrix",
    "VK_KHR_copy_commands2",
    "VK_KHR_copy_memory_indirect",
    "VK_KHR_create_renderpass2",
    "VK_KHR_dedicated_allocation",
    "VK_KHR_deferred_host_operations",
    "VK_KHR_depth_clamp_zero_one",
    "VK_KHR_depth_stencil_resolve",
    "VK_KHR_descriptor_update_template",
    "VK_KHR_device_address_commands",
    "VK_KHR_device_fault",
    "VK_KHR_device_group",
    "VK_KHR_device_group_creation",
    "VK_KHR_display",
    "VK_KHR_display_swapchain",
    "VK_KHR_draw_indirect_count",
    "VK_KHR_driver_properties",
    "VK_KHR_dynamic_rendering",
    "VK_KHR_dynamic_rendering_local_read",
    "VK_KHR_extended_flags",
    "VK_KHR_external_fence",
    "VK_KHR_external_fence_capabilities",
    "VK_KHR_external_fence_fd",
    "VK_KHR_external_memory",
    "VK_KHR_external_memory_capabilities",
    "VK_KHR_external_memory_fd",
    "VK_KHR_external_semaphore",
    "VK_KHR_external_semaphore_capabilities",
    "VK_KHR_external_semaphore_fd",
    "VK_KHR_format_feature_flags2",
    "VK_KHR_fragment_shader_barycentric",
    "VK_KHR_fragment_shading_rate",
    "VK_KHR_get_display_properties2",
    "VK_KHR_get_memory_requirements2",
    "VK_KHR_get_physical_device_properties2",
    "VK_KHR_get_surface_capabilities2",
    "VK_KHR_global_priority",
    "VK_KHR_image_format_list",
    "VK_KHR_imageless_framebuffer",
    "VK_KHR_incremental_present",
    "VK_KHR_index_type_uint8",
    "VK_KHR_internally_synchronized_queues",
    "VK_KHR_line_rasterization",
    "VK_KHR_load_store_op_none",
    "VK_KHR_maintenance1",
    "VK_KHR_maintenance10",
    "VK_KHR_maintenance11",
    "VK_KHR_maintenance2",
    "VK_KHR_maintenance3",
    "VK_KHR_maintenance4",
    "VK_KHR_maintenance5",
    "VK_KHR_maintenance6",
    "VK_KHR_maintenance7",
    "VK_KHR_maintenance8",
    "VK_KHR_maintenance9",
    "VK_KHR_map_memory2",
    "VK_KHR_multiview",
    "VK_KHR_opacity_micromap",
    "VK_KHR_performance_query",
    "VK_KHR_pipeline_binary",
    "VK_KHR_pipeline_executable_properties",
    "VK_KHR_pipeline_library",
    "VK_KHR_portability_enumeration",
    "VK_KHR_portability_subset",
    "VK_KHR_present_id",
    "VK_KHR_present_id2",
    "VK_KHR_present_mode_fifo_latest_ready",
    "VK_KHR_present_wait",
    "VK_KHR_present_wait2",
    "VK_KHR_push_descriptor",
    "VK_KHR_ray_query",
    "VK_KHR_ray_tracing_maintenance1",
    "VK_KHR_ray_tracing_pipeline",
    "VK_KHR_ray_tracing_position_fetch",
    "VK_KHR_relaxed_block_layout",
    "VK_KHR_robustness2",
    "VK_KHR_sampler_mirror_clamp_to_edge",
    "VK_KHR_sampler_ycbcr_conversion",
    "VK_KHR_separate_depth_stencil_layouts",
    "VK_KHR_shader_abort",
    "VK_KHR_shader_atomic_int64",
    "VK_KHR_shader_bfloat16",
    "VK_KHR_shader_clock",
    "VK_KHR_shader_constant_data",
    "VK_KHR_shader_draw_parameters",
    "VK_KHR_shader_expect_assume",
    "VK_KHR_shader_float16_int8",
    "VK_KHR_shader_float_controls",
    "VK_KHR_shader_float_controls2",
    "VK_KHR_shader_fma",
    "VK_KHR_shader_integer_dot_product",
    "VK_KHR_shader_maximal_reconvergence",
    "VK_KHR_shader_non_semantic_info",
    "VK_KHR_shader_quad_control",
    "VK_KHR_shader_relaxed_extended_instruction",
    "VK_KHR_shader_subgroup_extended_types",
    "VK_KHR_shader_subgroup_rotate",
    "VK_KHR_shader_subgroup_uniform_control_flow",
    "VK_KHR_shader_terminate_invocation",
    "VK_KHR_shader_untyped_pointers",
    "VK_KHR_shared_presentable_image",
    "VK_KHR_spirv_1_4",
    "VK_KHR_storage_buffer_storage_class",
    "VK_KHR_surface",
    "VK_KHR_surface_maintenance1",
    "VK_KHR_surface_protected_capabilities",
    "VK_KHR_swapchain",
    "VK_KHR_swapchain_maintenance1",
    "VK_KHR_swapchain_mutable_format",
    "VK_KHR_synchronization2",
    "VK_KHR_timeline_semaphore",
    "VK_KHR_unified_image_layouts",
    "VK_KHR_uniform_buffer_standard_layout",
    "VK_KHR_variable_pointers",
    "VK_KHR_vertex_attribute_divisor",
    "VK_KHR_video_decode_av1",
    "VK_KHR_video_decode_h264",
    "VK_KHR_video_decode_h265",
    "VK_KHR_video_decode_queue",
    "VK_KHR_video_decode_vp9",
    "VK_KHR_video_encode_av1",
    "VK_KHR_video_encode_feedback2",
    "VK_KHR_video_encode_h264",
    "VK_KHR_video_encode_h265",
    "VK_KHR_video_encode_intra_refresh",
    "VK_KHR_video_encode_quantization_map",
    "VK_KHR_video_encode_queue",
    "VK_KHR_video_maintenance1",
    "VK_KHR_video_maintenance2",
    "VK_KHR_video_queue",
    "VK_KHR_vulkan_memory_model",
    "VK_KHR_workgroup_memory_explicit_layout",
    "VK_KHR_zero_initialize_workgroup_memory",
    "VK_LUNARG_direct_driver_loading",
    "VK_MESA_image_alignment_control",
    "VK_MSFT_layered_driver",
    "VK_NVX_binary_import",
    "VK_NVX_image_view_handle",
    "VK_NVX_multiview_per_view_attributes",
    "VK_NV_clip_space_w_scaling",
    "VK_NV_cluster_acceleration_structure",
    "VK_NV_command_buffer_inheritance",
    "VK_NV_compute_occupancy_priority",
    "VK_NV_compute_shader_derivatives",
    "VK_NV_cooperative_matrix",
    "VK_NV_cooperative_matrix2",
    "VK_NV_cooperative_matrix_decode_vector",
    "VK_NV_cooperative_vector",
    "VK_NV_copy_memory_indirect",
    "VK_NV_corner_sampled_image",
    "VK_NV_coverage_reduction_mode",
    "VK_NV_cuda_kernel_launch",
    "VK_NV_dedicated_allocation",
    "VK_NV_dedicated_allocation_image_aliasing",
    "VK_NV_descriptor_pool_overallocation",
    "VK_NV_device_diagnostic_checkpoints",
    "VK_NV_device_diagnostics_config",
    "VK_NV_device_generated_commands",
    "VK_NV_device_generated_commands_compute",
    "VK_NV_displacement_micromap",
    "VK_NV_display_stereo",
    "VK_NV_extended_sparse_address_space",
    "VK_NV_external_compute_queue",
    "VK_NV_external_memory",
    "VK_NV_external_memory_capabilities",
    "VK_NV_external_memory_rdma",
    "VK_NV_fill_rectangle",
    "VK_NV_fragment_coverage_to_color",
    "VK_NV_fragment_shader_barycentric",
    "VK_NV_fragment_shading_rate_enums",
    "VK_NV_framebuffer_mixed_samples",
    "VK_NV_geometry_shader_passthrough",
    "VK_NV_glsl_shader",
    "VK_NV_inherited_viewport_scissor",
    "VK_NV_linear_color_attachment",
    "VK_NV_low_latency",
    "VK_NV_low_latency2",
    "VK_NV_memory_decompression",
    "VK_NV_mesh_shader",
    "VK_NV_optical_flow",
    "VK_NV_partitioned_acceleration_structure",
    "VK_NV_per_stage_descriptor_set",
    "VK_NV_present_barrier",
    "VK_NV_present_metering",
    "VK_NV_push_constant_bank",
    "VK_NV_raw_access_chains",
    "VK_NV_ray_tracing",
    "VK_NV_ray_tracing_invocation_reorder",
    "VK_NV_ray_tracing_linear_swept_spheres",
    "VK_NV_ray_tracing_motion_blur",
    "VK_NV_ray_tracing_validation",
    "VK_NV_representative_fragment_test",
    "VK_NV_sample_mask_override_coverage",
    "VK_NV_scissor_exclusive",
    "VK_NV_shader_atomic_float16_vector",
    "VK_NV_shader_image_footprint",
    "VK_NV_shader_sm_builtins",
    "VK_NV_shader_subgroup_partitioned",
    "VK_NV_shading_rate_image",
    "VK_NV_viewport_array2",
    "VK_NV_viewport_swizzle",
    "VK_QCOM_cooperative_matrix_conversion",
    "VK_QCOM_data_graph_model",
    "VK_QCOM_elapsed_timer_query",
    "VK_QCOM_filter_cubic_clamp",
    "VK_QCOM_filter_cubic_weights",
    "VK_QCOM_fragment_density_map_offset",
    "VK_QCOM_image_processing",
    "VK_QCOM_image_processing2",
    "VK_QCOM_image_processing3",
    "VK_QCOM_multiview_per_view_render_areas",
    "VK_QCOM_multiview_per_view_viewports",
    "VK_QCOM_queue_perf_hint",
    "VK_QCOM_render_pass_shader_resolve",
    "VK_QCOM_render_pass_store_ops",
    "VK_QCOM_render_pass_transform",
    "VK_QCOM_rotated_copy_commands",
    "VK_QCOM_shader_multiple_wait_queues",
    "VK_QCOM_tile_memory_heap",
    "VK_QCOM_tile_properties",
    "VK_QCOM_tile_shading",
    "VK_QCOM_ycbcr_degamma",
    "VK_SEC_amigo_profiling",
    "VK_SEC_pipeline_cache_incremental_mode",
    "VK_SEC_throttle_hint",
    "VK_VALVE_descriptor_set_host_mapping",
    "VK_VALVE_fragment_density_map_layered",
    "VK_VALVE_mutable_descriptor_type",
    "VK_VALVE_shader_mixed_float_dot_product",
    "VK_VALVE_video_encode_rgb_conversion",
};

bool IsGuestExtension(std::string_view name) noexcept {
  std::size_t low = 0;
  std::size_t high = kGuestExtensionCount;
  while (low < high) {
    const std::size_t mid = low + (high - low) / 2;
    const int order = std::string_view(kGuestExtensions[mid]).compare(name);
    if (order == 0) return true;
    if (order < 0) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return false;
}

const char* const kHostOnlyExtensions[kHostOnlyExtensionCount] = {
    "VK_EXT_full_screen_exclusive",
    "VK_KHR_external_fence_win32",
    "VK_KHR_external_memory_win32",
    "VK_KHR_external_semaphore_win32",
    "VK_KHR_win32_keyed_mutex",
    "VK_KHR_win32_surface",
    "VK_NV_acquire_winrt_display",
    "VK_NV_external_memory_win32",
    "VK_NV_win32_keyed_mutex",
};

const char* const kAndroidExtensions[kAndroidExtensionCount] = {
    "VK_ANDROID_external_format_resolve",
    "VK_ANDROID_external_memory_android_hardware_buffer",
    "VK_ANDROID_native_buffer",
    "VK_KHR_android_surface",
};

}
