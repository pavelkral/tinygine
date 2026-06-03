#include "rhi/RHI_DX11.h"
#include "engine/EngineDependencies.h"

bool RHI_DX11::Init(HWND hWnd, int w, int h) {
    width = w;
    height = h;

    // 1. setup device creation flags (debug layer in debug builds)
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    // enable debug layer in debug builds, but handle the case where it's not
    // available (e.g. on end-user machines)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    //  device a swap chain
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &swap, &dev, nullptr, &ctx);

    //  Fallback: if debug layer is requested but not available, try again
    // without it
    if (FAILED(hr) && (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG)) {
        OutputDebugStringA("[RHI_DX11] warning: DirectX 11 Debug Layer not "
                           "available. Initializing without debug...\n");
        createDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, nullptr,
            0, D3D11_SDK_VERSION, &sd, &swap, &dev, nullptr, &ctx);
    }

    if (FAILED(hr))
        return false;

    //  (Break on Error)
#ifdef _DEBUG
    if (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG) {
        ComPtr<ID3D11Debug> d3dDebug;
        if (SUCCEEDED(dev.As(&d3dDebug))) {
            ComPtr<ID3D11InfoQueue> d3dInfoQueue;
            if (SUCCEEDED(d3dDebug.As(&d3dInfoQueue))) {
                // Break on critical errors or memory corruption
                d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION,
                                                 true);
                d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);

                // Uncomment if you want to break on warnings as well
                // d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING,
                // true);
            }
        }
    }
#endif

    // Initialize BackBuffer
    m_backBuffer = std::make_shared<DX11Texture>();
    ComPtr<ID3D11Resource> bb;
    swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    dev->CreateRenderTargetView(bb.Get(), 0, &m_backBuffer->rtv);

    // create depth buffer and view
    D3D11_TEXTURE2D_DESC dd = {(UINT)w,
                               (UINT)h,
                               1,
                               1,
                               DXGI_FORMAT_D24_UNORM_S8_UINT,
                               {1, 0},
                               D3D11_USAGE_DEFAULT,
                               D3D11_BIND_DEPTH_STENCIL};
    ComPtr<ID3D11Texture2D> db;
    dev->CreateTexture2D(&dd, 0, &db);
    dev->CreateDepthStencilView(db.Get(), 0, &dsv);

    // Sampler for regular textures (Anisotropic)
    D3D11_SAMPLER_DESC smp = {};
    smp.Filter = D3D11_FILTER_ANISOTROPIC;
    smp.MaxAnisotropy = 16;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    smp.MinLOD = 0.0f;
    smp.MaxLOD = D3D11_FLOAT32_MAX;
    dev->CreateSamplerState(&smp, &samp);

    // Sampler shadow (Comparison sampler)
    D3D11_SAMPLER_DESC ss = {};
    ss.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    ss.BorderColor[0] = ss.BorderColor[1] = ss.BorderColor[2] =
        ss.BorderColor[3] = 1.0f;
    ss.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    dev->CreateSamplerState(&ss, &shadowSamp);

    //  Buffer  Push Constants (Object Data)
    UINT cbSize = (sizeof(ObjectData) + 255) & ~255; //  256
    D3D11_BUFFER_DESC pcb = {cbSize,
                             D3D11_USAGE_DYNAMIC,
                             D3D11_BIND_CONSTANT_BUFFER,
                             D3D11_CPU_ACCESS_WRITE,
                             0,
                             0};
    dev->CreateBuffer(&pcb, nullptr, &m_pushConstantBuffer);

    return true;
}

RHITexture *RHI_DX11::GetBackBuffer() { return m_backBuffer.get(); }

