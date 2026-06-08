#include "rhi/RHI_Vulkan.h"
#include "engine/EngineDependencies.h"
#include "engine/terrain/TerrainTypes.h"
#include "rhi/utils/ShaderCompiler.h"
VKAPI_ATTR VkBool32 VKAPI_CALL RHI_Vulkan::VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        OutputDebugStringA("\n=================================================\n");
        OutputDebugStringA("[VULKAN VALIDATION LAYER]:\n");
        OutputDebugStringA(pCallbackData->pMessage);
        OutputDebugStringA(
            "\n=================================================\n\n");
    }
    return VK_FALSE;
}

void RHI_Vulkan::SingleTimeCommand(
    std::function<void(VkCommandBuffer)> action) {
    VkCommandBufferAllocateInfo allocInfo = {
                                             VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo = {
                                          VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    action(cmd);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
}

std::shared_ptr<RHITexture> RHI_Vulkan::CreateDummyTexture(bool isCube) {
    auto tex = std::make_shared<VKTexture>();
    tex->width = 1;
    tex->height = 1;

    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {1, 1, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = isCube ? 6 : 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (isCube) {
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &tex->image, &tex->alloc,
                   nullptr);

    uint32_t pixels[6] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                          0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    auto stagingBuffer =
        CreateBuffer(BufferType::Constant, pixels, isCube ? 24 : 4);

    SingleTimeCommand([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = tex->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = isCube ? 6 : 1;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);

        VkBufferImageCopy copyRegion = {};
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = isCube ? 6 : 1;
        copyRegion.imageExtent = {1, 1, 1};
        vkCmdCopyBufferToImage(
            cmd, static_cast<VKBuffer *>(stagingBuffer.get())->buf, tex->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &barrier);
    });

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = tex->image;
    viewInfo.viewType = isCube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = isCube ? 6 : 1;
    vkCreateImageView(device, &viewInfo, nullptr, &tex->view);

    tex->imageInfo = {mainSampler, tex->view,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    tex->uavInfo = {mainSampler, tex->view, VK_IMAGE_LAYOUT_GENERAL};
    return tex;
}

static bool VKIsBlockCompressed(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_BC1_UNORM || format == DXGI_FORMAT_BC3_UNORM;
}

static uint32_t VKBytesPerBlock(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R16_UNORM:
        return 2;
    case DXGI_FORMAT_BC1_UNORM:
        return 8;
    case DXGI_FORMAT_BC3_UNORM:
        return 16;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    default:
        return 4;
    }
}

static VkFormat VKFormatFromDXGI(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R16_UNORM:
        return VK_FORMAT_R16_UNORM;
    case DXGI_FORMAT_BC1_UNORM:
        return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case DXGI_FORMAT_BC3_UNORM:
        return VK_FORMAT_BC3_UNORM_BLOCK;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    default:
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

static void VKMipLayout(DXGI_FORMAT format, uint32_t width, uint32_t height,
                        uint32_t &rowBytes, uint32_t &rowCount) {
    if (VKIsBlockCompressed(format)) {
        rowBytes = std::max<uint32_t>(1, (width + 3) / 4) * VKBytesPerBlock(format);
        rowCount = std::max<uint32_t>(1, (height + 3) / 4);
    } else {
        rowBytes = width * VKBytesPerBlock(format);
        rowCount = height;
    }
}

void RHI_Vulkan::RegisterBindlessTexture(VKTexture* tex) {
    if (!tex)
        return;

    if (m_bindlessImageInfos.empty()) {
        VkDescriptorImageInfo dummy = tex->imageInfo;
        if (m_dummy2D) {
            dummy = static_cast<VKTexture*>(m_dummy2D.get())->imageInfo;
        }
        m_bindlessImageInfos.resize(RHI_BINDLESS_TEXTURE_CAPACITY, dummy);
    }

    uint32_t idx;
    if (!m_freeBindlessSlots.empty()) {
        // Reuse a slot freed by a destroyed (e.g. evicted terrain) texture so
        // the bindless index pool does not grow without bound.
        idx = m_freeBindlessSlots.back();
        m_freeBindlessSlots.pop_back();
    } else {
        if (m_bindlessTextureCount >= RHI_BINDLESS_TEXTURE_CAPACITY)
            return;
        idx = m_bindlessTextureCount++;
    }

    tex->bindlessIndex = idx;
    m_bindlessImageInfos[idx] = tex->imageInfo;
    tex->ownerRHI = this;
    tex->hasBindlessSlot = true;
}

void RHI_Vulkan::FreeBindlessSlot(uint32_t index) {
    // Defer recycling until this frame slot cycles back (GPU done with it).
    m_deferredBindlessFree[currentFrame].push_back(index);
    // Point the descriptor entry at the dummy so nothing samples a stale view.
    if (index < m_bindlessImageInfos.size() && m_dummy2D)
        m_bindlessImageInfos[index] =
            static_cast<VKTexture *>(m_dummy2D.get())->imageInfo;
}

void RHI_Vulkan::RetireVKTexture(VKTexture *t) {
    if (t->hasBindlessSlot)
        FreeBindlessSlot(t->bindlessIndex);
    if (t->ownsImage)
        m_imageGarbage[currentFrame].push_back({t->image, t->view, t->alloc});
}

RHI_Vulkan::UploadAlloc RHI_Vulkan::AllocUpload(VkDeviceSize size,
                                                VkDeviceSize align) {
    VkUploadHeap &h = m_uploadHeaps[currentFrame];
    VkDeviceSize aligned = (h.cursor + (align - 1)) & ~(align - 1);
    if (aligned + size > h.cap)
        aligned = 0; // wrap (heap is sized for a full frame's uploads)
    h.cursor = aligned + size;
    return {h.buf, h.cpu + aligned, aligned};
}

void RHI_Vulkan::OpenUploadCmd() {
    if (uploadOpen)
        return;
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(uploadCmds[currentFrame], &bi); // implicit reset (pool has RESET bit)
    uploadOpen = true;
}

VKTexture::~VKTexture() {
    if (ownerRHI)
        ownerRHI->RetireVKTexture(this);
}

void RHI_Vulkan::CleanupSwapchain() {
    vkDeviceWaitIdle(device);

    for (auto &pair : fbCache) {
        vkDestroyFramebuffer(device, pair.second, nullptr);
    }
    fbCache.clear();

    vkDestroyImageView(device, depthView, nullptr);
    vmaDestroyImage(allocator, depthImage, depthAlloc);

    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    m_backBuffers.clear();
}

void RHI_Vulkan::CreateSwapchainResources(bool vsyncEnabled) {
    currentVsyncState = vsyncEnabled;

    // Fetch surface capabilities
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &caps);

    // Handle window minimization and DPI scaling
    if (caps.currentExtent.width != 0xFFFFFFFF) {
        width = caps.currentExtent.width;
        height = caps.currentExtent.height;
    } else {
        width = std::max(caps.minImageExtent.width,
                         std::min(caps.maxImageExtent.width, width));
        height = std::max(caps.minImageExtent.height,
                          std::min(caps.maxImageExtent.height, height));
    }

    // Determine presentation mode (VSync)
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface,
                                              &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physDevice, surface, &presentModeCount, presentModes.data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // Default VSync
    if (!vsyncEnabled) {
        for (const auto &mode : presentModes) {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                break;
            }
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            }
        }
    }

    // Create Swapchain
    VkSwapchainCreateInfoKHR swapchainInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount =
        std::max(caps.minImageCount, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT));
    swapchainInfo.imageFormat = swapchainFormat;
    swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainInfo.imageExtent = {width, height};
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;

    vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);

    // Create Main Depth Buffer
    VkImageCreateInfo depthImageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.format = VK_FORMAT_D32_SFLOAT;
    depthImageInfo.extent = {width, height, 1};
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = 1;
    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo depthAllocInfo = {};
    depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(allocator, &depthImageInfo, &depthAllocInfo, &depthImage,
                   &depthAlloc, nullptr);

    VkImageViewCreateInfo depthViewInfo = {
                                           VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthViewInfo.image = depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &depthViewInfo, nullptr, &depthView);

    // Fetch backbuffers from swapchain
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    std::vector<VkImage> scImages(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, scImages.data());

    swapchainImageViews.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = scImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]);

        auto tex = std::make_shared<VKTexture>();
        tex->image = scImages[i];
        tex->view = swapchainImageViews[i];
        tex->imageInfo.imageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        m_backBuffers.push_back(tex);
    }
}

