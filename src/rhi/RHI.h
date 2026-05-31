#pragma once

#include "pch/Pch.h"
#include "rhi/GraphicsTypes.h"
#include "rhi/MipGenerator.h"

enum class BufferType { Vertex, Index, Constant, Instance, ComputeUAV };
class RHIBuffer { public: UINT stride = 0; virtual ~RHIBuffer(); };
class RHITexture { public: int width = 0; int height = 0; virtual ~RHITexture(); };
class RHIPipeline { public: virtual ~RHIPipeline(); };

class RHI {
public:
    bool m_vsync = true;
    RHIPipeline* m_currentPipeline = nullptr;

    virtual ~RHI();
    virtual bool Init(HWND hWnd, int w, int h) = 0;
    virtual void ImGuiInit(HWND hWnd) = 0; virtual void ImGuiBegin() = 0; virtual void ImGuiEnd() = 0; virtual void ImGuiCleanup() = 0;
    virtual void BeginFrame() = 0; virtual void EndFrame() = 0;

    virtual std::shared_ptr<RHIBuffer> CreateBuffer(BufferType type, const void* data, size_t size, UINT stride = 0) = 0;
    virtual void UpdateBuffer(RHIBuffer* buffer, const void* data, size_t size) = 0;
    virtual std::shared_ptr<RHIPipeline> CreatePipeline(const PipelineConfig& config) = 0;

    virtual std::shared_ptr<RHITexture> CreateTexture(const std::wstring& path) = 0;
    virtual std::shared_ptr<RHITexture> CreateShadowTexture(int width, int height) = 0;
    virtual std::shared_ptr<RHITexture> CreateDDSTexture(const std::wstring& path) = 0;

    // ---  MRT And OFFSCREEN ---
    virtual std::shared_ptr<RHITexture> CreateRenderTarget(int w, int h, int format) = 0;
    virtual void SetMRTTargets(std::vector<RHITexture*> targets, RHITexture* depthMap) = 0;
    virtual void ClearRenderTarget(RHITexture* target, const float color[4]) = 0;
    virtual void ClearDepthTarget(RHITexture* depthTarget, float depth, uint8_t stencil) = 0;
	virtual RHITexture* GetBackBuffer() = 0; // helper function to get the current back buffer as a render target
    // ---------------------------------------

    virtual void BeginShadowPass(RHITexture* shadowMap) = 0;
    virtual void SetMainPassTarget() = 0;
    virtual void SetPipeline(RHIPipeline* pipeline) = 0;
    virtual void SetTexture(RHITexture* texture, int slot) = 0;

    virtual void SetGlobalUniforms(RHIBuffer* buffer, const void* data, size_t size) = 0;
    virtual void SetPushConstants(const void* data, size_t size) = 0;
    virtual void SetObjectUniforms(RHIBuffer* buffer, const void* data, size_t size) = 0;
    virtual void SetBoneUniforms(RHIBuffer* buffer, const void* data, size_t size) = 0;

    virtual void DrawIndexed(RHIBuffer* vb, RHIBuffer* ib, UINT indexCount) = 0;
    virtual void Draw(RHIBuffer* vb, UINT vertexCount) = 0;
    virtual void DrawIndexedInstanced(RHIBuffer* vb, RHIBuffer* ib, RHIBuffer* instB, UINT indexCount, UINT instanceCount, UINT instanceOffset) = 0;

    virtual void Resize(int width, int height) = 0;
    virtual void GetSize(int& w, int& h) const = 0;

    // COMPUTE
    virtual std::shared_ptr<RHIPipeline> CreateComputePipeline(const std::wstring& csPath) = 0;
    virtual std::shared_ptr<RHITexture> CreateUAVTexture(int width, int height, int format = 0) = 0;
    virtual void SetComputePipeline(RHIPipeline* pipeline) = 0;
    virtual void SetComputeUniforms(RHIBuffer* buffer, const void* data, size_t size, int slot) = 0;
    virtual void SetComputeTextureSRV(RHITexture* texture, int slot) = 0;
    virtual void SetComputeTextureUAV(RHITexture* texture, int slot) = 0;
    virtual void DispatchCompute(UINT groupX, UINT groupY, UINT groupZ) = 0;
    virtual void ComputeBarrier(RHITexture* uavTexture) = 0;
    virtual void SetComputeBufferUAV(RHIBuffer* buffer, int slot) = 0;
    virtual void ComputeBufferBarrier(RHIBuffer* buffer) = 0;
};