std::shared_ptr<RHITexture> RHI_DX11::CreateRenderTarget(int w, int h,
                                                         int format) {
    auto t = std::make_shared<DX11Texture>();
    t->width = w;
    t->height = h;
    DXGI_FORMAT fmt = (format == 0)   ? DXGI_FORMAT_R8G8B8A8_UNORM
                      : (format == 1) ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                      : DXGI_FORMAT_R32G32B32A32_FLOAT;

    D3D11_TEXTURE2D_DESC td = {(UINT)w,
                               (UINT)h,
                               1,
                               1,
                               fmt,
                               {1, 0},
                               D3D11_USAGE_DEFAULT,
                               D3D11_BIND_RENDER_TARGET |
                                   D3D11_BIND_SHADER_RESOURCE};
    ComPtr<ID3D11Texture2D> tex;
    dev->CreateTexture2D(&td, nullptr, &tex);
    dev->CreateRenderTargetView(tex.Get(), nullptr, &t->rtv);
    dev->CreateShaderResourceView(tex.Get(), nullptr, &t->srv);
    return t;
}

void RHI_DX11::SetMRTTargets(std::vector<RHITexture *> targets,
                             RHITexture *depthMap) {
    std::vector<ID3D11RenderTargetView *> rtvs;
    for (auto t : targets) {
        if (t)
            rtvs.push_back(((DX11Texture *)t)->rtv.Get());
    }

	// depth map override: if a depth map is provided, use its DSV instead of the
    ID3D11DepthStencilView *dsvLocal =
        depthMap ? ((DX11Texture *)depthMap)->dsv.Get() : dsv.Get();
    ctx->OMSetRenderTargets((UINT)rtvs.size(), rtvs.data(), dsvLocal);

    D3D11_VIEWPORT vp = {0, 0, (float)width, (float)height, 0, 1};
    ctx->RSSetViewports(1, &vp);
}

void RHI_DX11::ClearRenderTarget(RHITexture *target, const float color[4]) {
    if (target && ((DX11Texture *)target)->rtv)
        ctx->ClearRenderTargetView(((DX11Texture *)target)->rtv.Get(), color);
}

void RHI_DX11::ClearDepthTarget(RHITexture *depthTarget, float depth,
                                uint8_t stencil) {
    ID3D11DepthStencilView *dsvLocal =
        depthTarget ? ((DX11Texture *)depthTarget)->dsv.Get() : dsv.Get();
    if (dsvLocal)
        ctx->ClearDepthStencilView(dsvLocal, D3D11_CLEAR_DEPTH, depth, stencil);
}

void RHI_DX11::SetMainPassTarget() {
    SetMRTTargets({m_backBuffer.get()}, nullptr);
}

std::shared_ptr<RHITexture> RHI_DX11::CreateShadowTexture(int w, int h) {
    auto t = std::make_shared<DX11Texture>();
    t->width = w;
    t->height = h;
    D3D11_TEXTURE2D_DESC td = {(UINT)w,
                               (UINT)h,
                               1,
                               1,
                               DXGI_FORMAT_R32_TYPELESS,
                               {1, 0},
                               D3D11_USAGE_DEFAULT,
                               D3D11_BIND_DEPTH_STENCIL |
                                   D3D11_BIND_SHADER_RESOURCE};
    ComPtr<ID3D11Texture2D> tex;
    dev->CreateTexture2D(&td, nullptr, &tex);
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};
    dsvd.Format = DXGI_FORMAT_D32_FLOAT;
    dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dev->CreateDepthStencilView(tex.Get(), &dsvd, &t->dsv);
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = DXGI_FORMAT_R32_FLOAT;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    dev->CreateShaderResourceView(tex.Get(), &srvd, &t->srv);
    return t;
}

void RHI_DX11::BeginShadowPass(RHITexture *t) {
    auto dxt = (DX11Texture *)t;
    ctx->OMSetRenderTargets(0, nullptr, dxt->dsv.Get());
    ctx->ClearDepthStencilView(dxt->dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    D3D11_VIEWPORT vp = {0.0f, 0.0f, (float)t->width, (float)t->height,
                         0.0f, 1.0f};
    ctx->RSSetViewports(1, &vp);
}

void RHI_DX11::ImGuiInit(HWND hWnd) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(dev.Get(), ctx.Get());
}