void RHI_Vulkan::BindGlobalDescriptors() {
    if (!currentPipeline || !currentUBO || !needsDescriptorUpdate)
        return;

    VkDescriptorSetAllocateInfo allocInfo = {
                                             VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descPools[currentFrame];
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &currentPipeline->descLayout;

    VkDescriptorSet newSet;
    if (vkAllocateDescriptorSets(device, &allocInfo, &newSet) != VK_SUCCESS)
        return;

    std::vector<VkWriteDescriptorSet> writes;

    // 1. Global Uniforms (Camera, Lights, SSAOParams)
    // OPRAVA: M�sto natvrdo zadan�ho sizeof(GlobalData) te� pou�ijeme dynamickou
    // lastUBOSize!
    VkDescriptorBufferInfo bInfo = {currentUBO->buf, 0, lastUBOSize};
    writes.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, newSet, 0,
                      0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr,
                      &bInfo, nullptr});

    // 2. Object Specific Uniforms
    VkDescriptorBufferInfo objInfo = {
                                      m_currentObjectBuffer ? m_currentObjectBuffer->buf
                              : static_cast<VKBuffer *>(m_dummyUBO.get())->buf,
        0, sizeof(SkinnedObjectData)};
    writes.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, newSet, 1,
                      0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr,
                      &objInfo, nullptr});

    // 3. Skinned Animation Bones
    VkDescriptorBufferInfo boneInfo = {
                                       m_currentBoneBuffer ? m_currentBoneBuffer->buf
                            : static_cast<VKBuffer *>(m_dummyUBO.get())->buf,
        0, sizeof(SM::Matrix) * MAX_BONES};
    writes.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, newSet, 2,
                      0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr,
                      &boneInfo, nullptr});

    // 4. Textures
    VkDescriptorImageInfo imgInfos[8];
    if (currentPipeline->usesBindlessTextures) {
        if (m_bindlessImageInfos.empty()) {
            VkDescriptorImageInfo dummy =
                static_cast<VKTexture *>(m_dummy2D.get())->imageInfo;
            m_bindlessImageInfos.resize(RHI_BINDLESS_TEXTURE_CAPACITY, dummy);
        }
        writes.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, newSet,
                          3, 0, RHI_BINDLESS_TEXTURE_CAPACITY,
                          VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                          m_bindlessImageInfos.data(), nullptr, nullptr});
    } else {
        for (int i = 0; i < 8; i++) {
            if (currentTextures[i]) {
                imgInfos[i] = currentTextures[i]->imageInfo;
            } else {
                imgInfos[i] = static_cast<VKTexture *>(m_dummy2D.get())->imageInfo;
            }
            writes.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, newSet,
                              static_cast<uint32_t>(3 + i), 0, 1,
                              VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imgInfos[i], nullptr,
                              nullptr});
        }
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);

    std::vector<uint32_t> dynOffsets = {lastUBOOffset};
    dynOffsets.push_back(m_currentObjectBuffer ? m_currentObjectOffset : 0);
    dynOffsets.push_back(m_currentBoneBuffer ? m_currentBoneOffset : 0);

    vkCmdBindDescriptorSets(
        cmdBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
        currentPipeline->layout, 0, 1, &newSet,
        static_cast<uint32_t>(dynOffsets.size()), dynOffsets.data());
    needsDescriptorUpdate = false;
}

RHI_Vulkan::~RHI_Vulkan() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

#ifdef _DEBUG
    if (debugMessenger != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
        auto destroyDebugFunc =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyDebugFunc) {
            destroyDebugFunc(instance, debugMessenger, nullptr);
        }
    }
#endif
    // Safely destroy sync primitives
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (imageAvailableSemaphores[i])
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        if (inFlightFences[i])
            vkDestroyFence(device, inFlightFences[i], nullptr);
        if (descPools[i])
            vkDestroyDescriptorPool(device, descPools[i], nullptr);
    }

    for (int i = 0; i < 8; i++) {
        if (renderFinishedSemaphores[i])
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
    }

    // Drain deferred GPU resources (GPU is idle after the vkDeviceWaitIdle above)
    for (auto &bin : m_imageGarbage) {
        for (auto &r : bin) {
            if (r.view)
                vkDestroyImageView(device, r.view, nullptr);
            if (r.image)
                vmaDestroyImage(allocator, r.image, r.alloc);
        }
        bin.clear();
    }
    for (auto &uh : m_uploadHeaps) {
        if (uh.buf)
            vmaDestroyBuffer(allocator, uh.buf, uh.alloc);
    }
    for (auto s : uploadSemaphores) {
        if (s)
            vkDestroySemaphore(device, s, nullptr);
    }
}

bool RHI_Vulkan::Init(HWND hWnd, int w, int h) {
    width = w;
    height = h;

    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "TinyGine Vulkan App";
    appInfo.apiVersion = VK_API_VERSION_1_1;

    std::vector<const char *> extensions = {"VK_KHR_surface",
                                            "VK_KHR_win32_surface"};
    std::vector<const char *> layers;

#ifdef _DEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    VkInstanceCreateInfo instanceInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.data();
    instanceInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instanceInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
        return false;
    }

#ifdef _DEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = {
                                                    VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = VulkanDebugCallback;

    auto createDebugFunc = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createDebugFunc) {
        createDebugFunc(instance, &debugInfo, nullptr, &debugMessenger);
    }
