#pragma once

#include "pch/Pch.h"
#include "rhi/RHI.h"
#include "rhi/backend_types/VulkanTypes.h"

/// =============================================================
/// =============================================================
/// Vulkan IMPLEMENTATION
/// =============================================================
/// =============================================================


class RHI_Vulkan : public RHI {
private:
	// Core Vulkan handles
	VkInstance instance = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice physDevice = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VmaAllocator allocator = VK_NULL_HANDLE;

	// --- ASYNC TRANSFER (dedicated DMA copy queue, with graphics-queue fallback) ---
	// On a GPU that exposes a transfer-only queue family, streamed-texture copies
	// run on a real DMA engine in parallel with rendering. The ownership of each
	// uploaded image is transferred transfer-family -> graphics-family (release on
	// the transfer queue, acquire on the graphics queue). If no dedicated family
	// exists, m_transferQueue == graphicsQueue and no ownership transfer is done.
	uint32_t m_graphicsFamily = 0;
	uint32_t m_transferFamily = 0;
	VkQueue m_transferQueue = VK_NULL_HANDLE;
	bool m_hasDedicatedTransfer = false;
	VkCommandPool m_transferPool = VK_NULL_HANDLE;
	// Queue-family ACQUIRE barriers for images uploaded on the transfer queue this
	// frame; recorded on the graphics command buffer at BeginFrame (uploads happen
	// during Update, before BeginFrame). Empty in the fallback path.
	std::vector<VkImageMemoryBarrier> m_pendingAcquires;

	// --- RENDER PASSES ---
	VkRenderPass mainRenderPass = VK_NULL_HANDLE; // UI and final output to screen (No Depth)
	VkRenderPass mrtRenderPass = VK_NULL_HANDLE; // 3D Scene G-Buffer (Colors, Normals, Positions + Depth)
	VkRenderPass shadowRenderPass = VK_NULL_HANDLE; // Directional Light Shadows (Depth only)
	VkRenderPass singleOffscreenRenderPass = VK_NULL_HANDLE; // Post-processing passes (1 Color, No Depth)

	// Command execution
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> cmdBuffers;

	// Swapchain resources
	std::vector<VkImageView> swapchainImageViews;
	std::vector<std::shared_ptr<VKTexture>> m_backBuffers;
	VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;

	// Global Depth Buffer
	VkImage depthImage = VK_NULL_HANDLE;
	VkImageView depthView = VK_NULL_HANDLE;
	VmaAllocation depthAlloc = VK_NULL_HANDLE;

	// Descriptor Management
	std::vector<VkDescriptorPool> descPools;
	VkDescriptorPool imguiPool = VK_NULL_HANDLE;
	VkSampler mainSampler = VK_NULL_HANDLE;
	VkSampler shadowSampler = VK_NULL_HANDLE;
	VkSampler m_terrainSampler = VK_NULL_HANDLE; // CLAMP sampler for terrain (s2)
	std::vector<VkDescriptorImageInfo> m_bindlessImageInfos;
	uint32_t m_bindlessTextureCount = 0;

	// --- BINDLESS SLOT RECYCLING (deferred until the frame fence signals) ---
	std::vector<uint32_t> m_freeBindlessSlots;
	std::vector<std::vector<uint32_t>> m_deferredBindlessFree; // per frame

	// --- DEFERRED GPU RESOURCE DESTRUCTION ---
	struct RetiredImage { VkImage image; VkImageView view; VmaAllocation alloc; };
	std::vector<std::vector<RetiredImage>> m_imageGarbage; // per frame

	// --- ASYNC STREAMING UPLOAD ---
	// Streamed textures (terrain) stage into a per-frame ring buffer and record
	// their copies into a per-frame upload command buffer, which is batched into
	// the frame's submit. Replaces the per-texture SingleTimeCommand + queue idle.
	struct VkUploadHeap {
		VkBuffer buf = VK_NULL_HANDLE;
		VmaAllocation alloc = VK_NULL_HANDLE;
		uint8_t* cpu = nullptr;
		VkDeviceSize cap = 0;
		VkDeviceSize cursor = 0;
	};
	std::vector<VkUploadHeap> m_uploadHeaps;      // per frame
	std::vector<VkCommandBuffer> uploadCmds;      // per frame
	std::vector<VkSemaphore> uploadSemaphores;    // per frame: upload submit -> render submit
	bool uploadOpen = false;

	struct UploadAlloc { VkBuffer buf; uint8_t* cpu; VkDeviceSize offset; };
	void OpenUploadCmd();
	UploadAlloc AllocUpload(VkDeviceSize size, VkDeviceSize align);

	// --- STATE TRACKING (Bound resources) ---
	VKTexture* currentTextures[8] = { nullptr };

	VKBuffer* currentUBO = nullptr;
	uint32_t lastUBOOffset = 0;

	VKBuffer* m_currentObjectBuffer = nullptr;
	uint32_t m_currentObjectOffset = 0;

	VKBuffer* m_currentBoneBuffer = nullptr;
	uint32_t m_currentBoneOffset = 0;

	// Compute Shader State
	VKTexture* m_currentComputeUAV[2] = { nullptr };
	VKTexture* m_currentComputeSRV[2] = { nullptr };
	VKBuffer* m_currentComputeBuffer[2] = { nullptr };
	VkDescriptorSetLayout computeDescLayout = VK_NULL_HANDLE;

	// --- DOUBLE BUFFERING & SYNCHRONIZATION ---
	// CPU prepares 2 frames ahead to prevent input lag and GPU stalling.
	const int MAX_FRAMES_IN_FLIGHT = 2;
	uint32_t currentFrame = 0;