void RHI_DX11::ImGuiBegin() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void RHI_DX11::ImGuiEnd() {
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void RHI_DX11::ImGuiCleanup() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

std::shared_ptr<RHIBuffer> RHI_DX11::CreateBuffer(BufferType type,
                                                  const void *data, size_t size,
                                                  UINT stride) {
    auto b = std::make_shared<DX11Buffer>();
    b->stride = stride;
    UINT bufferSize = (UINT)size;

    if (type == BufferType::ComputeUAV) {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = bufferSize;
        bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = stride;

        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = data;
        dev->CreateBuffer(&bd, data ? &sd : nullptr, &b->buf);

        if (b->buf) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.NumElements = bufferSize / stride;
            dev->CreateUnorderedAccessView(b->buf.Get(), &uavDesc, &b->uav);
        }

        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.ByteWidth = bufferSize;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        dev->CreateBuffer(&vbDesc, data ? &sd : nullptr, &b->vb_copy);

        return b;
    }

    if (type == BufferType::Constant)
        bufferSize = (bufferSize + 15) & ~15;

    D3D11_BUFFER_DESC bd = {
                            bufferSize, D3D11_USAGE_DYNAMIC, 0, D3D11_CPU_ACCESS_WRITE, 0, 0};
    if (type == BufferType::Vertex || type == BufferType::Instance)
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    if (type == BufferType::Index)
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    if (type == BufferType::Constant)
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA sd = {data, 0, 0};
    dev->CreateBuffer(&bd, data ? &sd : nullptr, &b->buf);
    return b;
}

void RHI_DX11::UpdateBuffer(RHIBuffer *b, const void *d, size_t s) {
    auto dxb = (DX11Buffer *)b;
    if (!dxb || !dxb->buf)
        return;

    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(ctx->Map(dxb->buf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, d, s);
        ctx->Unmap(dxb->buf.Get(), 0);
    }
}

