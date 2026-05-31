#pragma once

#include "pch/Pch.h"
#include "rhi/RHI.h"

struct DX11Buffer : public RHIBuffer {
    ComPtr<ID3D11Buffer> buf;
    ComPtr<ID3D11UnorderedAccessView> uav;
    ComPtr<ID3D11Buffer> vb_copy;
};
struct DX11Texture : public RHITexture {
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11UnorderedAccessView> uav;
    ComPtr<ID3D11RenderTargetView> rtv;
};
struct DX11Pipeline : public RHIPipeline {
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11ComputeShader> cs;
    ComPtr<ID3D11InputLayout> il;
    ComPtr<ID3D11RasterizerState> rs;
    ComPtr<ID3D11DepthStencilState> ds;
    D3D11_PRIMITIVE_TOPOLOGY top;
    ComPtr<ID3D11BlendState> blendState;
};

