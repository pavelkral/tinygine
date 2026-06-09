#pragma once

#include "pch/Pch.h"
#include "rhi/RHI.h"
#include "D3D12MemAlloc.h" // D3D12MA::Allocation backing every GPU resource

class RHI_DX12; // fwd decl for bindless slot recycling

struct DX12Buffer : public RHIBuffer {
    ComPtr<ID3D12Resource> res;
    // D3D12MA sub-allocation that owns the memory behind `res`. Must outlive the
    // resource (and be released together with it via deferred destruction).
    ComPtr<D3D12MA::Allocation> alloc;
    D3D12_VERTEX_BUFFER_VIEW vbv;
    D3D12_INDEX_BUFFER_VIEW ibv;
    UINT8* map = nullptr;
    UINT64 sizePerFrame = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE uavHandle = {};
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
};

struct DX12Texture : public RHITexture {
    ComPtr<ID3D12Resource> res;
    // D3D12MA sub-allocation that owns the memory behind `res` (see DX12Buffer).
    ComPtr<D3D12MA::Allocation> alloc;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE uavHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;

    // Bindless slot recycling: when this texture is destroyed it returns its
    // SRV heap slot to the owner so the bindless index pool does not leak.
    RHI_DX12* ownerRHI = nullptr;
    bool hasBindlessSlot = false;
    ~DX12Texture();
};

struct DX12Pipeline : public RHIPipeline {
    ComPtr<ID3D12PipelineState> pso;
    ComPtr<ID3D12RootSignature> rs;
    D3D_PRIMITIVE_TOPOLOGY top;
    bool usesBindlessTextures = false;
};