std::shared_ptr<RHITexture> RHI_DX11::CreateTexture(const std::wstring &path) {
    auto t = std::make_shared<DX11Texture>();
    int tw, th, tc;
    std::string p(path.begin(), path.end());
    unsigned char *d = stbi_load(p.c_str(), &tw, &th, &tc, 4);
    if (!d)
        return nullptr;
    t->width = tw;
    t->height = th;
    auto mips = GenerateMipChain(d, tw, th);
    stbi_image_free(d);
    std::vector<D3D11_SUBRESOURCE_DATA> subData(mips.size());
    for (size_t i = 0; i < mips.size(); ++i) {
        subData[i].pSysMem = mips[i].pixels.data();
        subData[i].SysMemPitch = mips[i].width * 4;
    }
    D3D11_TEXTURE2D_DESC td = {(UINT)tw,
                               (UINT)th,
                               (UINT)mips.size(),
                               1,
                               DXGI_FORMAT_R8G8B8A8_UNORM,
                               {1, 0},
                               D3D11_USAGE_DEFAULT,
                               D3D11_BIND_SHADER_RESOURCE};
    ComPtr<ID3D11Texture2D> tex;
    dev->CreateTexture2D(&td, subData.data(), &tex);
    dev->CreateShaderResourceView(tex.Get(), 0, &t->srv);
    return t;
}
std::shared_ptr<RHITexture> RHI_DX11::CreateUAVTexture3D(int width, int height, int depth, int format) {
    auto t = std::make_shared<DX11Texture>();
    t->width = width;
    t->height = height;

    // Pokud format == 1, použijeme R16G16B16A16 (např. pro atmosférické LUT), jinak klasický 8-bit
    DXGI_FORMAT fmt = (format == 1) ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;

    // 1. Vytvoření 3D Textury (D3D11_TEXTURE3D_DESC místo 2D)
    D3D11_TEXTURE3D_DESC desc3D = {};
    desc3D.Width = width;
    desc3D.Height = height;
    desc3D.Depth = depth;
    desc3D.MipLevels = 1;
    desc3D.Format = fmt;
    desc3D.Usage = D3D11_USAGE_DEFAULT;
    desc3D.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    ComPtr<ID3D11Texture3D> tex3D;
    HRESULT hr = dev->CreateTexture3D(&desc3D, nullptr, &tex3D);
    if (FAILED(hr)) {
        return nullptr;
    }

    // 2. Vytvoření SRV (Pro čtení v Raymarch shaderu)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = fmt;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D; // MUSÍ BÝT 3D!
    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.MipLevels = 1;
    dev->CreateShaderResourceView(tex3D.Get(), &srvDesc, &t->srv);

    // 3. Vytvoření UAV (Pro zápis z Compute Shaderu šumu)
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = fmt;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D; // MUSÍ BÝT 3D!
    uavDesc.Texture3D.MipSlice = 0;
    uavDesc.Texture3D.FirstWSlice = 0;
    uavDesc.Texture3D.WSize = depth;
    dev->CreateUnorderedAccessView(tex3D.Get(), &uavDesc, &t->uav);

    return t;
}
std::shared_ptr<RHITexture>
RHI_DX11::CreateDDSTexture(const std::wstring &path) {
    std::string stringPath(path.begin(), path.end());
    auto tex = mt::Texture::from_file(stringPath);
    if (!tex.ok())
        return nullptr;

    auto dxt = std::make_shared<DX11Texture>();
    dxt->width = tex.desc().width;
    dxt->height = tex.desc().height;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = tex.desc().width;
    desc.Height = tex.desc().height;
    desc.MipLevels = tex.desc().mip_count;
    desc.ArraySize = mt::physical_layer_count(tex.desc());
    desc.Format = static_cast<DXGI_FORMAT>(mt::to_dxgi_format(tex.desc().format));
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (tex.desc().is_cube)
        desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    std::vector<D3D11_SUBRESOURCE_DATA> initData(tex.subresources().size());
    for (size_t i = 0; i < tex.subresources().size(); ++i) {
        initData[i].pSysMem = tex.subresources()[i].data;
        initData[i].SysMemPitch = tex.subresources()[i].row_pitch;
        initData[i].SysMemSlicePitch = tex.subresources()[i].slice_pitch;
    }

    ComPtr<ID3D11Texture2D> pTexture;
    dev->CreateTexture2D(&desc, initData.data(), &pTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    if (tex.desc().is_cube) {
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = desc.MipLevels;
        srvDesc.TextureCube.MostDetailedMip = 0;
    } else {
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
    }
    dev->CreateShaderResourceView(pTexture.Get(), &srvDesc, &dxt->srv);
    return dxt;
}

std::shared_ptr<RHIPipeline>
RHI_DX11::CreatePipeline(const PipelineConfig &config) {
    auto p = std::make_shared<DX11Pipeline>();
    ComPtr<ID3DBlob> vsB, psB, err;
    D3DCompileFromFile(config.vsPath.c_str(), nullptr,
                       D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", 0,
                       0, &vsB, &err);
    if (!config.psPath.empty())
        D3DCompileFromFile(config.psPath.c_str(), nullptr,
                           D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", 0,
                           0, &psB, &err);
    dev->CreateVertexShader(vsB->GetBufferPointer(), vsB->GetBufferSize(), 0,
                            &p->vs);
    if (psB)
        dev->CreatePixelShader(psB->GetBufferPointer(), psB->GetBufferSize(), 0,
                               &p->ps);

    if (config.vsPath.find(L"fullscreen") == std::wstring::npos) {
        std::vector<D3D11_INPUT_ELEMENT_DESC> ied = {
                                                     {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
             D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
             D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
             D3D11_INPUT_PER_VERTEX_DATA, 0}};

        if (config.isSkinned) {
            ied.push_back({"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, 32,
                           D3D11_INPUT_PER_VERTEX_DATA, 0});
            ied.push_back({"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48,
                           D3D11_INPUT_PER_VERTEX_DATA, 0});
        }
        if (config.useInstancing) {
            ied.push_back({"TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
                           D3D11_INPUT_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16,
                           D3D11_INPUT_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32,
                           D3D11_INPUT_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48,
                           D3D11_INPUT_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 7, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64,
                           D3D11_INPUT_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 8, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 80,
                           D3D11_INPUT_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 9, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 96,
                           D3D11_INPUT_PER_INSTANCE_DATA, 1});
        }
        dev->CreateInputLayout(ied.data(), (UINT)ied.size(),
                               vsB->GetBufferPointer(), vsB->GetBufferSize(),
                               &p->il);
    }

    p->top = config.topology == Topology::LineList
                 ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST
                 : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = config.fillMode == FillMode::Wireframe ? D3D11_FILL_WIREFRAME
                                                         : D3D11_FILL_SOLID;
    rd.CullMode =
        config.cullMode == CullMode::None ? D3D11_CULL_NONE : D3D11_CULL_BACK;
    rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &p->rs);
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = config.depthTest;
    dsd.DepthWriteMask = config.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL
                                           : D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    dev->CreateDepthStencilState(&dsd, &p->ds);

    D3D11_BLEND_DESC bDesc = {};
    if (config.isAdditive) {
        bDesc.RenderTarget[0].BlendEnable = TRUE;
        bDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        bDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        bDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    } else if (config.isTransparent) {
        bDesc.RenderTarget[0].BlendEnable = TRUE;
        bDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        bDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    }
    else if (config.enableBlend) {
        // --- NOVÉ: Klasický Alpha Blending pro Upscale mraků ---
        bDesc.RenderTarget[0].BlendEnable = TRUE;
        bDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        bDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        bDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    }
    else {
        bDesc.RenderTarget[0].BlendEnable = FALSE;
    }
    bDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    for (int i = 1; i < config.numRenderTargets; i++) {
        bDesc.RenderTarget[i] = bDesc.RenderTarget[0];
    }

    dev->CreateBlendState(&bDesc, &p->blendState);
    return p;
}

