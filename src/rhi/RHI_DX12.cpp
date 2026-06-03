#include "rhi/RHI_DX12.h"
#include "engine/EngineDependencies.h"
#include "rhi/utils/ShaderCompiler.h"
void RHI_DX12::Sync() {
    fVal++;
    queue->Signal(fence.Get(), fVal);

    if (fence->GetCompletedValue() < fVal) {
        fence->SetEventOnCompletion(fVal, fEvt);
        WaitForSingleObject(fEvt, INFINITE);
    }
    fIdx = swap->GetCurrentBackBufferIndex();
}

void RHI_DX12::TransitionResource(DX12Texture *tex,
                                  D3D12_RESOURCE_STATES targetState) {
    if (!tex || tex->currentState == targetState || !tex->res)
        return;

    D3D12_RESOURCE_BARRIER rb = {};
    rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    rb.Transition.pResource = tex->res.Get();
    rb.Transition.StateBefore = tex->currentState;
    rb.Transition.StateAfter = targetState;

    cmd->ResourceBarrier(1, &rb);
    tex->currentState = targetState;
}

void RHI_DX12::TransitionBuffer(ID3D12Resource *res,
                                D3D12_RESOURCE_STATES before,
                                D3D12_RESOURCE_STATES after) {
    if (!res || before == after)
        return;

    D3D12_RESOURCE_BARRIER rb = {};
    rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    rb.Transition.pResource = res;
    rb.Transition.StateBefore = before;
    rb.Transition.StateAfter = after;

    cmd->ResourceBarrier(1, &rb);
}

bool RHI_DX12::Init(HWND hWnd, int width, int height) {
    w = width;
    h = height;

    // enable debug layer
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif

    // create device 
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&dev)))) {
        MessageBoxW(hWnd, L"Failed to create D3D12 Device", L"Fatal Error",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    //create command queue
    D3D12_COMMAND_QUEUE_DESC qd = {D3D12_COMMAND_LIST_TYPE_DIRECT};
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));

    // setup dxgi
    UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    ComPtr<IDXGIFactory4> fac;
    CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&fac));

    // check for tearing support
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(fac.As(&factory5))) {
        factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                      &tearingSupported, sizeof(tearingSupported));
    }

    // 5. Create SwapChain (Double Buffering)
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = w;
    sd.Height = h;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Stereo = FALSE;
    sd.SampleDesc = {1, 0}; // no MSAA antialiasing
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = FrameCount; // 2 Buffers (Front and Back)
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = (tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

    ComPtr<IDXGISwapChain1> sc;
    HRESULT hr = fac->CreateSwapChainForHwnd(queue.Get(), hWnd, &sd, nullptr,
                                             nullptr, &sc);

    if (FAILED(hr)) {
        tearingSupported = FALSE;
        sd.Flags = 0;
        hr = fac->CreateSwapChainForHwnd(queue.Get(), hWnd, &sd, nullptr, nullptr,
                                         &sc);
        if (FAILED(hr))
            return false;
    }

    sc.As(&swap);
    fac->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

    // 6. Create large memory blocks for descriptors (Descriptor Heaps)
    D3D12_DESCRIPTOR_HEAP_DESC rhd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256};
    dev->CreateDescriptorHeap(&rhd, IID_PPV_ARGS(&rtvH));

    // This heap must be huge (65536) because we will load hundreds of textures and
    // buffers
    D3D12_DESCRIPTOR_HEAP_DESC shd = {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                      65536,
                                      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    dev->CreateDescriptorHeap(&shd, IID_PPV_ARGS(&srvH));

    D3D12_DESCRIPTOR_HEAP_DESC dhd = {D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128};
    dev->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&dsvH));

    // Determine the size of a single descriptor on the current GPU
    sInc = dev->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    rtvInc =
        dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dsvInc =
        dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // 7. Link SwapChain windows (BackBuffers) to RTV Heap
    D3D12_CPU_DESCRIPTOR_HANDLE rH = rtvH->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < FrameCount; i++) {
        m_backBuffers[i] = std::make_shared<DX12Texture>();
        swap->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]->res));
        m_backBuffers[i]->currentState = D3D12_RESOURCE_STATE_PRESENT;

        D3D12_CPU_DESCRIPTOR_HANDLE handle = rH;
        handle.ptr += rtvOff * rtvInc;
        m_backBuffers[i]->rtvHandle = handle;

        dev->CreateRenderTargetView(m_backBuffers[i]->res.Get(), nullptr, handle);
        dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    IID_PPV_ARGS(&alc[i]));
        rtvOff++;
    }

    //  Create main depth buffer (Z-Buffer)
    D3D12_RESOURCE_DESC dsD = {D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                               0,
                               (UINT64)w,
                               (UINT)h,
                               1,
                               1,
                               DXGI_FORMAT_D32_FLOAT,
                               {1, 0},
                               D3D12_TEXTURE_LAYOUT_UNKNOWN,
                               D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL};
    D3D12_HEAP_PROPERTIES dHp = {D3D12_HEAP_TYPE_DEFAULT};
    D3D12_CLEAR_VALUE dCv = {DXGI_FORMAT_D32_FLOAT, {1.0f, 0}};

    dev->CreateCommittedResource(&dHp, D3D12_HEAP_FLAG_NONE, &dsD,
                                 D3D12_RESOURCE_STATE_DEPTH_WRITE, &dCv,
                                 IID_PPV_ARGS(&depth));
    dev->CreateDepthStencilView(depth.Get(), nullptr,
                                dsvH->GetCPUDescriptorHandleForHeapStart());

    // Create command list and fence
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alc[0].Get(),
                           nullptr, IID_PPV_ARGS(&cmd));
    cmd->Close();

    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    fEvt = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    fIdx = swap->GetCurrentBackBufferIndex();

    // 10. Create root signature for Compute Shaders
    CD3DX12_DESCRIPTOR_RANGE uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE bufUavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

    CD3DX12_ROOT_PARAMETER crp[4];
    crp[0].InitAsConstantBufferView(0);            // Global data
    crp[1].InitAsDescriptorTable(1, &uavRange);    // Output texture (Write)
    crp[2].InitAsDescriptorTable(1, &srvRange);    // Input texture (Read)
    crp[3].InitAsDescriptorTable(1, &bufUavRange); // Output buffer (Particles)

    CD3DX12_STATIC_SAMPLER_DESC smp(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    CD3DX12_ROOT_SIGNATURE_DESC crsd(4, crp, 1, &smp);

    ComPtr<ID3DBlob> sig, err;
    D3D12SerializeRootSignature(&crsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig,
                                err.GetAddressOf());
    dev->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                             IID_PPV_ARGS(&computeRS));

    return true;
}

