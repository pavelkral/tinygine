#pragma once

#include "pch/Pch.h"
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

class ShaderCompiler {
public:
    // DX11 FXC (Shader Model 5.0)
    static ComPtr<ID3DBlob> CompileDX11(const std::wstring& path, const char* entry, const char* target);

    // DX12 DXC (Shader Model 6.0)
    static ComPtr<IDxcBlob> CompileDX12(const std::wstring& path, const char* entry, const char* target);

    // Vulkan DXC, to SPIR-V
    static std::vector<uint8_t> CompileVulkan(const std::wstring& path, const char* entry, const char* target);
};