void RHI_DX11::BeginFrame() {
    ID3D11ShaderResourceView *nullSRVs[8] = {nullptr};
    ctx->PSSetShaderResources(0, 8, nullSRVs);
}

void RHI_DX11::EndFrame() { swap->Present(m_vsync ? 1 : 0, 0); }

void RHI_DX11::SetPipeline(RHIPipeline *p) {
    auto dx = (DX11Pipeline *)p;
    m_currentPipeline = p;
    if (dx->il)
        ctx->IASetInputLayout(dx->il.Get());
    ctx->VSSetShader(dx->vs.Get(), 0, 0);
    ctx->PSSetShader(dx->ps.Get(), 0, 0);
    ctx->RSSetState(dx->rs.Get());
    ctx->OMSetDepthStencilState(dx->ds.Get(), 0);
    ctx->IASetPrimitiveTopology(dx->top);

    float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ctx->OMSetBlendState(dx->blendState.Get(), blendFactor, 0xffffffff);
}

void RHI_DX11::SetTexture(RHITexture *t, int slot) {
    if (!t) {
        ID3D11ShaderResourceView *nullSRV = nullptr;
        ctx->PSSetShaderResources(slot, 1, &nullSRV);
        return;
    }
    auto dxt = (DX11Texture *)t;
    ctx->PSSetShaderResources(slot, 1, dxt->srv.GetAddressOf());
    ctx->PSSetSamplers(0, 1, samp.GetAddressOf());
    ctx->PSSetSamplers(1, 1, shadowSamp.GetAddressOf());
}

void RHI_DX11::SetGlobalUniforms(RHIBuffer *b, const void *d, size_t s) {
    UpdateBuffer(b, d, s);
    ctx->VSSetConstantBuffers(0, 1, ((DX11Buffer *)b)->buf.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, ((DX11Buffer *)b)->buf.GetAddressOf());
}

void RHI_DX11::SetPushConstants(const void *data, size_t size) {
    if (!m_pushConstantBuffer)
        return;
    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(ctx->Map(m_pushConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
                           0, &ms))) {
        memcpy(ms.pData, data, size);
        ctx->Unmap(m_pushConstantBuffer.Get(), 0);
        ctx->VSSetConstantBuffers(1, 1, m_pushConstantBuffer.GetAddressOf());
        ctx->PSSetConstantBuffers(1, 1, m_pushConstantBuffer.GetAddressOf());
    }
}