void RHI_DX12::GetSize(int &outW, int &outH) const {
    outW = w;
    outH = h;
}

void RHI_DX12::Resize(int newW, int newH) {
    if (newW == 0 || newH == 0 || (newW == w && newH == h))
        return;

    w = newW;
    h = newH;

    // Stop GPU
    fVal++;
    queue->Signal(fence.Get(), fVal);
    if (fence->GetCompletedValue() < fVal) {
        fence->SetEventOnCompletion(fVal, fEvt);
        WaitForSingleObject(fEvt, INFINITE);
    }

    // Discard old textures
    for (int i = 0; i < FrameCount; i++) {
        m_backBuffers[i]->res.Reset();
        fenceValues[i] = fVal;
    }
    depth.Reset();

    // Resize swap chain
    DXGI_SWAP_CHAIN_DESC1 sd;
    swap->GetDesc1(&sd);
    swap->ResizeBuffers(FrameCount, w, h, sd.Format, sd.Flags);
    fIdx = swap->GetCurrentBackBufferIndex();

    // Create new RTVs
    D3D12_CPU_DESCRIPTOR_HANDLE rH = rtvH->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < FrameCount; i++) {
        swap->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]->res));
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rH;
        handle.ptr += i * rtvInc;
        m_backBuffers[i]->rtvHandle = handle;
        dev->CreateRenderTargetView(m_backBuffers[i]->res.Get(), nullptr, handle);
    }

    // Create new Depth Buffer
    D3D12_RESOURCE_DESC dsD = {D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                               0,
                               (UINT64)w,
                               (UINT)h,
                               1,
                               1,
                               DXGI_FORMAT_D32_FLOAT,
                               {1, 0},
                               D3D12_TEXTURE_LAYOUT_UNKNOWN,
                               D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL};
    D3D12_HEAP_PROPERTIES dHp = {D3D12_HEAP_TYPE_DEFAULT};
    D3D12_CLEAR_VALUE dCv = {DXGI_FORMAT_D32_FLOAT, {1.0f, 0}};
    dev->CreateCommittedResource(&dHp, D3D12_HEAP_FLAG_NONE, &dsD,
                                 D3D12_RESOURCE_STATE_DEPTH_WRITE, &dCv,
                                 IID_PPV_ARGS(&depth));
    dev->CreateDepthStencilView(depth.Get(), nullptr,
                                dsvH->GetCPUDescriptorHandleForHeapStart());
}

void RHI_DX12::ImGuiInit(HWND h) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

    ImFontConfig fontConfig;
    fontConfig.RasterizerMultiply = 1.75f;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;

    ImFont *font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf",
                                                19.0f, &fontConfig);
    if (!font)
        io.Fonts->AddFontDefault(&fontConfig);
    io.Fonts->Build();

    D3D12_DESCRIPTOR_HEAP_DESC dhd = {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1,
                                      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    dev->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&imguiH));

    ImGui_ImplWin32_Init(h);
    ImGui_ImplDX12_Init(dev.Get(), FrameCount, DXGI_FORMAT_R8G8B8A8_UNORM,
                        imguiH.Get(),
                        imguiH->GetCPUDescriptorHandleForHeapStart(),
                        imguiH->GetGPUDescriptorHandleForHeapStart());
}

void RHI_DX12::ImGuiBegin() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void RHI_DX12::ImGuiEnd() {
    ImGui::Render();

    ID3D12DescriptorHeap *h[] = {imguiH.Get()};
    cmd->SetDescriptorHeaps(1, h);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd.Get());

    // Return SRV heap for normal rendering
    ID3D12DescriptorHeap *main[] = {srvH.Get()};
    cmd->SetDescriptorHeaps(1, main);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(nullptr, (void *)cmd.Get());
    }
}

void RHI_DX12::ImGuiCleanup() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void RHI_DX12::BeginFrame() {
    // Wait for the previous frame's memory to be released
    if (fence->GetCompletedValue() < fenceValues[fIdx]) {
        fence->SetEventOnCompletion(fenceValues[fIdx], fEvt);
        WaitForSingleObject(fEvt, INFINITE);
    }

    alc[fIdx]->Reset();
    cmd->Reset(alc[fIdx].Get(), nullptr);
    lastPSO = nullptr;
    lastRS = nullptr;

    // Reset memory offsets for the new frame
    cOff = 0;
    oOff = 0;
    bOff = 0;
    compOff = 0;

    // Map the main memory block
    ID3D12DescriptorHeap *heaps[] = {srvH.Get()};
    cmd->SetDescriptorHeaps(1, heaps);
}

void RHI_DX12::EndFrame() {
    // Prepare the screen for presentation
    TransitionResource(m_backBuffers[fIdx].get(), D3D12_RESOURCE_STATE_PRESENT);

    cmd->Close();
    ID3D12CommandList *lists[] = {cmd.Get()};
    queue->ExecuteCommandLists(1, lists);

    swap->Present(m_vsync ? 1 : 0, (!m_vsync && tearingSupported)
                                       ? DXGI_PRESENT_ALLOW_TEARING
                                                                  : 0);

    // Lock the frame's memory
    const UINT64 f = fVal + 1;
    queue->Signal(fence.Get(), f);
    fenceValues[fIdx] = f;
    fVal = f;

    fIdx = swap->GetCurrentBackBufferIndex();
}

RHITexture *RHI_DX12::GetBackBuffer() { return m_backBuffers[fIdx].get(); }

std::shared_ptr<RHITexture> RHI_DX12::CreateRenderTarget(int w_in, int h_in,
                                                         int format) {
    auto t = std::make_shared<DX12Texture>();
    t->width = w_in;
    t->height = h_in;

    DXGI_FORMAT fmt = (format == 0)   ? DXGI_FORMAT_R8G8B8A8_UNORM
                      : (format == 1) ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                      : DXGI_FORMAT_R32G32B32A32_FLOAT;

    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = w_in;
    d.Height = h_in;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Format = fmt;
    d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // Set the default clear color
    D3D12_CLEAR_VALUE cv = {};
    cv.Format = fmt;
    if (format == 0) {
        cv.Color[0] = 0.0f;
        cv.Color[1] = 0.0f;
        cv.Color[2] = 0.0f;
        cv.Color[3] = 1.0f;
    } else {
        cv.Color[0] = 0.0f;
        cv.Color[1] = 0.0f;
        cv.Color[2] = 0.0f;
        cv.Color[3] = 0.0f;
    }

    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            &cv, IID_PPV_ARGS(&t->res)))) {
        return nullptr;
    }
    t->currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // Create RTV (Write to texture )
    D3D12_CPU_DESCRIPTOR_HANDLE rH = rtvH->GetCPUDescriptorHandleForHeapStart();
    rH.ptr += rtvOff * rtvInc;
    t->rtvHandle = rH;
    rtvOff++;
    dev->CreateRenderTargetView(t->res.Get(), nullptr, t->rtvHandle);

    // Create SRV (Read from texture in shader)
    D3D12_CPU_DESCRIPTOR_HANDLE sH = srvH->GetCPUDescriptorHandleForHeapStart();
    sH.ptr += sOff * sInc;
    t->srvHandle = srvH->GetGPUDescriptorHandleForHeapStart();
    t->srvHandle.ptr += sOff * sInc;
    sOff++;
    dev->CreateShaderResourceView(t->res.Get(), nullptr, sH);

    return t;
}

