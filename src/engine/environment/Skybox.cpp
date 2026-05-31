#include "engine/environment/Skybox.h"
#include "engine/EngineDependencies.h"

Skybox::Skybox() = default;

bool Skybox::Init(RHI *rhi, const std::wstring &texturePath) {
    if (!rhi)
        return false;
    m_texture = rhi->CreateDDSTexture(texturePath);
    if (!m_texture) {
        std::cerr << "Skybox: Nepodarilo se nacist texturu: "
                  << std::string(texturePath.begin(), texturePath.end()) << "\n";
        return false;
    }
    m_mesh = MeshFactory::CreateCube(rhi, 1.0f);
    PipelineConfig skyboxCfg;
    skyboxCfg.vsPath = L"shaders/rhi/skybox.vert.hlsl";
    skyboxCfg.psPath = L"shaders/rhi/skybox.frag.hlsl";
    skyboxCfg.cullMode = CullMode::None;
    skyboxCfg.depthTest = true;
    skyboxCfg.depthWrite = false;
    skyboxCfg.useInstancing = false;
    skyboxCfg.isSkinned = false;
    skyboxCfg.numRenderTargets = 3;

    m_pipeline = rhi->CreatePipeline(skyboxCfg);

    return m_pipeline != nullptr;
}

void Skybox::Render(RHI *rhi, RHIBuffer *globalBuffer,
                    const GlobalData &gData) {
    if (!rhi || !m_pipeline || !m_mesh || !m_texture)
        return;
    rhi->SetPipeline(m_pipeline.get());
    rhi->SetGlobalUniforms(globalBuffer, &gData, sizeof(GlobalData));
    for (int i = 0; i < 8; i++) {
        rhi->SetTexture(nullptr, i);
    }
    rhi->SetTexture(m_texture.get(), 0);
    rhi->DrawIndexed(m_mesh->vb.get(), m_mesh->ib.get(), m_mesh->indexCount);
}
