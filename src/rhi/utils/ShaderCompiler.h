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
    // Pro DX11 používáme starší, ale stabilní FXC (Shader Model 5.0)
    static ComPtr<ID3DBlob> CompileDX11(const std::wstring& path, const char* entry, const char* target);

    // Pro DX12 používáme moderní DXC (Shader Model 6.0)
    static ComPtr<IDxcBlob> CompileDX12(const std::wstring& path, const char* entry, const char* target);

    // Pro Vulkan používáme DXC, ale instruujeme ho k pøekladu do SPIR-V
    static std::vector<uint8_t> CompileVulkan(const std::wstring& path, const char* entry, const char* target);
};