#pragma once

#include "pch/Pch.h"
#include "rhi/RHI.h"

class RHI_Vulkan; // fwd decl for deferred destruction / bindless slot recycling

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

    // Deferred destruction + bindless slot recycling (set by the creating RHI).
    // ownsImage textures get their image/view/alloc retired to the owner's
    // per-frame garbage bin; backbuffers leave this null so they are untouched.
    RHI_Vulkan* ownerRHI = nullptr;
    bool ownsImage = false;
    bool hasBindlessSlot = false;
    ~VKTexture();
};

struct VKPipeline : public RHIPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
    bool isSkinned = false;
    bool usesBindlessTextures = false;
};