void RHI_DX12::SetMRTTargets(std::vector<RHITexture *> targets,
                             RHITexture *depthMap) {
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;

    for (auto t : targets) {
        auto dx = (DX12Texture *)t;
        TransitionResource(dx, D3D12_RESOURCE_STATE_RENDER_TARGET);
        handles.push_back(dx->rtvHandle);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    int passWidth = w;
    int passHeight = h;

    if (depthMap) {
        auto dxd = (DX12Texture *)depthMap;
        TransitionResource(dxd, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        dsvHandle = dxd->dsvHandle;

        // If we are not rendering color, take the resolution from the shadow map
        if (targets.empty()) {
            passWidth = dxd->width;
            passHeight = dxd->height;
        }
    } else {
        dsvHandle = dsvH->GetCPUDescriptorHandleForHeapStart();
    }

    cmd->OMSetRenderTargets((UINT)handles.size(), handles.data(), FALSE,
                            &dsvHandle);

    D3D12_VIEWPORT vp = {0, 0, (float)passWidth, (float)passHeight, 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, (LONG)passWidth, (LONG)passHeight};
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
}

void RHI_DX12::SetMainPassTarget() {
    SetMRTTargets({GetBackBuffer()}, nullptr);
}

void RHI_DX12::ClearRenderTarget(RHITexture *target, const float color[4]) {
    if (!target)
        return;
    auto dx = (DX12Texture *)target;
    TransitionResource(dx, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ClearRenderTargetView(dx->rtvHandle, color, 0, nullptr);
}

void RHI_DX12::ClearDepthTarget(RHITexture *depthTarget, float depthVal,
                                uint8_t stencil) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        depthTarget ? ((DX12Texture *)depthTarget)->dsvHandle
                                                     : dsvH->GetCPUDescriptorHandleForHeapStart();
    cmd->ClearDepthStencilView(handle, D3D12_CLEAR_FLAG_DEPTH, depthVal, stencil,
                               0, nullptr);
}

std::shared_ptr<RHITexture> RHI_DX12::CreateShadowTexture(int width,
                                                          int height) {
    auto t = std::make_shared<DX12Texture>();
    t->width = width;
    t->height = height;

    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = width;
    d.Height = height;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;

    // TYPELESS format allows us to write as D32 and read as R32
    d.Format = DXGI_FORMAT_R32_TYPELESS;
    d.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE cv = {};
    cv.Format = DXGI_FORMAT_D32_FLOAT;
    cv.DepthStencil.Depth = 1.0f;
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};

    // Texture starts in depth write mode
    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d,
                                            D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv,
                                            IID_PPV_ARGS(&t->res)))) {
        return nullptr;
    }
    t->currentState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    // Create DSV
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHdl =
        dsvH->GetCPUDescriptorHandleForHeapStart();
    dsvHdl.ptr += dsvInc;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvd = {};
    dsvd.Format = DXGI_FORMAT_D32_FLOAT;
    dsvd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dev->CreateDepthStencilView(t->res.Get(), &dsvd, dsvHdl);
    t->dsvHandle = dsvHdl;

    // Create SRV
    D3D12_CPU_DESCRIPTOR_HANDLE cpuH = srvH->GetCPUDescriptorHandleForHeapStart();
    cpuH.ptr += sOff * sInc;
    t->srvHandle = srvH->GetGPUDescriptorHandleForHeapStart();
    t->srvHandle.ptr += sOff * sInc;
    sOff++;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = DXGI_FORMAT_R32_FLOAT;
    srvd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvd.Texture2D.MipLevels = 1;
    dev->CreateShaderResourceView(t->res.Get(), &srvd, cpuH);

    return t;
}

void RHI_DX12::BeginShadowPass(RHITexture *t) {
    SetMRTTargets({}, t);
    ClearDepthTarget(t, 1.0f, 0);

    lastPSO = nullptr;
    lastRS = nullptr;

    ID3D12DescriptorHeap *heaps[] = {srvH.Get()};
    cmd->SetDescriptorHeaps(1, heaps);
}

