#pragma once

#include "pch/Pch.h"
#include "rhi/RHI.h"
#include "rhi/backend_types/DX12Types.h"

/// Persistent, per-frame mapped UPLOAD ring buffer. Allocations are a simple
/// cursor bump (no per-call resource creation), reset once per frame after the
/// GPU is known to be done with that frame's slot. Replaces the old pattern of
/// creating a fresh staging buffer + command list + full GPU flush per texture.
struct LinearUploadHeap {
    ComPtr<ID3D12Resource> buf;
    uint8_t* cpu = nullptr;
    UINT64 capacity = 0;
    UINT64 cursor = 0;

    struct Alloc {
        ID3D12Resource* res;
        uint8_t* cpu;
        UINT64 offset;
    };

    void Init(ID3D12Device* dev, UINT64 size);
    void Reset() { cursor = 0; }
    Alloc Allocate(UINT64 size, UINT64 align);
};

/// =============================================================
/// =============================================================
/// DX12 IMPLEMENTATION
/// =============================================================
/// =============================================================

class RHI_DX12 : public RHI {
private:
    // --- CORE DX12 ---
    ComPtr<ID3D12Device> dev;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain3> swap;
    ComPtr<ID3D12GraphicsCommandList> cmd;

    // --- DESCRIPTOR HEAPS (Memory for pointers to textures and buffers) ---
    ComPtr<ID3D12DescriptorHeap> rtvH; // Render Target Views (Screens)
    ComPtr<ID3D12DescriptorHeap> srvH; // Shader Resource Views (Textures, Buffers)
    ComPtr<ID3D12DescriptorHeap> dsvH; // Depth Stencil Views (Z-Buffers)
    ComPtr<ID3D12DescriptorHeap> imguiH; // Separate heap for ImGui
    // --- CPU AND GPU SYNCHRONIZATION ---
    ComPtr<ID3D12Fence> fence;
    HANDLE fEvt;
    UINT64 fVal = 0;
    static const int FrameCount = 2;
    UINT64 fenceValues[FrameCount] = {0, 0};
    int fIdx = 0; // Index of the current frame (0 or 1)

    // --- COMMAND ALLOCATORS ---
    ComPtr<ID3D12CommandAllocator> alc[FrameCount];

    // --- RENDER TARGETS ---
    std::shared_ptr<DX12Texture> m_backBuffers[FrameCount];
    ComPtr<ID3D12Resource> depth;
    int w, h;
    BOOL tearingSupported = FALSE;

    // --- MANAGE MEMORY OFFSETS ---
    // These variables ensure that buffers in a single frame do not overwrite each other
    UINT sInc, dsvInc, rtvInc; // Sizes of individual descriptor types
    UINT sOff = 0;             // Offset for SRV (Textures)
    UINT rtvOff = 0;           // Offset for RTV (Render Targets)
    UINT cOff = 0;             // Offset for Global Constant Buffer
    UINT oOff = 0;             // Offset for Object Constant Buffer
    UINT bOff = 0;             // Offset for Bone Constant Buffer
    UINT compOff = 0;          // Offset for Compute Constant Buffer

    // --- PIPELINE STATE ---
    ID3D12PipelineState *lastPSO = nullptr;
    ID3D12RootSignature *lastRS = nullptr;
    ComPtr<ID3D12RootSignature> computeRS;
    // Recycled SRV heap slots (bindless indices) returned by destroyed textures.
    std::vector<UINT> m_freeSrvSlots;

    // --- ASYNC STREAMING UPLOAD (dedicated COPY queue) ---
    // Streamed textures (terrain tiles) record their copies here once per frame
    // instead of stalling the GPU per-texture. Flushed in BeginFrame; the main
    // queue waits on the copy fence so draws see finished data.
    ComPtr<ID3D12CommandQueue> copyQueue;
    ComPtr<ID3D12CommandAllocator> copyAlc[FrameCount];
    ComPtr<ID3D12GraphicsCommandList> copyCmd;
    ComPtr<ID3D12Fence> copyFence;
    UINT64 copyFenceVal = 0;
    HANDLE copyEvt = nullptr;
    bool copyOpen = false;
    LinearUploadHeap m_uploadHeap[FrameCount];

    // --- DEFERRED DESTRUCTION ---
    // Slots and GPU resources retired this frame are only recycled / released
    // once the frame index cycles back (GPU guaranteed done), avoiding
    // use-after-free of descriptors/resources still referenced in flight.
    std::vector<UINT> m_deferredSrvFree[FrameCount];
    std::vector<ComPtr<ID3D12Resource>> m_resourceGarbage[FrameCount];