void RHI_DX11::SetObjectUniforms(RHIBuffer *b, const void *d, size_t s) {
    UpdateBuffer(b, d, s);
    ctx->VSSetConstantBuffers(1, 1, ((DX11Buffer *)b)->buf.GetAddressOf());
    ctx->PSSetConstantBuffers(1, 1, ((DX11Buffer *)b)->buf.GetAddressOf());
}

void RHI_DX11::SetBoneUniforms(RHIBuffer *b, const void *d, size_t s) {
    UpdateBuffer(b, d, s);
    ctx->VSSetConstantBuffers(2, 1, ((DX11Buffer *)b)->buf.GetAddressOf());
}

void RHI_DX11::DrawIndexed(RHIBuffer *vb, RHIBuffer *ib, UINT c) {
    auto v = (DX11Buffer *)vb;
    auto i = (DX11Buffer *)ib;
    UINT stride = v->stride > 0 ? v->stride : sizeof(Vertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, v->buf.GetAddressOf(), &stride, &offset);
    ctx->IASetIndexBuffer(i->buf.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->DrawIndexed(c, 0, 0);
}

void RHI_DX11::Draw(RHIBuffer *vb, UINT c) {
    if (vb) {
        auto v = (DX11Buffer *)vb;
        UINT stride = v->stride > 0 ? v->stride : sizeof(Vertex);
        UINT offset = 0;
        ctx->IASetVertexBuffers(0, 1, v->buf.GetAddressOf(), &stride, &offset);
    }
    ctx->Draw(c, 0);
}

void RHI_DX11::DrawIndexedInstanced(RHIBuffer *vb, RHIBuffer *ib,
                                    RHIBuffer *instB, UINT indexCount,
                                    UINT instanceCount, UINT instanceOffset) {
    auto v = (DX11Buffer *)vb;
    auto i = (DX11Buffer *)ib;
    auto inst = (DX11Buffer *)instB;

    ID3D11Buffer *instBufferToBind =
        inst->vb_copy ? inst->vb_copy.Get() : inst->buf.Get();
    ID3D11Buffer *buffers[2] = {v->buf.Get(), instBufferToBind};

    UINT strides[2] = {v->stride > 0 ? v->stride : (UINT)sizeof(Vertex),
                       inst->stride > 0 ? inst->stride
                                        : (UINT)sizeof(ObjectData)};
    UINT offsets[2] = {0, 0};
    ctx->IASetVertexBuffers(0, 2, buffers, strides, offsets);
    ctx->IASetIndexBuffer(i->buf.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, instanceOffset);
}

void RHI_DX11::GetSize(int &w, int &h) const {
    w = width;
    h = height;
}

void RHI_DX11::Resize(int newW, int newH) {
    if (newW == 0 || newH == 0 || (newW == width && newH == height))
        return;
    width = newW;
    height = newH;

    if (!ctx || !swap)
        return;

    // Uvoln�n� v�eho ze star�ho bufferu
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    if (m_backBuffer)
        m_backBuffer->rtv.Reset();
    dsv.Reset();

    HRESULT hr =
        swap->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr))
        return;

    ComPtr<ID3D11Resource> bb;
    swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (!m_backBuffer)
        m_backBuffer = std::make_shared<DX11Texture>();
    dev->CreateRenderTargetView(bb.Get(), 0, &m_backBuffer->rtv);

    // Znovu vytvo��me Depth Buffer
    D3D11_TEXTURE2D_DESC dd = {(UINT)width,
                               (UINT)height,
                               1,
                               1,
                               DXGI_FORMAT_D24_UNORM_S8_UINT,
                               {1, 0},
                               D3D11_USAGE_DEFAULT,
                               D3D11_BIND_DEPTH_STENCIL};
    ComPtr<ID3D11Texture2D> db;
    dev->CreateTexture2D(&dd, 0, &db);
    dev->CreateDepthStencilView(db.Get(), 0, &dsv);
}