#endif

    VkWin32SurfaceCreateInfoKHR surfaceInfo = {
                                               VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surfaceInfo.hinstance = GetModuleHandle(NULL);
    surfaceInfo.hwnd = hWnd;
    vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface);

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> gpus(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, gpus.data());
    physDevice = gpus[0];

    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(physDevice, &deviceProps);
    minUboAlignment = deviceProps.limits.minUniformBufferOffsetAlignment;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {
                                         VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures features = {};
    features.samplerAnisotropy = VK_TRUE;
    features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    features.textureCompressionBC = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexing = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
    descriptorIndexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    const char *deviceExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                VK_KHR_MAINTENANCE1_EXTENSION_NAME,
                                VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME};

    VkDeviceCreateInfo deviceInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = 3;
    deviceInfo.ppEnabledExtensionNames = deviceExts;
    deviceInfo.pEnabledFeatures = &features;
    deviceInfo.pNext = &descriptorIndexing;

    vkCreateDevice(physDevice, &deviceInfo, nullptr, &device);
    vkGetDeviceQueue(device, 0, 0, &graphicsQueue);

    VmaAllocatorCreateInfo vmaInfo = {};
    vmaInfo.physicalDevice = physDevice;
    vmaInfo.device = device;
    vmaInfo.instance = instance;
    vmaCreateAllocator(&vmaInfo, &allocator);

    // --- SUBPASS DEPENDENCIES ---
    VkSubpassDependency deps[2] = {};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    // --- 1. MAIN PASS (UI and Final Blit) ---
    VkAttachmentDescription mainAtt[1] = {};
    mainAtt[0].format = swapchainFormat;
    mainAtt[0].samples = VK_SAMPLE_COUNT_1_BIT;
    mainAtt[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    mainAtt[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    mainAtt[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    mainAtt[0].finalLayout =
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Ready for monitor presentation

    VkAttachmentReference mainColorRef = {
                                          0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription mainSubpass = {};
    mainSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    mainSubpass.colorAttachmentCount = 1;
    mainSubpass.pColorAttachments = &mainColorRef;
    mainSubpass.pDepthStencilAttachment = nullptr; // UI doesn't need depth

    VkRenderPassCreateInfo mainPassInfo = {
                                           VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    mainPassInfo.attachmentCount = 1;
    mainPassInfo.pAttachments = mainAtt;
    mainPassInfo.subpassCount = 1;
    mainPassInfo.pSubpasses = &mainSubpass;
    mainPassInfo.dependencyCount = 2;
    mainPassInfo.pDependencies = deps;

    vkCreateRenderPass(device, &mainPassInfo, nullptr, &mainRenderPass);

    // --- 2. MRT PASS (3D G-Buffer) ---
    VkAttachmentDescription mrtAtts[4] = {};
    for (int i = 0; i < 3; i++) {
        mrtAtts[i].samples = VK_SAMPLE_COUNT_1_BIT;
        mrtAtts[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        mrtAtts[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        mrtAtts[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        mrtAtts[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    mrtAtts[0].format = swapchainFormat;
    mrtAtts[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    mrtAtts[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;

    // MRT Depth Attachment
    mrtAtts[3].format = VK_FORMAT_D32_SFLOAT;
    mrtAtts[3].samples = VK_SAMPLE_COUNT_1_BIT;
    mrtAtts[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    mrtAtts[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    mrtAtts[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    mrtAtts[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference mrtColorRefs[3] = {
                                             {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
    VkAttachmentReference mrtDepthRef = {
                                         3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription mrtSubpass = {};
    mrtSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    mrtSubpass.colorAttachmentCount = 3;
    mrtSubpass.pColorAttachments = mrtColorRefs;
    mrtSubpass.pDepthStencilAttachment = &mrtDepthRef;

    VkRenderPassCreateInfo mrtPassInfo = {
                                          VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    mrtPassInfo.attachmentCount = 4;
    mrtPassInfo.pAttachments = mrtAtts;
    mrtPassInfo.subpassCount = 1;
    mrtPassInfo.pSubpasses = &mrtSubpass;
    mrtPassInfo.dependencyCount = 2;
    mrtPassInfo.pDependencies = deps;

    vkCreateRenderPass(device, &mrtPassInfo, nullptr, &mrtRenderPass);

    // --- 3. SHADOW PASS (Directional Light Depth) ---
    VkAttachmentDescription shadowAtt = {};
    shadowAtt.format = VK_FORMAT_D32_SFLOAT;
    shadowAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    shadowAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    shadowAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    shadowAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    shadowAtt.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference shadowDepthRef = {
                                            0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription shadowSubpass = {};
    shadowSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    shadowSubpass.pDepthStencilAttachment = &shadowDepthRef;

    VkRenderPassCreateInfo shadowPassInfo = {
                                             VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    shadowPassInfo.attachmentCount = 1;
    shadowPassInfo.pAttachments = &shadowAtt;
    shadowPassInfo.subpassCount = 1;
    shadowPassInfo.pSubpasses = &shadowSubpass;
    shadowPassInfo.dependencyCount = 2;
    shadowPassInfo.pDependencies = deps;

    vkCreateRenderPass(device, &shadowPassInfo, nullptr, &shadowRenderPass);

    // --- 4. OFFSCREEN PASS (Post-Process Ping-Pong) ---
    VkAttachmentDescription offscreenAtt = {};
    offscreenAtt.format = swapchainFormat;
    offscreenAtt.samples = VK_SAMPLE_COUNT_1_BIT;

    // fix 1 - without this, the first post-process pass would clear to black
    offscreenAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    offscreenAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	// fix 2 - without this, the first post-process pass would clear to black instead of loading the bloom texture's contents
    offscreenAtt.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 
    offscreenAtt.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference offscreenColorRef = {
                                               0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription offscreenSubpass = {};
    offscreenSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    offscreenSubpass.colorAttachmentCount = 1;
    offscreenSubpass.pColorAttachments = &offscreenColorRef;
    offscreenSubpass.pDepthStencilAttachment =
        nullptr; // No depth needed for bloom/blur

    VkRenderPassCreateInfo offscreenPassInfo = {
                                                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    offscreenPassInfo.attachmentCount = 1;
    offscreenPassInfo.pAttachments = &offscreenAtt;
    offscreenPassInfo.subpassCount = 1;
    offscreenPassInfo.pSubpasses = &offscreenSubpass;
    offscreenPassInfo.dependencyCount = 2;
    offscreenPassInfo.pDependencies = deps;

    vkCreateRenderPass(device, &offscreenPassInfo, nullptr,
                       &singleOffscreenRenderPass);

    // Allocate Command Buffers
    VkCommandPoolCreateInfo poolInfo = {
                                        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device, &poolInfo, nullptr, &cmdPool);

    CreateSwapchainResources(false);

    cmdBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo cmdAllocInfo = {
                                                VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAllocInfo.commandPool = cmdPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, cmdBuffers.data());

    // Create Descriptor Pools (One for each frame to avoid in-use overrides)
    descPools.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorPoolSize poolSizes[] = {
                                            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 5000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, RHI_BINDLESS_TEXTURE_CAPACITY * 4},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10000},
            {VK_DESCRIPTOR_TYPE_SAMPLER, 2000}};

        VkDescriptorPoolCreateInfo descPoolInfo = {
                                                   VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descPoolInfo.maxSets = 5000;
        descPoolInfo.poolSizeCount = 5;
        descPoolInfo.pPoolSizes = poolSizes;

        vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descPools[i]);
    }

    // --- GLOBAL SAMPLERS ---
    VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    // REPEAT: needed by tiled model textures (e.g. the floor). The terrain
    // avoids the wrap at tile edges by clamping its UV inside the shader.
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxLod = 12.0f;
    vkCreateSampler(device, &samplerInfo, nullptr, &mainSampler);

    VkSamplerCreateInfo shadowSamplerInfo = {
                                             VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    shadowSamplerInfo.magFilter = VK_FILTER_LINEAR;
    shadowSamplerInfo.minFilter = VK_FILTER_LINEAR;
    shadowSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    shadowSamplerInfo.compareEnable = VK_TRUE;
    shadowSamplerInfo.compareOp = VK_COMPARE_OP_LESS;
    vkCreateSampler(device, &shadowSamplerInfo, nullptr, &shadowSampler);

    // --- SYNCHRONIZATION ---
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(
        8); // Ensures a safe mapping to physical swapchain images

    VkSemaphoreCreateInfo semInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphores[i]);
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]);
    }
    for (int i = 0; i < 8; i++) {
        vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphores[i]);
    }

    // --- ASYNC STREAMING UPLOAD: per-frame ring staging + upload cmd buffers ---
    m_deferredBindlessFree.resize(MAX_FRAMES_IN_FLIGHT);
    m_imageGarbage.resize(MAX_FRAMES_IN_FLIGHT);

    uploadCmds.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo upAlloc = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    upAlloc.commandPool = cmdPool;
    upAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    upAlloc.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    vkAllocateCommandBuffers(device, &upAlloc, uploadCmds.data());

    uploadSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    {
        VkSemaphoreCreateInfo si = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            vkCreateSemaphore(device, &si, nullptr, &uploadSemaphores[i]);
    }

    m_uploadHeaps.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkUploadHeap &uh = m_uploadHeaps[i];
        uh.cap = 128ull * 1024 * 1024; // 128 MB per frame
        uh.cursor = 0;
        VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = uh.cap;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo aci = {};
        aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        vmaCreateBuffer(allocator, &bci, &aci, &uh.buf, &uh.alloc, nullptr);
        VmaAllocationInfo ai;
        vmaGetAllocationInfo(allocator, uh.alloc, &ai);
        uh.cpu = static_cast<uint8_t *>(ai.pMappedData);
    }

    // Initialize dummy fallbacks
    std::vector<uint8_t> dummyData(sizeof(SM::Matrix) * MAX_BONES, 0);
    m_dummyUBO =
        CreateBuffer(BufferType::Constant, dummyData.data(), dummyData.size());
    m_dummy2D = CreateDummyTexture(false);
    m_dummyCube = CreateDummyTexture(true);

    std::vector<uint8_t> dummySSBOData(256, 0);
    m_dummySSBO = CreateBuffer(BufferType::ComputeUAV, dummySSBOData.data(),
                               dummySSBOData.size(), 16);

    // --- COMPUTE SHADER LAYOUT ---
    VkDescriptorSetLayoutBinding compBindings[5] = {};
    compBindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                       VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    compBindings[1] = {15, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                       VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    compBindings[2] = {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1,
                       VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    compBindings[3] = {11, VK_DESCRIPTOR_TYPE_SAMPLER, 1,
                       VK_SHADER_STAGE_COMPUTE_BIT, &mainSampler};
    compBindings[4] = {16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                       VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
                       nullptr};

    VkDescriptorSetLayoutCreateInfo compLayoutInfo = {
                                                      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    compLayoutInfo.bindingCount = 5;
    compLayoutInfo.pBindings = compBindings;

    vkCreateDescriptorSetLayout(device, &compLayoutInfo, nullptr,
                                &computeDescLayout);

    return true;
}

RHITexture *RHI_Vulkan::GetBackBuffer() {
    return m_backBuffers[imageIndex].get();
}

std::shared_ptr<RHITexture> RHI_Vulkan::CreateRenderTarget(int w, int h,
                                                           int format) {
    auto tex = std::make_shared<VKTexture>();
    tex->width = w;
    tex->height = h;

    VkFormat fmt = (format == 0)   ? swapchainFormat
                   : (format == 1) ? VK_FORMAT_R16G16B16A16_SFLOAT
                                   : VK_FORMAT_R32G32B32A32_SFLOAT;

    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = fmt;
    imageInfo.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &tex->image, &tex->alloc,
                   nullptr);

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = tex->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = fmt;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &tex->view);

    tex->imageInfo = {mainSampler, tex->view,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    return tex;
}

std::shared_ptr<RHITexture> RHI_Vulkan::CreateShadowTexture(int w, int h) {
    auto tex = std::make_shared<VKTexture>();
    tex->width = w;
    tex->height = h;

    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.extent.width = w;
    imageInfo.extent.height = h;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &tex->image, &tex->alloc,
                   nullptr);

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = tex->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &tex->view);

    tex->imageInfo = {mainSampler, tex->view,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    return tex;
}

void RHI_Vulkan::SetMainPassTarget() {
    SetMRTTargets({GetBackBuffer()}, nullptr);
}

void RHI_Vulkan::BeginShadowPass(RHITexture *t) {
    if (!frameOk)
        return;
    SetMRTTargets({}, t);
}

void RHI_Vulkan::SetMRTTargets(std::vector<RHITexture *> targets,
                               RHITexture *depthMap) {
    if (isPassRunning) {
        vkCmdEndRenderPass(cmdBuffers[currentFrame]);
        isPassRunning = false;
    }
    if (!frameOk)
        return;

    currentMRT = targets;
    m_currentDepthMap = depthMap;

    VkRenderPass targetRenderPass;

    // Match attachments to proper RenderPass
    if (targets.empty()) {
        targetRenderPass = shadowRenderPass;
    } else if (targets.size() == 3) {
        targetRenderPass = mrtRenderPass;
    } else {
        if (targets[0] == GetBackBuffer()) {
            targetRenderPass = mainRenderPass;
        } else {
            targetRenderPass = singleOffscreenRenderPass;
        }
    }

    // Compute a unique hash to cache Framebuffers
    uint64_t hash = 0;
    for (auto t : targets) {
        hash ^= reinterpret_cast<uint64_t>(static_cast<VKTexture *>(t)->view) +
                0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    if (depthMap) {
        hash ^=
            reinterpret_cast<uint64_t>(static_cast<VKTexture *>(depthMap)->view) +
                0x9e3779b9 + (hash << 6) + (hash >> 2);
    }

    uint32_t passWidth = width;
    uint32_t passHeight = height;
    if (!targets.empty() && targets[0] && targets[0]->width > 0 && targets[0]->height > 0) {
        passWidth = targets[0]->width;
        passHeight = targets[0]->height;
    } else if (targets.empty() && depthMap && depthMap->width > 0 && depthMap->height > 0) {
        passWidth = depthMap->width;
        passHeight = depthMap->height;
    }

    // Create Framebuffer if it doesn't exist
    if (fbCache.find(hash) == fbCache.end()) {
        std::vector<VkImageView> views;

        if (targetRenderPass == shadowRenderPass) {
            views.push_back(depthMap ? static_cast<VKTexture *>(depthMap)->view
                                     : depthView);
        } else if (targetRenderPass == mrtRenderPass) {
            for (auto t : targets) {
                views.push_back(static_cast<VKTexture *>(t)->view);
            }
            views.push_back(depthMap ? static_cast<VKTexture *>(depthMap)->view
                                     : depthView);
        } else {
            // mainRenderPass and singleOffscreenRenderPass have exactly 1 color
            // attachment
            views.push_back(static_cast<VKTexture *>(targets[0])->view);
        }

        VkFramebufferCreateInfo fbInfo = {
                                          VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass = targetRenderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(views.size());
        fbInfo.pAttachments = views.data();
        fbInfo.width = passWidth;
        fbInfo.height = passHeight;
        fbInfo.layers = 1;

        vkCreateFramebuffer(device, &fbInfo, nullptr, &fbCache[hash]);
    }

    std::vector<VkClearValue> clearValues;

    if (targetRenderPass == shadowRenderPass) {
        clearValues.push_back({.depthStencil = {1.0f, 0}});
    } else if (targetRenderPass == mrtRenderPass) {
        clearValues.push_back({.color = {{0.05f, 0.05f, 0.05f, 1.0f}}});
        clearValues.push_back({.color = {{0.0f, 0.0f, 0.0f, 0.0f}}});
        clearValues.push_back({.color = {{0.0f, 0.0f, 0.0f, 0.0f}}});
        clearValues.push_back({.depthStencil = {1.0f, 0}});
    } else {
        clearValues.push_back({.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
    }

    VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass = targetRenderPass;
    rpBegin.framebuffer = fbCache[hash];
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = {passWidth, passHeight};
    rpBegin.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpBegin.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffers[currentFrame], &rpBegin,
                         VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp = {};
    vp.x = 0;
    vp.y = static_cast<float>(passHeight);
    vp.width = static_cast<float>(passWidth);
    vp.height = -static_cast<float>(passHeight);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffers[currentFrame], 0, 1, &vp);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = {passWidth, passHeight};
    vkCmdSetScissor(cmdBuffers[currentFrame], 0, 1, &scissor);

    isPassRunning = true;
}

void RHI_Vulkan::ClearRenderTarget(RHITexture* target, const float color[4]) {
    if (!isPassRunning || !target) return;

    //check if target is in current MRT
    uint32_t attachmentIndex = 0;
    for (size_t i = 0; i < currentMRT.size(); i++) {
        if (currentMRT[i] == target) {
            attachmentIndex = static_cast<uint32_t>(i);
            break;
        }
    }

    VkClearAttachment clearAttachment = {};
    clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachment.colorAttachment = attachmentIndex;
    clearAttachment.clearValue.color = { {color[0], color[1], color[2], color[3]} };

    VkClearRect clearRect = {};
    clearRect.rect.offset = { 0, 0 };
    clearRect.rect.extent = { static_cast<uint32_t>(target->width), static_cast<uint32_t>(target->height) };
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount = 1;

    //manual delete like dx12 DX12
    vkCmdClearAttachments(cmdBuffers[currentFrame], 1, &clearAttachment, 1, &clearRect);
}

void RHI_Vulkan::ClearDepthTarget(RHITexture* dt, float depth, uint8_t stencil) {
    if (!isPassRunning) return;

    VkClearAttachment clearAttachment = {};
    clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clearAttachment.clearValue.depthStencil = { depth, stencil };

    VkClearRect clearRect = {};
    clearRect.rect.offset = { 0, 0 };
    clearRect.rect.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount = 1;

    vkCmdClearAttachments(cmdBuffers[currentFrame], 1, &clearAttachment, 1, &clearRect);
}

void RHI_Vulkan::SetTexture(RHITexture *t, int s) {
    if (s >= 0 && s < 8) {
        currentTextures[s] = static_cast<VKTexture *>(t);
        needsDescriptorUpdate = true;
    }
}

std::vector<uint32_t> RHI_Vulkan::CompileHLSL(const std::wstring &path,
                                              const char *entry,
                                              const char *target) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return {};

    std::vector<char> src((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    file.close();

    ComPtr<IDxcCompiler3> compiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    DxcBuffer srcBuf = {src.data(), src.size(), CP_UTF8};

    wchar_t wEntry[64], wTarget[64];
    MultiByteToWideChar(CP_UTF8, 0, entry, -1, wEntry, 64);
    MultiByteToWideChar(CP_UTF8, 0, target, -1, wTarget, 64);

    std::vector<LPCWSTR> args = {
                                 L"-E",           wEntry,    L"-T",
        wTarget,         L"-spirv", L"-fspv-target-env=vulkan1.1",
        L"-fvk-b-shift", L"0",      L"0",
        L"-fvk-u-shift", L"15",     L"0",
        L"-fvk-t-shift", L"3",      L"0",
        L"-fvk-s-shift", L"11",     L"0",
        L"-D",           L"VULKAN"};

    ComPtr<IDxcResult> res;
    compiler->Compile(&srcBuf, args.data(), static_cast<UINT32>(args.size()),
                      nullptr, IID_PPV_ARGS(&res));

    ComPtr<IDxcBlob> blob;
    res->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr);

    if (!blob)
        return {};

    return std::vector<uint32_t>(
        reinterpret_cast<uint32_t *>(blob->GetBufferPointer()),
        reinterpret_cast<uint32_t *>(blob->GetBufferPointer()) +
            (blob->GetBufferSize() / 4));
}

std::shared_ptr<RHIPipeline>
RHI_Vulkan::CreatePipeline(const PipelineConfig &config) {
    auto p = std::make_shared<VKPipeline>();
    p->isSkinned = config.isSkinned;
    p->usesBindlessTextures = config.useBindlessTextures;

    auto vS = ShaderCompiler::CompileVulkan(config.vsPath, "VSMain", "vs_6_0");
    if (vS.empty()) return p;

    // 2. ZMĚNA: Používáme std::vector<uint8_t> místo uint32_t
    std::vector<uint8_t> pS;
    if (!config.psPath.empty()) {
        pS = ShaderCompiler::CompileVulkan(config.psPath, "PSMain", "ps_6_0");
    }

    std::vector<VkDescriptorSetLayoutBinding> activeBindings;
    if (config.useBindlessTextures) {
        activeBindings.push_back({0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                                  VK_SHADER_STAGE_VERTEX_BIT |
                                      VK_SHADER_STAGE_FRAGMENT_BIT,
                                  nullptr});
        activeBindings.push_back({1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                                  VK_SHADER_STAGE_VERTEX_BIT |
                                      VK_SHADER_STAGE_FRAGMENT_BIT,
                                  nullptr});
        activeBindings.push_back({2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                                  VK_SHADER_STAGE_VERTEX_BIT, nullptr});
        activeBindings.push_back({3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                  RHI_BINDLESS_TEXTURE_CAPACITY,
                                  VK_SHADER_STAGE_VERTEX_BIT |
                                      VK_SHADER_STAGE_FRAGMENT_BIT,
                                  nullptr});
        activeBindings.push_back({11, VK_DESCRIPTOR_TYPE_SAMPLER, 1,
                                  VK_SHADER_STAGE_VERTEX_BIT |
                                      VK_SHADER_STAGE_FRAGMENT_BIT,
                                  &mainSampler});
    } else {
        // Define generic layout mapping for modern PBR engine
        VkDescriptorSetLayoutBinding bindings[13] = {};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       nullptr};
        bindings[1] = {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       nullptr};
        bindings[2] = {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                       VK_SHADER_STAGE_VERTEX_BIT, nullptr};

        for (int i = 0; i <= 7; i++) {
            bindings[3 + i] = {static_cast<uint32_t>(3 + i),
                               VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1,
                               VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        }

        bindings[11] = {11, VK_DESCRIPTOR_TYPE_SAMPLER, 1,
                        VK_SHADER_STAGE_FRAGMENT_BIT, &mainSampler};
        bindings[12] = {12, VK_DESCRIPTOR_TYPE_SAMPLER, 1,
                        VK_SHADER_STAGE_FRAGMENT_BIT, &shadowSampler};

        for (int i = 0; i < 13; i++) {
            activeBindings.push_back(bindings[i]);
        }
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
                                                  VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = static_cast<uint32_t>(activeBindings.size());
    layoutInfo.pBindings = activeBindings.data();
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &p->descLayout);

    // Push constants for rapid per-object updates
    VkPushConstantRange pcr = {};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(ObjectData);

    VkPipelineLayoutCreateInfo pipeLayoutInfo = {
                                                 VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeLayoutInfo.setLayoutCount = 1;
    pipeLayoutInfo.pSetLayouts = &p->descLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges = &pcr;
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &p->layout);

    VkShaderModule vertModule = VK_NULL_HANDLE;
    VkShaderModule fragModule = VK_NULL_HANDLE;

    VkShaderModuleCreateInfo vertModInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    // 3. ZMĚNA: Velikost je nyní v bytech (už ŽÁDNÉ násobení * 4!)
    vertModInfo.codeSize = vS.size();
    // 4. ZMĚNA: Vulkan API stále vyžaduje uint32_t*, takže musíme byty bezpečně přetypovat
    vertModInfo.pCode = reinterpret_cast<const uint32_t*>(vS.data());
    vkCreateShaderModule(device, &vertModInfo, nullptr, &vertModule);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    VkPipelineShaderStageCreateInfo vertStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "VSMain";
    stages.push_back(vertStage);

    if (!pS.empty()) {
        VkShaderModuleCreateInfo fragModInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        fragModInfo.codeSize = pS.size(); // OPĚT: Žádné * 4 !
        fragModInfo.pCode = reinterpret_cast<const uint32_t*>(pS.data()); // Přetypování
        vkCreateShaderModule(device, &fragModInfo, nullptr, &fragModule);

        VkPipelineShaderStageCreateInfo fragStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragModule;
        fragStage.pName = "PSMain";
        stages.push_back(fragStage);
    }
    // Vertex Input Configuration
    std::vector<VkVertexInputBindingDescription> bindDescs;
    std::vector<VkVertexInputAttributeDescription> attrDescs;

    if (config.isTerrain) {
        VkVertexInputBindingDescription terrainBind = {};
        terrainBind.binding = 0;
        terrainBind.stride = sizeof(TerrainVertex);
        terrainBind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindDescs.push_back(terrainBind);

        VkVertexInputBindingDescription instBind = {};
        instBind.binding = 1;
        instBind.stride = sizeof(TerrainInstanceGPU);
        instBind.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        bindDescs.push_back(instBind);

        VkVertexInputAttributeDescription attr = {};
        attr.location = 0;
        attr.binding = 0;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = 0;
        attrDescs.push_back(attr);
        attr.location = 1;
        attr.binding = 0;
        attr.format = VK_FORMAT_R32G32_SFLOAT;
        attr.offset = 12;
        attrDescs.push_back(attr);
        attr.location = 2;
        attr.binding = 1;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = 0;
        attrDescs.push_back(attr);
        attr.location = 3;
        attr.binding = 1;
        attr.format = VK_FORMAT_R32_SFLOAT;
        attr.offset = 12;
        attrDescs.push_back(attr);
        attr.location = 4;
        attr.binding = 1;
        attr.format = VK_FORMAT_R32_UINT;
        attr.offset = 16;
        attrDescs.push_back(attr);
        attr.location = 5;
        attr.binding = 1;
        attr.format = VK_FORMAT_R32_UINT;
        attr.offset = 20;
        attrDescs.push_back(attr);
    } else if (config.vsPath.find(L"fullscreen") == std::wstring::npos) {
        VkVertexInputBindingDescription mainBind = {};
        mainBind.binding = 0;
        mainBind.stride = config.isSkinned ? sizeof(SkinnedVertex) : sizeof(Vertex);
        mainBind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindDescs.push_back(mainBind);

        VkVertexInputAttributeDescription attr = {};

        // Standard attributes
        attr.location = 0;
        attr.binding = 0;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = 0;
        attrDescs.push_back(attr);
        attr.location = 1;
        attr.binding = 0;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = 12;
        attrDescs.push_back(attr);
        attr.location = 2;
        attr.binding = 0;
        attr.format = VK_FORMAT_R32G32_SFLOAT;
        attr.offset = 24;
        attrDescs.push_back(attr);

        // Skinned animation attributes
        if (config.isSkinned) {
            attr.location = 3;
            attr.binding = 0;
            attr.format = VK_FORMAT_R32G32B32A32_SINT;
            attr.offset = 32;
            attrDescs.push_back(attr);
            attr.location = 4;
            attr.binding = 0;
            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr.offset = 48;
            attrDescs.push_back(attr);
        }

        // Instancing attributes (Model matrix spread across 4 locations)
        if (config.useInstancing) {
            VkVertexInputBindingDescription instBind = {};
            instBind.binding = 1;
            instBind.stride = sizeof(ObjectData);
            instBind.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            bindDescs.push_back(instBind);

            attr.location = 3;
            attr.binding = 1;
            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr.offset = 0;
            attrDescs.push_back(attr);
            attr.location = 4;
            attr.binding = 1;
            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr.offset = 16;
            attrDescs.push_back(attr);
            attr.location = 5;
            attr.binding = 1;
            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr.offset = 32;
            attrDescs.push_back(attr);
            attr.location = 6;
            attr.binding = 1;
            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr.offset = 48;
            attrDescs.push_back(attr);
            attr.location = 7;
            attr.binding = 1;
            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr.offset = 64;
            attrDescs.push_back(attr);
            attr.location = 8;
            attr.binding = 1;
            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr.offset = 80;
            attrDescs.push_back(attr);
            attr.location = 9;
            attr.binding = 1;
            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr.offset = 96;
            attrDescs.push_back(attr);
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
                                                            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount =
        static_cast<uint32_t>(bindDescs.size());
    vertexInputInfo.pVertexBindingDescriptions =
        bindDescs.empty() ? nullptr : bindDescs.data();
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attrDescs.size());
    vertexInputInfo.pVertexAttributeDescriptions =
        attrDescs.empty() ? nullptr : attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
                                                            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = config.topology == Topology::LineList
                                 ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                                 : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState = {
                                                       VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {
                                                         VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = config.fillMode == FillMode::Wireframe
                                 ? VK_POLYGON_MODE_LINE
                                 : VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.cullMode = config.cullMode == CullMode::None
                              ? VK_CULL_MODE_NONE
                              : VK_CULL_MODE_BACK_BIT;

    // Depth bias for shadow mapping
    if (pS.empty()) {
        rasterizer.depthBiasEnable = VK_TRUE;
        rasterizer.depthBiasConstantFactor = 1.25f;
        rasterizer.depthBiasSlopeFactor = 1.75f;
    } else {
        rasterizer.depthBiasEnable = VK_FALSE;
    }

    VkPipelineMultisampleStateCreateInfo multisampling = {
                                                          VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
                                                          VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = config.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = config.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    // Blending Configuration
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    if (config.isAdditive) {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    } else if (config.isTransparent) {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    else if (config.enableBlend) {
        // --- NOVÉ (Pro Cloud Upscale) ---
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    else {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }
    colorBlendAttachment.colorWriteMask = 0xF;

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(
        config.numRenderTargets == 0 ? 1 : config.numRenderTargets,
        colorBlendAttachment);

    VkPipelineColorBlendStateCreateInfo colorBlending = {
                                                         VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.logicOpEnable = VK_FALSE;
    if (pS.empty()) {
        colorBlending.attachmentCount = 0;
    } else {
        colorBlending.attachmentCount =
            static_cast<uint32_t>(blendAttachments.size());
        colorBlending.pAttachments = blendAttachments.data();
    }

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
                                                         VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicStateInfo.dynamicStateCount = 2;
    dynamicStateInfo.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {
                                                 VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = p->layout;

    // Match Pipeline precisely to its designated Render Pass
    if (pS.empty()) {
        pipelineInfo.renderPass = shadowRenderPass;
    } else if (config.numRenderTargets == 3) {
        pipelineInfo.renderPass = mrtRenderPass;
    } else if (!config.depthTest) {
        pipelineInfo.renderPass = singleOffscreenRenderPass;
    } else {
        pipelineInfo.renderPass = mainRenderPass;
    }

    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                              &p->pipeline);

    vkDestroyShaderModule(device, vertModule, nullptr);
    if (fragModule) {
        vkDestroyShaderModule(device, fragModule, nullptr);
    }

    return p;
}

void RHI_Vulkan::SetPipeline(RHIPipeline *p) {
    currentPipeline = static_cast<VKPipeline *>(p);
    m_currentPipeline = p;
    if (currentPipeline) {
        vkCmdBindPipeline(cmdBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          currentPipeline->pipeline);
    }
}

void RHI_Vulkan::BeginFrame() {
    // The fence for this frame slot was already waited on (and its resources
    // recycled) at the tail of EndFrame, so streamed uploads recorded during
    // Update wrote into an already-free upload heap. No wait needed here.
    VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                         imageAvailableSemaphores[currentFrame],
                                         VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        CleanupSwapchain();
        CreateSwapchainResources(currentVsyncState);
        frameOk = false;
        return;
    }
    frameOk = true;

    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    vkResetDescriptorPool(device, descPools[currentFrame], 0);
    needsDescriptorUpdate = true;

    // Reset per-frame dynamic buffer offsets
    cOff = 0;
    oOff = 0;
    bOff = 0;
    compOff = 0;

    currentUBO = nullptr;
    m_currentObjectBuffer = nullptr;
    m_currentBoneBuffer = nullptr;

    for (int i = 0; i < 8; i++) {
        currentTextures[i] = nullptr;
    }

    VkCommandBufferBeginInfo beginInfo = {
                                          VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmdBuffers[currentFrame], &beginInfo);
    isPassRunning = false;
}

void RHI_Vulkan::EndFrame() {
    if (!frameOk)
        return;

    if (isPassRunning) {
        vkCmdEndRenderPass(cmdBuffers[currentFrame]);
        isPassRunning = false;
    }

    vkEndCommandBuffer(cmdBuffers[currentFrame]);

    // Submit streamed-texture uploads on their OWN submit first, signaling a
    // semaphore. The render submit waits on it, so every copy + layout
    // transition is globally complete before any draw samples the texture.
    // (Direct analog of the DX12 copy-queue + fence wait.)
    bool didUpload = false;
    if (uploadOpen) {
        vkEndCommandBuffer(uploadCmds[currentFrame]);
        VkSubmitInfo us = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        us.commandBufferCount = 1;
        us.pCommandBuffers = &uploadCmds[currentFrame];
        us.signalSemaphoreCount = 1;
        us.pSignalSemaphores = &uploadSemaphores[currentFrame];
        vkQueueSubmit(graphicsQueue, 1, &us, VK_NULL_HANDLE);
        uploadOpen = false;
        didUpload = true;
    }

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkSemaphore waitSemaphores[2] = {imageAvailableSemaphores[currentFrame],
                                     uploadSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[2] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    submitInfo.waitSemaphoreCount = didUpload ? 2u : 1u;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffers[currentFrame];

    // Output semaphore maps to the physical image on the monitor (to prevent
    // locking)
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex % 8]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);

    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult res = vkQueuePresentKHR(graphicsQueue, &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR ||
        m_vsync != currentVsyncState) {
        CleanupSwapchain();
        CreateSwapchainResources(m_vsync);
    }

    // Advance double buffering cycle, then wait until the frame slot we are
    // about to reuse is finished so the next Update can safely overwrite its
    // upload heap and we can recycle its retired slots/resources.
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                    UINT64_MAX);

    for (uint32_t slot : m_deferredBindlessFree[currentFrame])
        m_freeBindlessSlots.push_back(slot);
    m_deferredBindlessFree[currentFrame].clear();

    for (auto &r : m_imageGarbage[currentFrame]) {
        if (r.view)
            vkDestroyImageView(device, r.view, nullptr);
        if (r.image)
            vmaDestroyImage(allocator, r.image, r.alloc);
    }
    m_imageGarbage[currentFrame].clear();

    m_uploadHeaps[currentFrame].cursor = 0;
}

std::shared_ptr<RHIBuffer> RHI_Vulkan::CreateBuffer(BufferType t, const void *d,
                                                    size_t s, UINT stride) {
    size_t actualSize = s;
    if (t == BufferType::Constant) {
        actualSize = (s + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }

    auto buffer = std::make_shared<VKBuffer>();
    buffer->sizePerFrame = actualSize;
    buffer->stride = stride;

    // Dynamic buffers need memory chunks for each frame
    if (t == BufferType::Constant || t == BufferType::Instance) {
        actualSize *= MAX_FRAMES_IN_FLIGHT;
    }
    buffer->size = actualSize;

    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = actualSize;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (t == BufferType::ComputeUAV) {
        bufferInfo.usage =
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    } else if (t == BufferType::Vertex || t == BufferType::Instance) {
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    } else if (t == BufferType::Index) {
        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    } else {
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer->buf,
                    &buffer->alloc, nullptr);

    if (d) {
        void *mappedData;
        vmaMapMemory(allocator, buffer->alloc, &mappedData);
        // Duplicate data across all frames
        for (int i = 0; i < (t == BufferType::Constant || t == BufferType::Instance
                                 ? MAX_FRAMES_IN_FLIGHT
                                 : 1);
             i++) {
            memcpy(static_cast<uint8_t *>(mappedData) + i * buffer->sizePerFrame, d,
                   s);
        }
        vmaFlushAllocation(allocator, buffer->alloc, 0, actualSize);
        vmaUnmapMemory(allocator, buffer->alloc);
    }

    if (allocInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        VmaAllocationInfo vmaInfo;
        vmaGetAllocationInfo(allocator, buffer->alloc, &vmaInfo);
        buffer->mapped = vmaInfo.pMappedData;
    }

    buffer->bufferInfo = {buffer->buf, 0, actualSize};
    return buffer;
}

void RHI_Vulkan::UpdateBuffer(RHIBuffer *b, const void *d, size_t s) {
    auto vk = static_cast<VKBuffer *>(b);
    size_t offset =
        (vk->size > vk->sizePerFrame) ? (currentFrame * vk->sizePerFrame) : 0;

    if (vk->mapped) {
        memcpy(static_cast<uint8_t *>(vk->mapped) + offset, d, s);
        vmaFlushAllocation(allocator, vk->alloc, offset, s);
    } else {
        void *p;
        vmaMapMemory(allocator, vk->alloc, &p);
        memcpy(static_cast<uint8_t *>(p) + offset, d, s);
        vmaFlushAllocation(allocator, vk->alloc, offset, s);
        vmaUnmapMemory(allocator, vk->alloc);
    }
}

std::shared_ptr<RHITexture>
RHI_Vulkan::CreateTextureFromData(const void* data, size_t dataSize, int width,
                                  int height, DXGI_FORMAT format,
                                  int mipLevels) {
    if (!data || dataSize == 0 || width <= 0 || height <= 0 || mipLevels <= 0)
        return nullptr;

    auto t = std::make_shared<VKTexture>();
    t->width = width;
    t->height = height;

    VkFormat vkFormat = VKFormatFromDXGI(format);
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkFormat;
    imageInfo.extent = {static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height), 1};
    imageInfo.mipLevels = static_cast<uint32_t>(mipLevels);
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &t->image, &t->alloc,
                       nullptr) != VK_SUCCESS) {
        return nullptr;
    }

    t->ownsImage = true; // image/view/alloc retired via deferred garbage on destroy

    // Stage into this frame's ring upload buffer (cursor bump, no per-texture
    // allocation) and record the copy onto the per-frame upload command buffer.
    UploadAlloc up = AllocUpload(dataSize, 16);
    memcpy(up.cpu, data, dataSize);
    vmaFlushAllocation(allocator, m_uploadHeaps[currentFrame].alloc, up.offset,
                       dataSize);

    std::vector<VkBufferImageCopy> regions;
    regions.reserve(mipLevels);
    size_t srcOffset = 0;

    for (int mip = 0; mip < mipLevels; ++mip) {
        uint32_t mipW =
            std::max<uint32_t>(1, static_cast<uint32_t>(width) >> mip);
        uint32_t mipH =
            std::max<uint32_t>(1, static_cast<uint32_t>(height) >> mip);
        uint32_t rowBytes = 0;
        uint32_t rowCount = 0;
        VKMipLayout(format, mipW, mipH, rowBytes, rowCount);
        size_t mipSize = static_cast<size_t>(rowBytes) * rowCount;
        if (srcOffset + mipSize > dataSize)
            return nullptr;

        VkBufferImageCopy copy = {};
        copy.bufferOffset = up.offset + srcOffset; // offset within the ring buffer
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = static_cast<uint32_t>(mip);
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {mipW, mipH, 1};
        regions.push_back(copy);
        srcOffset += mipSize;
    }

    OpenUploadCmd();
    VkCommandBuffer c = uploadCmds[currentFrame];
    {
        VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = t->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = static_cast<uint32_t>(mipLevels);
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);

        vkCmdCopyBufferToImage(c, up.buf, t->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(regions.size()),
                               regions.data());

        // This barrier (TRANSFER -> VERTEX/FRAGMENT) also synchronizes the
        // render commands batched after this upload buffer in the frame submit.
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = t->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = static_cast<uint32_t>(mipLevels);
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &t->view);

    t->imageInfo = {mainSampler, t->view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    RegisterBindlessTexture(t.get());
    return t;
}

std::shared_ptr<RHITexture>
RHI_Vulkan::CreateTexture(const std::wstring &path) {
    auto t = std::make_shared<VKTexture>();
    int tw, th, tc;
    std::string pStr(path.begin(), path.end());

    unsigned char *d = stbi_load(pStr.c_str(), &tw, &th, &tc, 4);
    if (!d)
        return nullptr;

    t->width = tw;
    t->height = th;
    auto mips = GenerateMipChain(d, tw, th);
    stbi_image_free(d);

    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = tw;
    imageInfo.extent.height = th;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = static_cast<uint32_t>(mips.size());
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &t->image, &t->alloc,
                   nullptr);

    size_t totalSize = 0;
    for (auto &m : mips) {
        totalSize += m.pixels.size() * 4;
    }

    auto stage = CreateBuffer(BufferType::Constant, nullptr, totalSize);
    void *mapped;
    vmaMapMemory(allocator, static_cast<VKBuffer *>(stage.get())->alloc, &mapped);

    size_t offset = 0;
    std::vector<VkBufferImageCopy> regions(mips.size());

    for (size_t i = 0; i < mips.size(); i++) {
        size_t mipSize = mips[i].pixels.size() * 4;
        memcpy(static_cast<uint8_t *>(mapped) + offset, mips[i].pixels.data(),
               mipSize);

        regions[i].bufferOffset = offset;
        regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel = static_cast<uint32_t>(i);
        regions[i].imageSubresource.baseArrayLayer = 0;
        regions[i].imageSubresource.layerCount = 1;
        regions[i].imageExtent = {static_cast<uint32_t>(mips[i].width),
                                  static_cast<uint32_t>(mips[i].height), 1};

        offset += mipSize;
    }
    vmaUnmapMemory(allocator, static_cast<VKBuffer *>(stage.get())->alloc);

    SingleTimeCommand([&](VkCommandBuffer c) {
        VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = t->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = static_cast<uint32_t>(mips.size());
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);

        vkCmdCopyBufferToImage(c, static_cast<VKBuffer *>(stage.get())->buf,
                               t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(regions.size()),
                               regions.data());

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &barrier);
    });

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = t->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = static_cast<uint32_t>(mips.size());
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &t->view);

    t->imageInfo = {mainSampler, t->view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    RegisterBindlessTexture(t.get());
    return t;
}
std::shared_ptr<RHITexture> RHI_Vulkan::CreateUAVTexture3D(int w, int h, int depth, int format) {
    auto t = std::make_shared<VKTexture>();
    t->width = w;
    t->height = h;

    VkFormat fmt = VK_FORMAT_R32G32B32A32_SFLOAT;
    if (format == 0) fmt = VK_FORMAT_R8G8B8A8_UNORM;

    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_3D; // MUSÍ BÝT 3D
    imageInfo.format = fmt;
    imageInfo.extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<uint32_t>(depth) }; // TADY JE DEPTH
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &t->image, &t->alloc, nullptr);

    SingleTimeCommand([&](VkCommandBuffer c) {
        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.image = t->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        });

    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = t->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D; // MUSÍ BÝT 3D
    viewInfo.format = fmt;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &t->view);

    t->imageInfo = { mainSampler, t->view, VK_IMAGE_LAYOUT_GENERAL };
    t->uavInfo = { mainSampler, t->view, VK_IMAGE_LAYOUT_GENERAL };
    return t;
}
std::shared_ptr<RHITexture>
RHI_Vulkan::CreateDDSTexture(const std::wstring &path) {
    std::string pStr(path.begin(), path.end());
    auto texInfo = mt::Texture::from_file(pStr);
    if (!texInfo.ok())
        return nullptr;

    auto t = std::make_shared<VKTexture>();
    t->width = texInfo.desc().width;
    t->height = texInfo.desc().height;

    VkImageCreateInfo imageInfo = mt::make_vk_image_create_info(texInfo.desc());
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &t->image, &t->alloc,
                   nullptr);

    auto stage = CreateBuffer(BufferType::Constant, texInfo.owned_bytes().data(),
                              texInfo.owned_bytes().size());

    SingleTimeCommand([&](VkCommandBuffer c) {
        VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = t->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = texInfo.desc().mip_count;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount =
            mt::physical_layer_count(texInfo.desc());
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);

        auto copies = mt::build_vk_buffer_image_copies(texInfo);
        vkCmdCopyBufferToImage(c, static_cast<VKBuffer *>(stage.get())->buf,
                               t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(copies.size()), copies.data());

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &barrier);
    });

    VkImageViewCreateInfo viewInfo =
        mt::make_vk_image_view_create_info(t->image, texInfo.desc());
    vkCreateImageView(device, &viewInfo, nullptr, &t->view);

    t->imageInfo = {mainSampler, t->view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    RegisterBindlessTexture(t.get());
    return t;
}

void RHI_Vulkan::ImGuiInit(HWND hWnd) {
    VkDescriptorPoolSize pool_sizes[] = {
                                         {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo poolInfo = {
                                           VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    poolInfo.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
    ImGui_ImplWin32_Init(hWnd);

    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateVkSurface = [](ImGuiViewport *vp, ImU64 vk_inst,
                                              const void *vk_allocators,
                                              ImU64 *out_vk_surface) -> int {
        VkWin32SurfaceCreateInfoKHR create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        create_info.hinstance = GetModuleHandle(NULL);
        create_info.hwnd = reinterpret_cast<HWND>(vp->PlatformHandleRaw);

        VkSurfaceKHR surface;
        VkResult err = vkCreateWin32SurfaceKHR(
            reinterpret_cast<VkInstance>(vk_inst), &create_info,
            reinterpret_cast<const VkAllocationCallbacks *>(vk_allocators),
            &surface);
        *out_vk_surface = reinterpret_cast<ImU64>(surface);
        return static_cast<int>(err);
    };

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = 0;
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPool = imguiPool;
    initInfo.MinImageCount = MAX_FRAMES_IN_FLIGHT;
    initInfo.ImageCount = static_cast<uint32_t>(swapchainImageViews.size());
    initInfo.PipelineInfoMain.RenderPass = mainRenderPass;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&initInfo);
}

void RHI_Vulkan::ImGuiBegin() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void RHI_Vulkan::ImGuiEnd() {
    ImGui::Render();
    if (!isPassRunning) {
        SetMainPassTarget();
    }
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                    cmdBuffers[currentFrame]);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void RHI_Vulkan::ImGuiCleanup() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    vkDestroyDescriptorPool(device, imguiPool, nullptr);
}

void RHI_Vulkan::GetSize(int &outW, int &outH) const {
    outW = width;
    outH = height;
}

void RHI_Vulkan::Resize(int newW, int newH) {
    if (newW == 0 || newH == 0 || (newW == width && newH == height))
        return;
    width = newW;
    height = newH;
    frameOk = false;

    CleanupSwapchain();
    CreateSwapchainResources(currentVsyncState);
    currentFrame = 0;
    frameOk = true;
}

void RHI_Vulkan::SetGlobalUniforms(RHIBuffer *b, const void *d, size_t s) {
    auto vb = static_cast<VKBuffer *>(b);
    currentUBO = vb;
    needsDescriptorUpdate = true;

    // s == 0 means "re-bind the global data already uploaded this frame"
    // (terrain does this, mirroring a DX12 root-CBV re-bind). On Vulkan we must
    // NOT clobber the offset/size to 0 — that would give the descriptor an empty
    // range and the shader would read zero view/projection matrices.
    if (!d || s == 0)
        return;

    size_t aligned = (s + minUboAlignment - 1) & ~(minUboAlignment - 1);
    UINT64 frameBase = currentFrame * vb->sizePerFrame;

    if (vb->mapped) {
        memcpy(static_cast<uint8_t *>(vb->mapped) + frameBase + cOff, d, s);
        vmaFlushAllocation(allocator, vb->alloc, frameBase + cOff, s);
    }

    lastUBOOffset = static_cast<uint32_t>(frameBase + cOff);
    lastUBOSize = static_cast<uint32_t>(s);
    cOff += aligned;
}

void RHI_Vulkan::SetPushConstants(const void *data, size_t size) {
    if (currentPipeline) {
        vkCmdPushConstants(cmdBuffers[currentFrame], currentPipeline->layout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           static_cast<uint32_t>(size), data);
    }
}

void RHI_Vulkan::SetObjectUniforms(RHIBuffer *b, const void *d, size_t s) {
    size_t aligned = (s + minUboAlignment - 1) & ~(minUboAlignment - 1);
    auto vb = static_cast<VKBuffer *>(b);
    UINT64 frameBase = currentFrame * vb->sizePerFrame;

    if (vb->mapped && d) {
        memcpy(static_cast<uint8_t *>(vb->mapped) + frameBase + oOff, d, s);
        vmaFlushAllocation(allocator, vb->alloc, frameBase + oOff, s);
    }

    m_currentObjectBuffer = vb;
    m_currentObjectOffset = static_cast<uint32_t>(frameBase + oOff);
    oOff += aligned;
    needsDescriptorUpdate = true;
}

void RHI_Vulkan::SetBoneUniforms(RHIBuffer *b, const void *d, size_t s) {
    size_t aligned = (s + minUboAlignment - 1) & ~(minUboAlignment - 1);
    auto vb = static_cast<VKBuffer *>(b);
    UINT64 frameBase = currentFrame * vb->sizePerFrame;

    if (vb->mapped && d) {
        memcpy(static_cast<uint8_t *>(vb->mapped) + frameBase + bOff, d, s);
        vmaFlushAllocation(allocator, vb->alloc, frameBase + bOff, s);
    }

    m_currentBoneBuffer = vb;
    m_currentBoneOffset = static_cast<uint32_t>(frameBase + bOff);
    bOff += aligned;
    needsDescriptorUpdate = true;
}

void RHI_Vulkan::DrawIndexed(RHIBuffer *v, RHIBuffer *i, UINT c) {
    if (!frameOk)
        return;
    if (!isPassRunning)
        SetMainPassTarget();

    BindGlobalDescriptors();
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmdBuffers[currentFrame], 0, 1,
                           &(static_cast<VKBuffer *>(v)->buf), &off);
    vkCmdBindIndexBuffer(cmdBuffers[currentFrame],
                         static_cast<VKBuffer *>(i)->buf, 0,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmdBuffers[currentFrame], c, 1, 0, 0, 0);
}

void RHI_Vulkan::Draw(RHIBuffer *v, UINT c) {
    if (!frameOk)
        return;
    if (!isPassRunning)
        SetMainPassTarget();

    BindGlobalDescriptors();
    if (v) {
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmdBuffers[currentFrame], 0, 1,
                               &(static_cast<VKBuffer *>(v)->buf), &off);
    }
    vkCmdDraw(cmdBuffers[currentFrame], c, 1, 0, 0);
}

void RHI_Vulkan::DrawIndexedInstanced(RHIBuffer *vb, RHIBuffer *ib,
                                      RHIBuffer *instB, UINT indexCount,
                                      UINT instanceCount, UINT instanceOffset) {
    if (!frameOk)
        return;
    if (!isPassRunning)
        SetMainPassTarget();

    BindGlobalDescriptors();

    auto instVk = static_cast<VKBuffer *>(instB);
    VkDeviceSize instOff = (instVk->size > instVk->sizePerFrame)
                               ? (currentFrame * instVk->sizePerFrame)
                               : 0;

    VkBuffer vbs[] = {static_cast<VKBuffer *>(vb)->buf, instVk->buf};
    VkDeviceSize offsets[] = {0, instOff};

    vkCmdBindVertexBuffers(cmdBuffers[currentFrame], 0, 2, vbs, offsets);
    vkCmdBindIndexBuffer(cmdBuffers[currentFrame],
                         static_cast<VKBuffer *>(ib)->buf, 0,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmdBuffers[currentFrame], indexCount, instanceCount, 0, 0,
                     instanceOffset);
}


std::shared_ptr<RHIPipeline> RHI_Vulkan::CreateComputePipeline(const std::wstring& csPath) {
    auto p = std::make_shared<VKPipeline>();

    // ZMĚNA: Voláme náš nový Compiler
    auto cS = ShaderCompiler::CompileVulkan(csPath, "CSMain", "cs_6_0");
    if (cS.empty()) return nullptr; // Elegantní selhání

    p->descLayout = computeDescLayout;

    VkPipelineLayoutCreateInfo pipeLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeLayoutInfo.setLayoutCount = 1;
    pipeLayoutInfo.pSetLayouts = &p->descLayout;
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &p->layout);

    VkShaderModule cM;
    VkShaderModuleCreateInfo smi = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };

    // ZMĚNA: Bezpečné předání dat do Vulkanu
    smi.codeSize = cS.size(); // Bez * 4
    smi.pCode = reinterpret_cast<const uint32_t*>(cS.data()); // Přetypování

    vkCreateShaderModule(device, &smi, nullptr, &cM);

    VkComputePipelineCreateInfo cpi = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    cpi.layout = p->layout;
    cpi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpi.stage.module = cM;
    cpi.stage.pName = "CSMain";

    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpi, nullptr, &p->pipeline);
    vkDestroyShaderModule(device, cM, nullptr);

    return p;
}
std::shared_ptr<RHITexture> RHI_Vulkan::CreateUAVTexture(int w, int h,
                                                         int format) {
    auto t = std::make_shared<VKTexture>();
    t->width = w;
    t->height = h;

    VkFormat fmt = VK_FORMAT_R32G32B32A32_SFLOAT;
    if (format == 0)
        fmt = VK_FORMAT_R8G8B8A8_UNORM;

    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = fmt;
    imageInfo.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &t->image, &t->alloc,
                   nullptr);

    SingleTimeCommand([&](VkCommandBuffer c) {
        VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.image = t->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);
    });

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = t->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = fmt;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &t->view);

    t->imageInfo = {mainSampler, t->view, VK_IMAGE_LAYOUT_GENERAL};
    t->uavInfo = {mainSampler, t->view, VK_IMAGE_LAYOUT_GENERAL};
    return t;
}

void RHI_Vulkan::SetComputePipeline(RHIPipeline *p) {
    currentPipeline = static_cast<VKPipeline *>(p);
    vkCmdBindPipeline(cmdBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                      currentPipeline->pipeline);

    for (int i = 0; i < 2; i++) {
        m_currentComputeUAV[i] = nullptr;
        m_currentComputeSRV[i] = nullptr;
        m_currentComputeBuffer[i] = nullptr;
    }
}

void RHI_Vulkan::SetComputeUniforms(RHIBuffer *b, const void *d, size_t s,
                                    int slot) {
    size_t aligned = (s + minUboAlignment - 1) & ~(minUboAlignment - 1);
    auto vb = static_cast<VKBuffer *>(b);
    UINT64 frameBase = currentFrame * vb->sizePerFrame;

    if (vb->mapped && d) {
        memcpy(static_cast<uint8_t *>(vb->mapped) + frameBase + compOff, d, s);
        vmaFlushAllocation(allocator, vb->alloc, frameBase + compOff, s);
    }

    currentUBO = vb;
    lastCompUBOOffset = static_cast<uint32_t>(frameBase + compOff);
    lastCompUBOSize = static_cast<uint32_t>(s);
    compOff += aligned;
}

void RHI_Vulkan::DispatchCompute(UINT x, UINT y, UINT z) {
    if (!currentPipeline || !currentUBO)
        return;

    VkDescriptorSetAllocateInfo allocInfo = {
                                             VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descPools[currentFrame];
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &currentPipeline->descLayout;

    VkDescriptorSet newSet;
    if (vkAllocateDescriptorSets(device, &allocInfo, &newSet) != VK_SUCCESS)
        return;

    VkDescriptorBufferInfo bInfo = {currentUBO->buf, 0, lastCompUBOSize};
    VkDescriptorImageInfo uavInfos[2];
    VkDescriptorImageInfo srvInfos[2];
    VkDescriptorBufferInfo ssboInfos[2];

    std::vector<VkWriteDescriptorSet> writes;
    writes.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, newSet, 0,
                      0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr,
                      &bInfo, nullptr});

    for (int i = 0; i < 1; i++) {
        uavInfos[i] = m_currentComputeUAV[i]
                          ? m_currentComputeUAV[i]->uavInfo
                          : static_cast<VKTexture *>(m_dummy2D.get())->uavInfo;
        writes.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, newSet,
                          static_cast<uint32_t>(15 + i), 0, 1,
                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &uavInfos[i], nullptr,
                          nullptr});
    }

    for (int i = 0; i < 1; i++) {
        srvInfos[i] = m_currentComputeSRV[i]
                          ? m_currentComputeSRV[i]->imageInfo
                          : static_cast<VKTexture *>(m_dummy2D.get())->imageInfo;
        writes.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, newSet,
                          static_cast<uint32_t>(3 + i), 0, 1,
                          VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &srvInfos[i], nullptr,
                          nullptr});
    }

    for (int i = 0; i < 1; i++) {
        if (m_currentComputeBuffer[i]) {
            ssboInfos[i] = m_currentComputeBuffer[i]->bufferInfo;
        } else {
            ssboInfos[i] = {static_cast<VKBuffer *>(m_dummySSBO.get())->buf, 0,
                            VK_WHOLE_SIZE};
        }
        writes.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, newSet,
                          static_cast<uint32_t>(16 + i), 0, 1,
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &ssboInfos[i],
                          nullptr});
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
    uint32_t dynOffset = lastCompUBOOffset;
    vkCmdBindDescriptorSets(
        cmdBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
        currentPipeline->layout, 0, 1, &newSet, 1, &dynOffset);
    vkCmdDispatch(cmdBuffers[currentFrame], x, y, z);
}