    void OpenCopyList();
    void FlushCopyList();

    void Sync();
    void TransitionResource(DX12Texture *tex, D3D12_RESOURCE_STATES targetState);
    void TransitionBuffer(ID3D12Resource *res, D3D12_RESOURCE_STATES before,
                          D3D12_RESOURCE_STATES after);
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSrvDescriptor(DX12Texture *tex);

public:
    // Retire a bindless SRV slot (called by ~DX12Texture). Recycled later.
    void FreeSrvDescriptor(UINT index);
    // Keep a GPU resource alive until the current frame slot is recycled.
    void DeferResourceRelease(ComPtr<ID3D12Resource> res);

    bool Init(HWND hWnd, int width, int height) override;
    void GetSize(int &outW, int &outH) const override;
    void Resize(int newW, int newH) override;
    void ImGuiInit(HWND h) override;
    void ImGuiBegin() override;
    void ImGuiEnd() override;
    void ImGuiCleanup() override;
    void BeginFrame() override;
    void EndFrame() override;
    RHITexture *GetBackBuffer() override;
    std::shared_ptr<RHITexture> CreateRenderTarget(int w_in, int h_in,
                                                   int format) override;
    void SetMRTTargets(std::vector<RHITexture *> targets,
                       RHITexture *depthMap) override;
    void SetMainPassTarget() override;
    void ClearRenderTarget(RHITexture *target, const float color[4]) override;
    void ClearDepthTarget(RHITexture *depthTarget, float depthVal,
                          uint8_t stencil) override;
    std::shared_ptr<RHITexture> CreateShadowTexture(int width,
                                                    int height) override;
    void BeginShadowPass(RHITexture *t) override;
    std::shared_ptr<RHITexture>
    CreateDDSTexture(const std::wstring &path) override;
    std::shared_ptr<RHITexture> CreateTexture(const std::wstring &path) override;
    std::shared_ptr<RHITexture> CreateTextureFromData(const void *data, size_t dataSize,
                                                      int width, int height,
                                                      DXGI_FORMAT format,
                                                      int mipLevels = 1) override;
    void SetTexture(RHITexture *t, int slot) override;
    std::shared_ptr<RHIBuffer> CreateBuffer(BufferType type, const void *data,
                                            size_t size,
                                            UINT stride = 0) override;
    void UpdateBuffer(RHIBuffer *b, const void *d, size_t s) override;
    void SetGlobalUniforms(RHIBuffer *b, const void *d, size_t s) override;
    void SetObjectUniforms(RHIBuffer *b, const void *d, size_t s) override;
    void SetBoneUniforms(RHIBuffer *b, const void *d, size_t s) override;
    void SetPushConstants(const void *data, size_t size) override;
    std::shared_ptr<RHIPipeline>
    CreatePipeline(const PipelineConfig &config) override;
    void SetPipeline(RHIPipeline *p) override;
    void DrawIndexed(RHIBuffer *vb, RHIBuffer *ib, UINT c) override;
    void Draw(RHIBuffer *vb, UINT c) override;
    void DrawIndexedInstanced(RHIBuffer *vb, RHIBuffer *ib, RHIBuffer *instB,
                              UINT indexCount, UINT instanceCount,
                              UINT instanceOffset) override;
    std::shared_ptr<RHIPipeline>
    CreateComputePipeline(const std::wstring &csPath) override;
    std::shared_ptr<RHITexture> CreateUAVTexture(int w_in, int h_in,
                                                 int format = 0) override;
    std::shared_ptr<RHITexture> CreateUAVTexture3D(int w_in, int h_in, int depth_in, int format = 0) override;
    void SetComputePipeline(RHIPipeline *p) override;
    void SetComputeUniforms(RHIBuffer *b, const void *d, size_t s,
                            int slot) override;
    void SetComputeTextureSRV(RHITexture *t, int slot) override;
    void SetComputeTextureUAV(RHITexture *t, int slot) override;
    void DispatchCompute(UINT x, UINT y, UINT z) override;
    void ComputeBarrier(RHITexture *uavTexture) override;
    void SetComputeBufferUAV(RHIBuffer *buffer, int slot) override;
    void ComputeBufferBarrier(RHIBuffer *buffer) override;
};