std::shared_ptr<RHITexture>
RHI_DX12::CreateDDSTexture(const std::wstring &path) {
    std::string stringPath(path.begin(), path.end());
    auto tex = mt::Texture::from_file(stringPath);

    if (!tex.ok())
        return nullptr;

    auto t = std::make_shared<DX12Texture>();
    t->width = tex.desc().width;
    t->height = tex.desc().height;

    D3D12_RESOURCE_DESC rd = mt::make_d3d12_resource_desc(tex.desc());
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};

    // Each resource must start in the COMMON state in Vulkan and DX12
    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                            D3D12_RESOURCE_STATE_COMMON, nullptr,
                                            IID_PPV_ARGS(&t->res)))) {
        return nullptr;
    }

    UINT64 uploadSize = 0;
    dev->GetCopyableFootprints(&rd, 0, (UINT)tex.subresources().size(), 0,
                               nullptr, nullptr, nullptr, &uploadSize);
    if (uploadSize == 0)
        return nullptr;

	// create upload buffer (RAM) for texture data, this is a staging buffer between disk and VRAM
    ComPtr<ID3D12Resource> upB;
    D3D12_HEAP_PROPERTIES upHp = {D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC upD = {D3D12_RESOURCE_DIMENSION_BUFFER,
                               0,
                               uploadSize,
                               1,
                               1,
                               1,
                               DXGI_FORMAT_UNKNOWN,
                               {1, 0},
                               D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                               D3D12_RESOURCE_FLAG_NONE};

    if (FAILED(dev->CreateCommittedResource(&upHp, D3D12_HEAP_FLAG_NONE, &upD,
                                            D3D12_RESOURCE_STATE_GENERIC_READ,
                                            nullptr, IID_PPV_ARGS(&upB)))) {
        return nullptr;
    }

    // Copy data from disk to RAM buffer
    void *mapped = nullptr;
    upB->Map(0, nullptr, &mapped);
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(
        tex.subresources().size());
    dev->GetCopyableFootprints(&rd, 0, (UINT)tex.subresources().size(), 0,
                               layouts.data(), nullptr, nullptr, nullptr);

    for (size_t i = 0; i < tex.subresources().size(); ++i) {
        const auto &sr = tex.subresources()[i];
        uint8_t *dst = (uint8_t *)mapped + layouts[i].Offset;
        const uint8_t *src = (const uint8_t *)sr.data;

        for (uint32_t y = 0; y < sr.height; ++y) {
            memcpy(dst + y * layouts[i].Footprint.RowPitch, src + y * sr.row_pitch,
                   sr.row_pitch);
        }
    }
    upB->Unmap(0, nullptr);

    // Create a temporary command queue for copying from RAM to VRAM
    ComPtr<ID3D12CommandAllocator> tempAlloc;
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                IID_PPV_ARGS(&tempAlloc));
    ComPtr<ID3D12GraphicsCommandList> tempCmd;
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(),
                           nullptr, IID_PPV_ARGS(&tempCmd));

    for (size_t i = 0; i < tex.subresources().size(); i++) {
        D3D12_TEXTURE_COPY_LOCATION dL = {
                                          t->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, (UINT)i};
        D3D12_TEXTURE_COPY_LOCATION sL = {
                                          upB.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, layouts[i]};
        tempCmd->CopyTextureRegion(&dL, 0, 0, 0, &sL, nullptr);
    }

    // After copying, change the texture state to be readable in the shader
    D3D12_RESOURCE_BARRIER rb = {};
    rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    rb.Transition.pResource = t->res.Get();
    rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    tempCmd->ResourceBarrier(1, &rb);
    tempCmd->Close();

    ID3D12CommandList *l[] = {tempCmd.Get()};
    queue->ExecuteCommandLists(1, l);
    Sync(); // Wait for the copy to complete

    // Register the texture in the SRV heap
    t->currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_CPU_DESCRIPTOR_HANDLE h = srvH->GetCPUDescriptorHandleForHeapStart();
    h.ptr += sOff * sInc;
    t->srvHandle = srvH->GetGPUDescriptorHandleForHeapStart();
    t->srvHandle.ptr += sOff * sInc;
    sOff++;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvd = mt::make_d3d12_srv_desc(tex.desc());
    dev->CreateShaderResourceView(t->res.Get(), &srvd, h);

    return t;
}

std::shared_ptr<RHITexture> RHI_DX12::CreateTexture(const std::wstring &path) {
    auto t = std::make_shared<DX12Texture>();
    int tw, th, tc;
    std::string p(path.begin(), path.end());

    unsigned char *d = stbi_load(p.c_str(), &tw, &th, &tc, 4);
    if (!d)
        return nullptr;

    t->width = tw;
    t->height = th;

    auto mips = GenerateMipChain(d, tw, th);
    stbi_image_free(d);

    D3D12_RESOURCE_DESC td = {D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                              0,
                              (UINT64)tw,
                              (UINT)th,
                              1,
                              (UINT16)mips.size(),
                              DXGI_FORMAT_R8G8B8A8_UNORM,
                              {1, 0},
                              D3D12_TEXTURE_LAYOUT_UNKNOWN,
                              D3D12_RESOURCE_FLAG_NONE};
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};

    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
                                            D3D12_RESOURCE_STATE_COMMON, nullptr,
                                            IID_PPV_ARGS(&t->res)))) {
        return nullptr;
    }

    UINT64 upSize = 0;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(mips.size());
    std::vector<UINT> rows(mips.size());
    std::vector<UINT64> rowSizes(mips.size());
    dev->GetCopyableFootprints(&td, 0, mips.size(), 0, layouts.data(),
                               rows.data(), rowSizes.data(), &upSize);
    if (upSize == 0)
        return nullptr;

    ComPtr<ID3D12Resource> upB;
    D3D12_HEAP_PROPERTIES upHp = {D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC upD = {D3D12_RESOURCE_DIMENSION_BUFFER,
                               0,
                               upSize,
                               1,
                               1,
                               1,
                               DXGI_FORMAT_UNKNOWN,
                               {1, 0},
                               D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                               D3D12_RESOURCE_FLAG_NONE};
    if (FAILED(dev->CreateCommittedResource(&upHp, D3D12_HEAP_FLAG_NONE, &upD,
                                            D3D12_RESOURCE_STATE_GENERIC_READ,
                                            nullptr, IID_PPV_ARGS(&upB)))) {
        return nullptr;
    }

    void *mappedPtr;
    upB->Map(0, nullptr, &mappedPtr);
    for (size_t i = 0; i < mips.size(); ++i) {
        uint8_t *dst = (uint8_t *)mappedPtr + layouts[i].Offset;
        const uint8_t *src = (const uint8_t *)mips[i].pixels.data();
        for (UINT y = 0; y < rows[i]; y++) {
            memcpy(dst + y * layouts[i].Footprint.RowPitch,
                   src + y * mips[i].width * 4, mips[i].width * 4);
        }
    }
    upB->Unmap(0, nullptr);

    ComPtr<ID3D12CommandAllocator> tempAlloc;
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                IID_PPV_ARGS(&tempAlloc));
    ComPtr<ID3D12GraphicsCommandList> tempCmd;
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(),
                           nullptr, IID_PPV_ARGS(&tempCmd));

    for (UINT i = 0; i < mips.size(); i++) {
        D3D12_TEXTURE_COPY_LOCATION dL = {
                                          t->res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, i};
        D3D12_TEXTURE_COPY_LOCATION sL = {
                                          upB.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, layouts[i]};
        tempCmd->CopyTextureRegion(&dL, 0, 0, 0, &sL, nullptr);
    }

    D3D12_RESOURCE_BARRIER rb = {};
    rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    rb.Transition.pResource = t->res.Get();
    rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    tempCmd->ResourceBarrier(1, &rb);
    tempCmd->Close();

    ID3D12CommandList *l[] = {tempCmd.Get()};
    queue->ExecuteCommandLists(1, l);
    Sync();

    t->currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_CPU_DESCRIPTOR_HANDLE h = srvH->GetCPUDescriptorHandleForHeapStart();
    h.ptr += sOff * sInc;
    t->srvHandle = srvH->GetGPUDescriptorHandleForHeapStart();
    t->srvHandle.ptr += sOff * sInc;
    sOff++;

    D3D12_SHADER_RESOURCE_VIEW_DESC sd = {
                                          DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D,
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};
    sd.Texture2D.MipLevels = mips.size();
    dev->CreateShaderResourceView(t->res.Get(), &sd, h);

    return t;
}

