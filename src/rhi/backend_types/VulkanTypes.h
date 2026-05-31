#pragma once

#include "pch/Pch.h"
#include "rhi/RHI.h"

struct VKBuffer : public RHIBuffer {
    VkBuffer buf = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    void* mapped = nullptr;

    size_t size = 0;
    size_t sizePerFrame = 0; // Ensures safe double buffering by tracking per-frame block size
    VkDescriptorBufferInfo bufferInfo = {};
};

struct VKTexture : public RHITexture {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;

    VkDescriptorImageInfo imageInfo = {};
    VkDescriptorImageInfo uavInfo = {};
    VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;
};

struct VKPipeline : public RHIPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
    bool isSkinned = false;
};


