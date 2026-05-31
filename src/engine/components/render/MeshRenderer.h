#pragma once

#include "pch/Pch.h"

class MeshRenderer : public Component {
public:
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool isInstanced;
    std::string meshName = "Cube"; // NYN� POU��V�ME TEXTOV� ID
    std::string matName = "";        MeshRenderer(std::shared_ptr<Mesh> m = nullptr, std::shared_ptr<Material> mat = nullptr, bool instanced = true, std::string mName = "Cube");    json Serialize() override;    void Deserialize(const json& j) override;    ObjectData GetInstanceData();
};
