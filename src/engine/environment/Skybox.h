#pragma once
#include "pch/Pch.h"
#include "engine/EngineDependencies.h"

class Skybox {
private:
    std::shared_ptr<RHIPipeline> m_pipeline;
    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<RHITexture> m_texture;

public:
    Skybox();
    bool Init(RHI* rhi, const std::wstring& texturePath);
    void Render(RHI* rhi, RHIBuffer* globalBuffer, const GlobalData& gData);
};