void RHI_Vulkan::SetComputeTextureSRV(RHITexture *t, int slot) {
    if (slot >= 0 && slot < 2) {
        m_currentComputeSRV[slot] = static_cast<VKTexture *>(t);
    }
}

void RHI_Vulkan::SetComputeTextureUAV(RHITexture *t, int slot) {
    if (slot >= 0 && slot < 2) {
        auto vkt = static_cast<VKTexture *>(t);
        if (vkt && vkt->uavInfo.imageLayout != VK_IMAGE_LAYOUT_GENERAL) {
            VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.oldLayout = vkt->uavInfo.imageLayout;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.image = vkt->image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(cmdBuffers[currentFrame],
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                                 0, nullptr, 1, &barrier);

            vkt->imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            vkt->uavInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        m_currentComputeUAV[slot] = vkt;
    }
}

void RHI_Vulkan::ComputeBarrier(RHITexture *t) {
    if (!t)
        return;
    auto vkt = static_cast<VKTexture *>(t);

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.image = vkt->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmdBuffers[currentFrame],
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkt->imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkt->uavInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void RHI_Vulkan::SetComputeBufferUAV(RHIBuffer *buffer, int slot) {
    if (slot >= 0 && slot < 2) {
        m_currentComputeBuffer[slot] = static_cast<VKBuffer *>(buffer);
        if (buffer) {
            auto vk = static_cast<VKBuffer *>(buffer);
            VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            barrier.dstAccessMask =
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = vk->buf;
            barrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(cmdBuffers[currentFrame],
                                 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                                 1, &barrier, 0, nullptr);
        }
    }
}

void RHI_Vulkan::ComputeBufferBarrier(RHIBuffer *buffer) {
    if (!buffer)
        return;
    auto vk = static_cast<VKBuffer *>(buffer);

    VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = vk->buf;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        cmdBuffers[currentFrame], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0, 0, nullptr, 1, &barrier, 0, nullptr);
}