	std::vector<VkSemaphore> imageAvailableSemaphores; // Wait for monitor to return a free image
	std::vector<VkFence> inFlightFences; // Prevent CPU from overwriting currently rendering frame
	std::vector<VkSemaphore> renderFinishedSemaphores; // Allocated to 8: Safely maps to the physical
	// swapchain image index

	// Dynamic UBO Offsets (Incremented per draw call)
	VkDeviceSize cOff = 0;
	VkDeviceSize oOff = 0;
	VkDeviceSize bOff = 0;
	VkDeviceSize compOff = 0;
	// ---------------------------------------------------

	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t imageIndex = 0;
	VkDeviceSize minUboAlignment = 256;

	uint32_t lastCompUBOOffset = 0;
	uint32_t lastCompUBOSize = 0;

	bool isPassRunning = false;
	bool frameOk = false;
	bool currentVsyncState = false;
	VKPipeline* currentPipeline = nullptr;
	bool needsDescriptorUpdate = true;

	// Dummy resources to prevent validation errors on unbound slots
	std::shared_ptr<RHIBuffer> m_dummyUBO;
	std::shared_ptr<RHIBuffer> m_dummySSBO;
	std::shared_ptr<RHITexture> m_dummy2D;
	std::shared_ptr<RHITexture> m_dummyCube;

	// Render target caching
	std::map<uint64_t, VkFramebuffer> fbCache;
	std::vector<RHITexture*> currentMRT;
	RHITexture* m_currentDepthMap = nullptr;
	uint32_t lastUBOSize = 256;
	// Debugging
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	static VKAPI_ATTR VkBool32 VKAPI_CALL
		VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData);
	void SingleTimeCommand(std::function<void(VkCommandBuffer)> action);
	std::shared_ptr<RHITexture> CreateDummyTexture(bool isCube);
	void CleanupSwapchain();
	void CreateSwapchainResources(bool vsyncEnabled);
	void BindGlobalDescriptors();
	void RegisterBindlessTexture(VKTexture* tex);

public:
	// Called by ~VKTexture: retire its GPU image + bindless slot for deferred recycling.
	void RetireVKTexture(VKTexture* t);
	void FreeBindlessSlot(uint32_t index);

	~RHI_Vulkan();
	bool Init(HWND hWnd, int w, int h) override;
	RHITexture* GetBackBuffer() override;
	std::shared_ptr<RHITexture> CreateRenderTarget(int w, int h,int format) override;
	std::shared_ptr<RHITexture> CreateShadowTexture(int w, int h) override;
	void SetMainPassTarget() override;
	void BeginShadowPass(RHITexture* t) override;
	void SetMRTTargets(std::vector<RHITexture*> targets,RHITexture* depthMap) override;
	void ClearRenderTarget(RHITexture* target, const float color[4]) override;
	void ClearDepthTarget(RHITexture* dt, float depth, uint8_t stencil) override;
	void SetTexture(RHITexture* t, int s) override;
	std::vector<uint32_t> CompileHLSL(const std::wstring& path, const char* entry,const char* target);
	std::shared_ptr<RHIPipeline> CreatePipeline(const PipelineConfig& config) override;
	void SetPipeline(RHIPipeline* p) override;
	void BeginFrame() override;
	void EndFrame() override;
	std::shared_ptr<RHIBuffer> CreateBuffer(BufferType t, const void* d, size_t s,UINT stride = 0) override;
	void UpdateBuffer(RHIBuffer* b, const void* d, size_t s) override;
	std::shared_ptr<RHITexture> CreateTexture(const std::wstring& path) override;
	std::shared_ptr<RHITexture> CreateDDSTexture(const std::wstring& path) override;
	std::shared_ptr<RHITexture> CreateTextureFromData(const void* data, size_t dataSize, int width, int height, DXGI_FORMAT format, int mipLevels = 1) override;
	void ImGuiInit(HWND hWnd) override;
	void ImGuiBegin() override;
	void ImGuiEnd() override;
	void ImGuiCleanup() override;
	void GetSize(int& outW, int& outH) const override;
	void Resize(int newW, int newH) override;
	void SetGlobalUniforms(RHIBuffer* b, const void* d, size_t s) override;
	void SetPushConstants(const void* data, size_t size) override;
	void SetObjectUniforms(RHIBuffer* b, const void* d, size_t s) override;
	void SetBoneUniforms(RHIBuffer* b, const void* d, size_t s) override;
	void DrawIndexed(RHIBuffer* v, RHIBuffer* i, UINT c) override;
	void Draw(RHIBuffer* v, UINT c) override;
	void DrawIndexedInstanced(RHIBuffer* vb, RHIBuffer* ib, RHIBuffer* instB,UINT indexCount, UINT instanceCount,UINT instanceOffset) override;
	std::shared_ptr<RHIPipeline> CreateComputePipeline(const std::wstring& csPath) override;
	std::shared_ptr<RHITexture> CreateUAVTexture(int w, int h,int format = 0) override;
	std::shared_ptr<RHITexture> CreateUAVTexture3D(int w, int h, int depth, int format = 0) override;
	void SetComputePipeline(RHIPipeline* p) override;
	void SetComputeUniforms(RHIBuffer* b, const void* d, size_t s,int slot) override;
	void DispatchCompute(UINT x, UINT y, UINT z) override;
	void SetComputeTextureSRV(RHITexture* t, int slot) override;
	void SetComputeTextureUAV(RHITexture* t, int slot) override;
	void ComputeBarrier(RHITexture* t) override;
	void SetComputeBufferUAV(RHIBuffer* buffer, int slot) override;
	void ComputeBufferBarrier(RHIBuffer* buffer) override;
};



