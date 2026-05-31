#pragma once


#include "pch/Pch.h"

class AssetRegistry {
public:
    RHI* m_rhi = nullptr;

    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_meshes;
    std::unordered_map<std::string, std::shared_ptr<SkeletalMesh>> m_skelMeshes;
    std::unordered_map<std::string, std::shared_ptr<Material>> m_allMaterials;

    std::shared_ptr<RHIPipeline> m_pipeSolid;
    std::shared_ptr<RHIPipeline> m_pipeInstanced;
    std::shared_ptr<RHIPipeline> m_pipeSkinned;
    void Init(RHI* rhi);
    void Clear();
    void LoadHardcodedAssets();
    void LoadDiskAssets();
    void SaveAssets();
    std::shared_ptr<SkeletalMesh> LoadSkeletalMesh(const std::string& path,
        const PipelineConfig& cfg);
    void LoadPrimitiveMeshes();
};