std::shared_ptr<RHIPipeline>
RHI_DX11::CreateComputePipeline(const std::wstring &csPath) {
    auto p = std::make_shared<DX11Pipeline>();
    ComPtr<ID3DBlob> csB, err;
    HRESULT hr = D3DCompileFromFile(csPath.c_str(), nullptr,
                                    D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain",
                                    "cs_5_0", 0, 0, &csB, &err);
    if (FAILED(hr))
        return nullptr;
    dev->CreateComputeShader(csB->GetBufferPointer(), csB->GetBufferSize(),
                             nullptr, &p->cs);
    return p;
}

std::shared_ptr<RHITexture> RHI_DX11::CreateUAVTexture(int w, int h,
                                                       int format) {
    auto t = std::make_shared<DX11Texture>();
    t->width = w;
    t->height = h;
    DXGI_FORMAT fmt = (format == 1) ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                    : DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = fmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    ComPtr<ID3D11Texture2D> tex;
    dev->CreateTexture2D(&desc, nullptr, &tex);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = fmt;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    dev->CreateShaderResourceView(tex.Get(), &srvDesc, &t->srv);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = fmt;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    dev->CreateUnorderedAccessView(tex.Get(), &uavDesc, &t->uav);

    return t;
}

void RHI_DX11::SetComputePipeline(RHIPipeline *pipeline) {
    auto dx = (DX11Pipeline *)pipeline;
    ctx->CSSetShader(dx->cs.Get(), nullptr, 0);
}

void RHI_DX11::SetComputeUniforms(RHIBuffer *buffer, const void *data,
                                  size_t size, int slot) {
    UpdateBuffer(buffer, data, size);
    ctx->CSSetConstantBuffers(slot, 1,
                              ((DX11Buffer *)buffer)->buf.GetAddressOf());
}

void RHI_DX11::SetComputeTextureSRV(RHITexture *texture, int slot) {
    if (!texture) {
        ID3D11ShaderResourceView *nullSRV = nullptr;
        ctx->CSSetShaderResources(slot, 1, &nullSRV);
        return;
    }
    auto dxt = (DX11Texture *)texture;
    ctx->CSSetShaderResources(slot, 1, dxt->srv.GetAddressOf());
    ctx->CSSetSamplers(0, 1, samp.GetAddressOf());
}

void RHI_DX11::SetComputeTextureUAV(RHITexture *texture, int slot) {
    if (!texture) {
        ID3D11UnorderedAccessView *nullUAV = nullptr;
        ctx->CSSetUnorderedAccessViews(slot, 1, &nullUAV, nullptr);
        return;
    }
    auto dxt = (DX11Texture *)texture;
    ctx->CSSetUnorderedAccessViews(slot, 1, dxt->uav.GetAddressOf(), nullptr);
}

void RHI_DX11::DispatchCompute(UINT groupX, UINT groupY, UINT groupZ) {
    ctx->Dispatch(groupX, groupY, groupZ);
}

void RHI_DX11::ComputeBarrier(RHITexture *uavTexture) {
    ID3D11UnorderedAccessView *nullUAV[8] = {nullptr};
    ctx->CSSetUnorderedAccessViews(0, 8, nullUAV, nullptr);
}

void RHI_DX11::SetComputeBufferUAV(RHIBuffer *buffer, int slot) {
    if (!buffer)
        return;
    auto dxb = (DX11Buffer *)buffer;
    ctx->CSSetUnorderedAccessViews(slot + 1, 1, dxb->uav.GetAddressOf(), nullptr);
}

void RHI_DX11::ComputeBufferBarrier(RHIBuffer *buffer) {
    ID3D11UnorderedAccessView *nullUAV[4] = {nullptr};
    ctx->CSSetUnorderedAccessViews(0, 4, nullUAV, nullptr);
    if (!buffer)
        return;
    auto dx = (DX11Buffer *)buffer;
    if (dx->vb_copy)
        ctx->CopyResource(dx->vb_copy.Get(), dx->buf.Get());
}
