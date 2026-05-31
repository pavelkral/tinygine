#pragma once

#include "pch/Pch.h"

class SkinnedMeshRenderer : public Component {
public:
    std::shared_ptr<SkeletalMesh> mesh;
    std::vector<std::shared_ptr<Material>> materialOverrides;

    XMFLOAT4 baseColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };

    std::string meshName = "";
    std::string meshPath = "";
    std::vector<std::string> overrideMatNames;        SkinnedMeshRenderer(std::shared_ptr<SkeletalMesh> m = nullptr, std::string mName = "", std::string mPath = "");    json Serialize() override;    void Deserialize(const json& j) override;    void SetMaterial(size_t subMeshIndex, std::shared_ptr<Material> mat);    SkinnedObjectData GetSubMeshData(size_t subMeshIndex);
};

