#include "rhi/utils/ShaderCompiler.h"

// ==============================================================================
// DIRECTX 11 (FXC)
// ==============================================================================
ComPtr<ID3DBlob> ShaderCompiler::CompileDX11(const std::wstring& path, const char* entry, const char* target) {
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0, &shaderBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            MessageBoxA(0, (char*)errorBlob->GetBufferPointer(), "DX11 Shader Compile Error", MB_OK | MB_ICONERROR);
        }
        else {
            MessageBoxW(0, path.c_str(), L"DX11: Shader file not found!", MB_OK | MB_ICONERROR);
        }
        return nullptr;
    }
    return shaderBlob;
}

// ==============================================================================
// DIRECTX 12 (DXC)
// ==============================================================================
ComPtr<IDxcBlob> ShaderCompiler::CompileDX12(const std::wstring& path, const char* entry, const char* target) {
    ComPtr<IDxcUtils> utils;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

    ComPtr<IDxcCompiler3> compiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

    ComPtr<IDxcIncludeHandler> includeHandler;
    utils->CreateDefaultIncludeHandler(&includeHandler);

    ComPtr<IDxcBlobEncoding> sourceBlob;
    if (FAILED(utils->LoadFile(path.c_str(), nullptr, &sourceBlob))) {
        MessageBoxW(0, path.c_str(), L"DX12: Shader file not found!", MB_OK | MB_ICONERROR);
        return nullptr;
    }

    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    BOOL known = FALSE; UINT32 encoding = 0;
    sourceBlob->GetEncoding(&known, &encoding);
    sourceBuffer.Encoding = known ? encoding : DXC_CP_ACP;

    wchar_t wEntry[64], wTarget[64];
    MultiByteToWideChar(CP_UTF8, 0, entry, -1, wEntry, 64);
    MultiByteToWideChar(CP_UTF8, 0, target, -1, wTarget, 64);

    std::vector<LPCWSTR> args = { path.c_str(), L"-E", wEntry, L"-T", wTarget };

    ComPtr<IDxcResult> results;
    compiler->Compile(&sourceBuffer, args.data(), (UINT32)args.size(), includeHandler.Get(), IID_PPV_ARGS(&results));

    ComPtr<IDxcBlobUtf8> errors;
    results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0) {
        OutputDebugStringA(errors->GetStringPointer());
        MessageBoxA(0, errors->GetStringPointer(), "DX12 Shader Compile Error", MB_OK | MB_ICONERROR);
        return nullptr;
    }

    ComPtr<IDxcBlob> shaderBlob;
    results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    return shaderBlob;
}

// ==============================================================================
// VULKAN (DXC -> SPIR-V)
// ==============================================================================
std::vector<uint8_t> ShaderCompiler::CompileVulkan(const std::wstring& path, const char* entry, const char* target) {
    ComPtr<IDxcUtils> utils;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

    ComPtr<IDxcCompiler3> compiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

    ComPtr<IDxcIncludeHandler> includeHandler;
    utils->CreateDefaultIncludeHandler(&includeHandler);

    ComPtr<IDxcBlobEncoding> sourceBlob;
    if (FAILED(utils->LoadFile(path.c_str(), nullptr, &sourceBlob))) {
        MessageBoxW(0, path.c_str(), L"Vulkan: Shader file not found!", MB_OK | MB_ICONERROR);
        return {};
    }

    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    BOOL known = FALSE; UINT32 encoding = 0;
    sourceBlob->GetEncoding(&known, &encoding);
    sourceBuffer.Encoding = known ? encoding : DXC_CP_ACP;

    wchar_t wEntry[64], wTarget[64];
    MultiByteToWideChar(CP_UTF8, 0, entry, -1, wEntry, 64);
    MultiByteToWideChar(CP_UTF8, 0, target, -1, wTarget, 64);

    //  VULKAN (SPIR-V) + SHIFs + ENTRY
    std::vector<LPCWSTR> args = {
        path.c_str(), L"-E", wEntry, L"-T", wTarget,
        L"-spirv", L"-fspv-target-env=vulkan1.1",
        L"-fvk-b-shift", L"0",  L"0",
        L"-fvk-u-shift", L"15", L"0",
        L"-fvk-t-shift", L"3",  L"0",
        L"-fvk-s-shift", L"11", L"0",
        L"-D", L"VULKAN"
    };

    ComPtr<IDxcResult> results;
    compiler->Compile(&sourceBuffer, args.data(), (UINT32)args.size(), includeHandler.Get(), IID_PPV_ARGS(&results));
    ComPtr<IDxcBlobUtf8> errors;
    results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0) {
        OutputDebugStringA(errors->GetStringPointer());
        MessageBoxA(0, errors->GetStringPointer(), "Vulkan Shader Compile Error", MB_OK | MB_ICONERROR);
        return {};
    }

    ComPtr<IDxcBlob> shaderBlob;
    results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    if (!shaderBlob) return {};

    std::vector<uint8_t> spirv(shaderBlob->GetBufferSize());
    memcpy(spirv.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
    return spirv;
}