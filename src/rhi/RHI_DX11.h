#pragma once

#include "pch/Pch.h"
#include "rhi/RHI.h"
#include "rhi/backend_types/DX11Types.h"

/// =============================================================
/// =============================================================
/// DX11 IMPLEMENTATION
/// =============================================================
/// =============================================================

class RHI_DX11 : public RHI {
    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> ctx;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11SamplerState> samp, shadowSamp;
    ComPtr<ID3D11Buffer> m_pushConstantBuffer;
    ComPtr<ID3D11BlendState> blendState;
    int width, height;

    std::shared_ptr<DX11Texture> m_backBuffer;
    ComPtr<ID3D11DepthStencilView> dsv;

public:
    bool Init(HWND hWnd, int w, int h) override;
    RHITexture *GetBackBuffer() override;
    std::shared_ptr<RHITexture> CreateRenderTarget(int w, int h,
                                                   int format) override;
    void SetMRTTargets(std::vector<RHITexture *> targets,
                       RHITexture *depthMap) override;
    void ClearRenderTarget(RHITexture *target, const float color[4]) override;
    void ClearDepthTarget(RHITexture *depthTarget, float depth,
                          uint8_t stencil) override;
    void SetMainPassTarget() override;
    std::shared_ptr<RHITexture> CreateShadowTexture(int w, int h) override;
    void BeginShadowPass(RHITexture *t) override;
    void ImGuiInit(HWND hWnd) override;
    void ImGuiBegin() override;
    void ImGuiEnd() override;
    void ImGuiCleanup() override;
    std::shared_ptr<RHIBuffer> CreateBuffer(BufferType type, const void *data,
                                            size_t size,
                                            UINT stride = 0) override;
    void UpdateBuffer(RHIBuffer *b, const void *d, size_t s) override;
    std::shared_ptr<RHITexture> CreateTexture(const std::wstring &path) override;
    std::shared_ptr<RHITexture>
    CreateDDSTexture(const std::wstring &path) override;
    std::shared_ptr<RHITexture> CreateUAVTexture3D(int width, int height, int depth, int format = 0) override;
    std::shared_ptr<RHIPipeline>
    CreatePipeline(const PipelineConfig &config) override;
    void BeginFrame() override;
    void EndFrame() override;
    void SetPipeline(RHIPipeline *p) override;
    void SetTexture(RHITexture *t, int slot) override;
    void SetGlobalUniforms(RHIBuffer *b, const void *d, size_t s) override;
    void SetPushConstants(const void *data, size_t size) override;
    void SetObjectUniforms(RHIBuffer *b, const void *d, size_t s) override;
    void SetBoneUniforms(RHIBuffer *b, const void *d, size_t s) override;
    void DrawIndexed(RHIBuffer *vb, RHIBuffer *ib, UINT c) override;
    void Draw(RHIBuffer *vb, UINT c) override;
    void DrawIndexedInstanced(RHIBuffer *vb, RHIBuffer *ib, RHIBuffer *instB,
                              UINT indexCount, UINT instanceCount,
                              UINT instanceOffset) override;
    void GetSize(int &w, int &h) const override;
    void Resize(int newW, int newH) override;
    std::shared_ptr<RHIPipeline>
    CreateComputePipeline(const std::wstring &csPath) override;
    std::shared_ptr<RHITexture> CreateUAVTexture(int w, int h,
                                                 int format = 0) override;
    void SetComputePipeline(RHIPipeline *pipeline) override;
    void SetComputeUniforms(RHIBuffer *buffer, const void *data, size_t size,
                            int slot) override;
    void SetComputeTextureSRV(RHITexture *texture, int slot) override;
    void SetComputeTextureUAV(RHITexture *texture, int slot) override;
    void DispatchCompute(UINT groupX, UINT groupY, UINT groupZ) override;
    void ComputeBarrier(RHITexture *uavTexture) override;
    void SetComputeBufferUAV(RHIBuffer *buffer, int slot) override;
    void ComputeBufferBarrier(RHIBuffer *buffer) override;
};