void RHI_DX12::SetTexture(RHITexture *t, int slot) {
    if (!t)
        return;
    auto dxt = (DX12Texture *)t;

    if (dxt->srvHandle.ptr == 0)
        return;

    TransitionResource(dxt, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->SetGraphicsRootDescriptorTable(slot + 2, dxt->srvHandle);
}

std::shared_ptr<RHIBuffer> RHI_DX12::CreateBuffer(BufferType type,
                                                  const void *data, size_t size,
                                                  UINT stride) {
    auto b = std::make_shared<DX12Buffer>();
    b->stride = stride;

    if (size == 0)
        return nullptr;

    // 1. Special handling for Compute Shader (Unordered Access Views)
    if (type == BufferType::ComputeUAV) {
        D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC rd = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                  0,
                                  size,
                                  1,
                                  1,
                                  1,
                                  DXGI_FORMAT_UNKNOWN,
                                  {1, 0},
                                  D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};

        if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                D3D12_RESOURCE_STATE_COMMON,
                                                nullptr, IID_PPV_ARGS(&b->res)))) {
            return nullptr;
        }
        b->currentState = D3D12_RESOURCE_STATE_COMMON;
        b->vbv = {b->res->GetGPUVirtualAddress(), (UINT)size, stride};

        D3D12_CPU_DESCRIPTOR_HANDLE cpuH =
            srvH->GetCPUDescriptorHandleForHeapStart();
        cpuH.ptr += sOff * sInc;
        b->uavHandle = srvH->GetGPUDescriptorHandleForHeapStart();
        b->uavHandle.ptr += sOff * sInc;
        sOff++;

        D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {};
        ud.Format = DXGI_FORMAT_UNKNOWN;
        ud.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        ud.Buffer.NumElements = (UINT)(size / stride);
        ud.Buffer.StructureByteStride = stride;
        dev->CreateUnorderedAccessView(b->res.Get(), nullptr, &ud, cpuH);

        // If we are uploading data
        if (data) {
            ComPtr<ID3D12Resource> stage;
            D3D12_HEAP_PROPERTIES shp = {D3D12_HEAP_TYPE_UPLOAD};
            D3D12_RESOURCE_DESC srd = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                       0,
                                       size,
                                       1,
                                       1,
                                       1,
                                       DXGI_FORMAT_UNKNOWN,
                                       {1, 0},
                                       D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                       D3D12_RESOURCE_FLAG_NONE};

            if (FAILED(dev->CreateCommittedResource(&shp, D3D12_HEAP_FLAG_NONE, &srd,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ,
                                                    nullptr, IID_PPV_ARGS(&stage))))
                return nullptr;

            void *p;
            stage->Map(0, nullptr, &p);
            memcpy(p, data, size);
            stage->Unmap(0, nullptr);

            ComPtr<ID3D12CommandAllocator> tempAlloc;
            dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(&tempAlloc));
            ComPtr<ID3D12GraphicsCommandList> tempCmd;
            dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(),
                                   nullptr, IID_PPV_ARGS(&tempCmd));

            tempCmd->CopyBufferRegion(b->res.Get(), 0, stage.Get(), 0, size);

            D3D12_RESOURCE_BARRIER rb = {};
            rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            rb.Transition.pResource = b->res.Get();
            rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            rb.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            tempCmd->ResourceBarrier(1, &rb);
            tempCmd->Close();

            ID3D12CommandList *l[] = {tempCmd.Get()};
            queue->ExecuteCommandLists(1, l);
            Sync();

            b->currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        } else {
            ComPtr<ID3D12CommandAllocator> tempAlloc;
            dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(&tempAlloc));
            ComPtr<ID3D12GraphicsCommandList> tempCmd;
            dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(),
                                   nullptr, IID_PPV_ARGS(&tempCmd));

            D3D12_RESOURCE_BARRIER rb = {};
            rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            rb.Transition.pResource = b->res.Get();
            rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            rb.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            tempCmd->ResourceBarrier(1, &rb);
            tempCmd->Close();

            ID3D12CommandList *l[] = {tempCmd.Get()};
            queue->ExecuteCommandLists(1, l);
            Sync();

            b->currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        return b;
    }

    // 2. Normal buffers for models
    bool isStaticGeometry =
        (type == BufferType::Vertex || type == BufferType::Index) &&
                            (data != nullptr);

    if (isStaticGeometry) {
        D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC rd = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                  0,
                                  size,
                                  1,
                                  1,
                                  1,
                                  DXGI_FORMAT_UNKNOWN,
                                  {1, 0},
                                  D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                  D3D12_RESOURCE_FLAG_NONE};

        if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                D3D12_RESOURCE_STATE_COMMON,
                                                nullptr, IID_PPV_ARGS(&b->res)))) {
            return nullptr;
        }
        b->currentState = D3D12_RESOURCE_STATE_COMMON;

        if (data) {
            ComPtr<ID3D12Resource> stage;
            D3D12_HEAP_PROPERTIES shp = {D3D12_HEAP_TYPE_UPLOAD};
            D3D12_RESOURCE_DESC srd = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                       0,
                                       size,
                                       1,
                                       1,
                                       1,
                                       DXGI_FORMAT_UNKNOWN,
                                       {1, 0},
                                       D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                       D3D12_RESOURCE_FLAG_NONE};
            if (FAILED(dev->CreateCommittedResource(&shp, D3D12_HEAP_FLAG_NONE, &srd,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ,
                                                    nullptr, IID_PPV_ARGS(&stage))))
                return nullptr;

            void *p;
            stage->Map(0, nullptr, &p);
            memcpy(p, data, size);
            stage->Unmap(0, nullptr);

            ComPtr<ID3D12CommandAllocator> tempAlloc;
            dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(&tempAlloc));
            ComPtr<ID3D12GraphicsCommandList> tempCmd;
            dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(),
                                   nullptr, IID_PPV_ARGS(&tempCmd));

            tempCmd->CopyBufferRegion(b->res.Get(), 0, stage.Get(), 0, size);

            D3D12_RESOURCE_BARRIER rb = {};
            rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            rb.Transition.pResource = b->res.Get();
            rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            rb.Transition.StateAfter =
                (type == BufferType::Vertex)
                                           ? D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
                                           : D3D12_RESOURCE_STATE_INDEX_BUFFER;

            tempCmd->ResourceBarrier(1, &rb);
            tempCmd->Close();
            ID3D12CommandList *l[] = {tempCmd.Get()};
            queue->ExecuteCommandLists(1, l);
            Sync();

            b->currentState = rb.Transition.StateAfter;
        }

        UINT actualStride = stride > 0 ? stride : sizeof(Vertex);
        if (type == BufferType::Vertex)
            b->vbv = {b->res->GetGPUVirtualAddress(), (UINT)size, actualStride};
        if (type == BufferType::Index)
            b->ibv = {b->res->GetGPUVirtualAddress(), (UINT)size,
                      DXGI_FORMAT_R32_UINT};
    } else {
        // Constant buffers must be aligned to 256-byte boundaries
        UINT64 alignedSize =
            (type == BufferType::Constant) ? ((size + 255) & ~255) : size;
        b->sizePerFrame = alignedSize;

        // Double buffering means allocating 2x the memory
        UINT64 totalSize = alignedSize * FrameCount;
        D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_UPLOAD};
        D3D12_RESOURCE_DESC rd = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                  0,
                                  totalSize,
                                  1,
                                  1,
                                  1,
                                  DXGI_FORMAT_UNKNOWN,
                                  {1, 0},
                                  D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                  D3D12_RESOURCE_FLAG_NONE};

        if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                D3D12_RESOURCE_STATE_GENERIC_READ,
                                                nullptr, IID_PPV_ARGS(&b->res)))) {
            return nullptr;
        }
        b->currentState = D3D12_RESOURCE_STATE_GENERIC_READ;

        if (data) {
            void *p;
            b->res->Map(0, nullptr, &p);
            memcpy(p, data, size);
            b->res->Unmap(0, nullptr);
        }

        // Keep the pointer open for fast CPU writes (Map->Unmap is slow)
        b->res->Map(0, nullptr, (void **)&b->map);

        UINT actualStride = stride > 0 ? stride : sizeof(Vertex);
        if (type == BufferType::Vertex)
            b->vbv = {b->res->GetGPUVirtualAddress(), (UINT)size, actualStride};
        if (type == BufferType::Instance)
            b->vbv = {b->res->GetGPUVirtualAddress(), (UINT)size, sizeof(ObjectData)};
        if (type == BufferType::Index)
            b->ibv = {b->res->GetGPUVirtualAddress(), (UINT)size,
                      DXGI_FORMAT_R32_UINT};
    }
    return b;
}

