#pragma once

#include "pch/Pch.h"
#include "rhi/RHI.h"

class Material {
public:
    std::string name;
    std::string pipelineType = "Instanced"; 

    std::shared_ptr<RHIPipeline> pipeline;
    std::shared_ptr<RHITexture> albedoTex, normalTex, metalTex, roughTex;

    std::string albedoPath = "", normalPath = "", metalPath = "", roughPath = "";

    XMFLOAT4 baseColor = { 1, 1, 1, 1 };
    float roughness = 0.5f, metalness = 0.0f;    
    Material(RHI* rhi, const std::string& matName, const std::wstring& vsPath, const std::wstring& psPath, bool useInstancing = true, bool isSkinned = false);    
    Material(const std::string& matName, std::shared_ptr<RHIPipeline> sharedPipeline, std::string pType = "Instanced");    
    void BindTextures(RHI* rhi);    
    json Serialize();    
    void Deserialize(const json& j, RHI* rhi);
};
