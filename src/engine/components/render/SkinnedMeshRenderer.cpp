#include "engine/EngineDependencies.h"
#include "engine/components/render/SkinnedMeshRenderer.h"

SkinnedMeshRenderer::SkinnedMeshRenderer(std::shared_ptr<SkeletalMesh> m, std::string mName, std::string mPath)
        : mesh(m), meshName(mName), meshPath(mPath) {
        componentType = "SkinnedMeshRenderer";
        if (mesh) {
            materialOverrides.resize(mesh->subMeshes.size(), nullptr);
        }
    }


json SkinnedMeshRenderer::Serialize() {
        json j = Component::Serialize();
        j["baseColorMultiplier"] = { baseColorMultiplier.x, baseColorMultiplier.y, baseColorMultiplier.z, baseColorMultiplier.w };
        j["meshName"] = meshName;
        j["meshPath"] = meshPath;

        json mats = json::array();
        for (auto& m : materialOverrides) {
            if (m) mats.push_back(m->name);
            else mats.push_back("");
        }
        j["overrideMatNames"] = mats;
        return j;
    }


void SkinnedMeshRenderer::Deserialize(const json& j) {
        if (j.contains("baseColorMultiplier")) {
            baseColorMultiplier = { j["baseColorMultiplier"][0], j["baseColorMultiplier"][1], j["baseColorMultiplier"][2], j["baseColorMultiplier"][3] };
        }
        meshName = j.value("meshName", "");
        meshPath = j.value("meshPath", "");

        if (j.contains("overrideMatNames")) {
            for (auto& m : j["overrideMatNames"]) overrideMatNames.push_back(m.get<std::string>());
        }
    }


void SkinnedMeshRenderer::SetMaterial(size_t subMeshIndex, std::shared_ptr<Material> mat) {
        if (subMeshIndex < materialOverrides.size()) materialOverrides[subMeshIndex] = mat;
    }


SkinnedObjectData SkinnedMeshRenderer::GetSubMeshData(size_t subMeshIndex) {
        const auto& subMesh = mesh->subMeshes[subMeshIndex];

        // BEZPE�NOSTN� POJISTKA PROTI P�DU (Out of bounds)
        std::shared_ptr<Material> activeMaterial = subMesh.material;
        if (subMeshIndex < materialOverrides.size() && materialOverrides[subMeshIndex]) {
            activeMaterial = materialOverrides[subMeshIndex];
        }

        SkinnedObjectData data = {};
        SM::Vector3 p((float)gameObject->transform.position.x, (float)gameObject->transform.position.y, (float)gameObject->transform.position.z);
        SM::Matrix model = SM::Matrix::CreateScale(gameObject->transform.scale) * SM::Matrix::CreateFromQuaternion(gameObject->transform.rotation) * SM::Matrix::CreateTranslation(p);

        data.model = model.Transpose();
        if (activeMaterial) {
            data.baseColor = { activeMaterial->baseColor.x * baseColorMultiplier.x, activeMaterial->baseColor.y * baseColorMultiplier.y, activeMaterial->baseColor.z * baseColorMultiplier.z, activeMaterial->baseColor.w * baseColorMultiplier.w };
            data.hasAlbedoTex = activeMaterial->albedoTex ? 1.0f : 0.0f;
            data.hasNormalTex = activeMaterial->normalTex ? 1.0f : 0.0f;
            data.hasMetalTex = activeMaterial->metalTex ? 1.0f : 0.0f;
            data.hasRoughTex = activeMaterial->roughTex ? 1.0f : 0.0f;
            data.roughness = activeMaterial->roughness;
            data.metalness = activeMaterial->metalness;
        }
        else {
            data.baseColor = baseColorMultiplier; data.roughness = 0.5f; data.metalness = 0.0f;
        }
        data.alphaCutoff = -1.0f;
        return data;
    }