void RHI_DX12::UpdateBuffer(RHIBuffer *b, const void *d, size_t s) {
    auto dx = (DX12Buffer *)b;
    if (dx->map) {
        // Write to the correct half of the ring buffer
        UINT64 frameBase = fIdx * dx->sizePerFrame;
        memcpy(dx->map + frameBase, d, s);

        if (dx->vbv.SizeInBytes > 0)
            dx->vbv.BufferLocation = dx->res->GetGPUVirtualAddress() + frameBase;
        if (dx->ibv.SizeInBytes > 0)
            dx->ibv.BufferLocation = dx->res->GetGPUVirtualAddress() + frameBase;
    }
}

void RHI_DX12::SetGlobalUniforms(RHIBuffer *b, const void *d, size_t s) {
    auto dx = (DX12Buffer *)b;
    size_t aligned = (s + 255) & ~255;
    UINT64 frameBase = fIdx * dx->sizePerFrame;

    if (d && dx->map)
        memcpy(dx->map + frameBase + cOff, d, s);

    cmd->SetGraphicsRootConstantBufferView(0, dx->res->GetGPUVirtualAddress() +
                                                  frameBase + cOff);
    cOff += aligned;
}

void RHI_DX12::SetObjectUniforms(RHIBuffer *b, const void *d, size_t s) {
    auto dx = (DX12Buffer *)b;
    size_t aligned = (s + 255) & ~255;
    UINT64 frameBase = fIdx * dx->sizePerFrame;

    if (d && dx->map)
        memcpy(dx->map + frameBase + oOff, d, s);

    cmd->SetGraphicsRootConstantBufferView(1, dx->res->GetGPUVirtualAddress() +
                                                  frameBase + oOff);
    oOff += aligned;
}

void RHI_DX12::SetBoneUniforms(RHIBuffer *b, const void *d, size_t s) {
    auto dx = (DX12Buffer *)b;
    size_t aligned = (s + 255) & ~255;
    UINT64 frameBase = fIdx * dx->sizePerFrame;

    if (d && dx->map)
        memcpy(dx->map + frameBase + bOff, d, s);

    cmd->SetGraphicsRootConstantBufferView(10, dx->res->GetGPUVirtualAddress() +
                                                   frameBase + bOff);
    bOff += aligned;
}

void RHI_DX12::SetPushConstants(const void *data, size_t size) {
    cmd->SetGraphicsRoot32BitConstants(1, (UINT)size / 4, data, 0);
}

std::shared_ptr<RHIPipeline>
RHI_DX12::CreatePipeline(const PipelineConfig &config) {
    auto p = std::make_shared<DX12Pipeline>();

    // 1. Kompilace Vertex Shaderu pøes moderní DXC
    auto vsBlob = ShaderCompiler::CompileDX12(config.vsPath, "VSMain", "vs_6_0");
    if (!vsBlob) {
        // Pokud selže, vracíme p. (MessageBox s chybou už vyskoèil uvnitø CompileDX12)
        return p;
    }

    // 2. Kompilace Pixel Shaderu (pokud je zadán)
    ComPtr<IDxcBlob> psBlob = nullptr;
    if (!config.psPath.empty()) {
        psBlob = ShaderCompiler::CompileDX12(config.psPath, "PSMain", "ps_6_0");
        if (!psBlob) {
            return p;
        }
    }
    // Create Root Signature (Maps shaders to VRAM)
    D3D12_DESCRIPTOR_RANGE ranges[8];
    for (int i = 0; i < 8; i++)
        ranges[i] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, (UINT)i};

    D3D12_ROOT_PARAMETER rp[11] = {};
    UINT rootParamCount = 11;

    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp[0].Descriptor = {0, 0}; // Global Data (b0)

    if (config.isSkinned) {
        rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rp[1].Descriptor = {1, 0}; // Object Data (b1) for Skinned
    } else {
        rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rp[1].Constants = {1, 0, sizeof(ObjectData) / 4}; // Instancing
    }

    for (int i = 2; i <= 9; i++) {
        rp[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rp[i].DescriptorTable = {1, &ranges[i - 2]};
        rp[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }

    rp[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp[10].Descriptor = {2, 0}; // Bones (b2)

    D3D12_STATIC_SAMPLER_DESC smp[2] = {};
    smp[0].Filter = D3D12_FILTER_ANISOTROPIC;
    smp[0].MaxAnisotropy = 16;
    smp[0].ShaderRegister = 0;
    smp[0].AddressU = smp[0].AddressV = smp[0].AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    smp[0].MaxLOD = D3D12_FLOAT32_MAX;

    smp[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    smp[1].ShaderRegister = 1;
    smp[1].AddressU = smp[1].AddressV = smp[1].AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    smp[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    smp[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_ROOT_SIGNATURE_DESC rsd = {
                                     rootParamCount, rp, 2, smp,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    ComPtr<ID3DBlob> sig;
    D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, 0);
    dev->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                             IID_PPV_ARGS(&p->rs));

    // Set up Vertex Input Layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> ied;
    if (config.vsPath.find(L"fullscreen") == std::wstring::npos) {
        ied.push_back({"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
        ied.push_back({"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
                       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
        ied.push_back({"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
                       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});

        if (config.isSkinned) {
            ied.push_back({"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, 32,
                           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
            ied.push_back({"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48,
                           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
        }
        if (config.useInstancing) {
            ied.push_back({"TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
                           D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16,
                           D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32,
                           D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48,
                           D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 7, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64,
                           D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 8, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 80,
                           D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1});
            ied.push_back({"TEXCOORD", 9, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 96,
                           D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1});
        }
    }

    p->top = config.topology == Topology::LineList
                 ? D3D_PRIMITIVE_TOPOLOGY_LINELIST
                 : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gps = {};
    if (!ied.empty())
        gps.InputLayout = {ied.data(), (UINT)ied.size()};

    gps.pRootSignature = p->rs.Get();
    gps.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    if (psBlob) {
        gps.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    }

    gps.RasterizerState = {D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK};
    gps.RasterizerState.CullMode = config.cullMode == CullMode::None
                                       ? D3D12_CULL_MODE_NONE
                                       : D3D12_CULL_MODE_BACK;
    gps.RasterizerState.FillMode = config.fillMode == FillMode::Wireframe
                                       ? D3D12_FILL_MODE_WIREFRAME
                                       : D3D12_FILL_MODE_SOLID;

    // Blending type
    D3D12_RENDER_TARGET_BLEND_DESC rtBlend = {};
    if (config.isAdditive) {
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_ONE;
        rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    } else if (config.isTransparent) {
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }
    else if (config.enableBlend) {
        // --- NOVÉ (Pro Cloud Upscale) ---
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_ONE;
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
 
    } else {
        rtBlend.BlendEnable = FALSE;
    }

    rtBlend.LogicOpEnable = FALSE;
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    gps.NumRenderTargets = psBlob ? config.numRenderTargets : 0;
    for (int i = 0; i < 8; i++)
        gps.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;

    for (UINT i = 0; i < gps.NumRenderTargets; i++) {
        gps.BlendState.RenderTarget[i] = rtBlend;
        if (i == 0)
            gps.RTVFormats[i] = DXGI_FORMAT_R8G8B8A8_UNORM;
        else if (i == 1)
            gps.RTVFormats[i] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        else if (i == 2)
            gps.RTVFormats[i] = DXGI_FORMAT_R32G32B32A32_FLOAT;
    }

    gps.DepthStencilState = {TRUE, D3D12_DEPTH_WRITE_MASK_ALL,
                             D3D12_COMPARISON_FUNC_LESS_EQUAL};
    gps.DepthStencilState.DepthEnable = config.depthTest;
    gps.DepthStencilState.DepthWriteMask = config.depthWrite
                                               ? D3D12_DEPTH_WRITE_MASK_ALL
                                               : D3D12_DEPTH_WRITE_MASK_ZERO;
    gps.SampleMask = UINT_MAX;
    gps.PrimitiveTopologyType = config.topology == Topology::LineList
                                    ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
                                    : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gps.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    gps.SampleDesc = {1, 0};

    dev->CreateGraphicsPipelineState(&gps, IID_PPV_ARGS(&p->pso));
    return p;
}

void RHI_DX12::SetPipeline(RHIPipeline *p) {
    auto dx = (DX12Pipeline *)p;
    m_currentPipeline = p;

    if (lastRS != dx->rs.Get()) {
        cmd->SetGraphicsRootSignature(dx->rs.Get());
        lastRS = dx->rs.Get();
    }
    if (lastPSO != dx->pso.Get()) {
        cmd->SetPipelineState(dx->pso.Get());
        lastPSO = dx->pso.Get();
    }
    cmd->IASetPrimitiveTopology(dx->top);
}

void RHI_DX12::DrawIndexed(RHIBuffer *vb, RHIBuffer *ib, UINT c) {
    auto v = (DX12Buffer *)vb;
    auto i = (DX12Buffer *)ib;

    v->vbv.StrideInBytes = v->stride > 0 ? v->stride : sizeof(Vertex);

    cmd->IASetVertexBuffers(0, 1, &v->vbv);
    cmd->IASetIndexBuffer(&i->ibv);
    cmd->DrawIndexedInstanced(c, 1, 0, 0, 0);
}

void RHI_DX12::Draw(RHIBuffer *vb, UINT c) {
    if (vb) {
        auto v = (DX12Buffer *)vb;
        v->vbv.StrideInBytes = v->stride > 0 ? v->stride : sizeof(Vertex);
        cmd->IASetVertexBuffers(0, 1, &v->vbv);
    }
    cmd->DrawInstanced(c, 1, 0, 0);
}

void RHI_DX12::DrawIndexedInstanced(RHIBuffer *vb, RHIBuffer *ib,
                                    RHIBuffer *instB, UINT indexCount,
                                    UINT instanceCount, UINT instanceOffset) {
    auto v = (DX12Buffer *)vb;
    auto i = (DX12Buffer *)ib;
    auto inst = (DX12Buffer *)instB;

    v->vbv.StrideInBytes = v->stride > 0 ? v->stride : sizeof(Vertex);

    D3D12_VERTEX_BUFFER_VIEW views[2] = {v->vbv, inst->vbv};
    cmd->IASetVertexBuffers(0, 2, views);
    cmd->IASetIndexBuffer(&i->ibv);

    cmd->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, instanceOffset);
}

std::shared_ptr<RHIPipeline> RHI_DX12::CreateComputePipeline(const std::wstring& csPath) {
    auto p = std::make_shared<DX12Pipeline>();

    // Využití naší nové tøídy s moderním DXC kompilátorem
    auto csBlob = ShaderCompiler::CompileDX12(csPath, "CSMain", "cs_6_0");
    if (!csBlob) return nullptr; // Pokud kompilace selže, nepokraèujeme!

    p->rs = computeRS;
    D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};
    cpsd.pRootSignature = p->rs.Get();
    cpsd.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
    dev->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&p->pso));

    return p;
}
std::shared_ptr<RHITexture> RHI_DX12::CreateUAVTexture(int w_in, int h_in,
                                                       int format) {
    auto t = std::make_shared<DX12Texture>();
    t->width = w_in;
    t->height = h_in;

    DXGI_FORMAT fmt = (format == 1) ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                    : DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = w_in;
    d.Height = h_in;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Format = fmt;
    d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d,
                                            D3D12_RESOURCE_STATE_COMMON, nullptr,
                                            IID_PPV_ARGS(&t->res)))) {
        return nullptr;
    }
    t->currentState = D3D12_RESOURCE_STATE_COMMON;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuH = srvH->GetCPUDescriptorHandleForHeapStart();
    cpuH.ptr += sOff * sInc;

    t->uavHandle = srvH->GetGPUDescriptorHandleForHeapStart();
    t->uavHandle.ptr += sOff * sInc;
    sOff++;

    D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {};
    ud.Format = fmt;
    ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    dev->CreateUnorderedAccessView(t->res.Get(), nullptr, &ud, cpuH);

    cpuH = srvH->GetCPUDescriptorHandleForHeapStart();
    cpuH.ptr += sOff * sInc;
    t->srvHandle = srvH->GetGPUDescriptorHandleForHeapStart();
    t->srvHandle.ptr += sOff * sInc;
    sOff++;

    D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = fmt;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;
    dev->CreateShaderResourceView(t->res.Get(), &sd, cpuH);

    return t;
}

void RHI_DX12::SetComputePipeline(RHIPipeline *p) {
    auto dx = (DX12Pipeline *)p;
    lastPSO = nullptr;
    cmd->SetPipelineState(dx->pso.Get());
    cmd->SetComputeRootSignature(dx->rs.Get());

    ID3D12DescriptorHeap *heaps[] = {srvH.Get()};
    cmd->SetDescriptorHeaps(1, heaps);
}

void RHI_DX12::SetComputeUniforms(RHIBuffer *b, const void *d, size_t s,
                                  int slot) {
    auto dx = (DX12Buffer *)b;
    size_t aligned = (s + 255) & ~255;
    UINT64 frameBase = fIdx * dx->sizePerFrame;

    if (d && dx->map)
        memcpy(dx->map + frameBase + compOff, d, s);

    cmd->SetComputeRootConstantBufferView(slot, dx->res->GetGPUVirtualAddress() +
                                                    frameBase + compOff);
    compOff += aligned;
}

void RHI_DX12::SetComputeTextureSRV(RHITexture *t, int slot) {
    auto dx = (DX12Texture *)t;
    TransitionResource(dx, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    cmd->SetComputeRootDescriptorTable(slot + 2, dx->srvHandle);
}

void RHI_DX12::SetComputeTextureUAV(RHITexture *t, int slot) {
    auto dx = (DX12Texture *)t;
    TransitionResource(dx, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmd->SetComputeRootDescriptorTable(slot + 1, dx->uavHandle);
}

void RHI_DX12::DispatchCompute(UINT x, UINT y, UINT z) {
    cmd->Dispatch(x, y, z);
}

void RHI_DX12::ComputeBarrier(RHITexture *uavTexture) {
    if (!uavTexture)
        return;
    auto dx = (DX12Texture *)uavTexture;
    D3D12_RESOURCE_BARRIER rb = {};
    rb.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    rb.UAV.pResource = dx->res.Get();
    cmd->ResourceBarrier(1, &rb);
}
std::shared_ptr<RHITexture> RHI_DX12::CreateUAVTexture3D(int w_in, int h_in, int depth_in, int format) {
    auto t = std::make_shared<DX12Texture>();
    t->width = w_in;
    t->height = h_in;

    DXGI_FORMAT fmt = (format == 1) ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D; // MUSÍ BÝT 3D
    d.Width = w_in;
    d.Height = h_in;
    d.DepthOrArraySize = depth_in; // U 3D textury je Depth zde
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Format = fmt;
    d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES hp = { D3D12_HEAP_TYPE_DEFAULT };
    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&t->res)))) {
        return nullptr;
    }
    t->currentState = D3D12_RESOURCE_STATE_COMMON;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuH = srvH->GetCPUDescriptorHandleForHeapStart();
    cpuH.ptr += sOff * sInc;

    t->uavHandle = srvH->GetGPUDescriptorHandleForHeapStart();
    t->uavHandle.ptr += sOff * sInc;
    sOff++;

    D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {};
    ud.Format = fmt;
    ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D; // MUSÍ BÝT 3D
    ud.Texture3D.MipSlice = 0;
    ud.Texture3D.FirstWSlice = 0;
    ud.Texture3D.WSize = depth_in;
    dev->CreateUnorderedAccessView(t->res.Get(), nullptr, &ud, cpuH);

    cpuH = srvH->GetCPUDescriptorHandleForHeapStart();
    cpuH.ptr += sOff * sInc;
    t->srvHandle = srvH->GetGPUDescriptorHandleForHeapStart();
    t->srvHandle.ptr += sOff * sInc;
    sOff++;

    D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = fmt;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D; // MUSÍ BÝT 3D
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture3D.MipLevels = 1;
    dev->CreateShaderResourceView(t->res.Get(), &sd, cpuH);

    return t;
}
void RHI_DX12::SetComputeBufferUAV(RHIBuffer *buffer, int slot) {
    if (!buffer)
        return;
    auto dx = (DX12Buffer *)buffer;
    TransitionBuffer(dx->res.Get(), dx->currentState,
                     D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    dx->currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    cmd->SetComputeRootDescriptorTable(3 + slot, dx->uavHandle);
}

void RHI_DX12::ComputeBufferBarrier(RHIBuffer *buffer) {
    if (!buffer)
        return;
    auto dx = (DX12Buffer *)buffer;

    D3D12_RESOURCE_BARRIER rb = {};
    rb.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    rb.UAV.pResource = dx->res.Get();
    cmd->ResourceBarrier(1, &rb);

    TransitionBuffer(dx->res.Get(), dx->currentState,
                     D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    dx->currentState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
}
