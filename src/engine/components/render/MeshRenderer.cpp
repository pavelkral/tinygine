#include "engine/EngineDependencies.h"
#include "engine/components/render/MeshRenderer.h"

MeshRenderer::MeshRenderer(std::shared_ptr<Mesh> m, std::shared_ptr<Material> mat, bool instanced, std::string mName)
        : mesh(m), material(mat), isInstanced(instanced), meshName(mName) {
        componentType = "MeshRenderer";
        if (mat) matName = mat->name;
    }


json MeshRenderer::Serialize() {
        json j = Component::Serialize();
        j["isInstanced"] = isInstanced;
        j["meshName"] = meshName;
        if (material) matName = material->name; // Pojistka p�ed ulo�en�m
        j["matName"] = matName;
        return j;
    }


void MeshRenderer::Deserialize(const json& j) {
        isInstanced = j.value("isInstanced", true);
        matName = j.value("matName", "");

        // ZP�TN� KOMPATIBILITA: Podpora pro nov� i star� JSON soubory
        if (j.contains("meshName")) {
            meshName = j.value("meshName", "Cube");
        }
        else if (j.contains("meshId")) {
            // Pokud je to star� save, p�evedeme star� ��seln� ID na nov� text
            int mId = j.value("meshId", 1);
            if (mId == 0) meshName = "Sphere";
            else if (mId == 1) meshName = "Cube";
            else if (mId == 2) meshName = "Capsule";
            else if (mId == 3) meshName = "FloorCube";
            else meshName = "Cube";
        }
        else {
            meshName = "Cube";
        }
    }


ObjectData MeshRenderer::GetInstanceData() {
        ObjectData data = {};
        SM::Vector3 p((float)gameObject->transform.position.x, (float)gameObject->transform.position.y, (float)gameObject->transform.position.z);
        SM::Matrix model = SM::Matrix::CreateScale(gameObject->transform.scale) * SM::Matrix::CreateFromQuaternion(gameObject->transform.rotation) * SM::Matrix::CreateTranslation(p);

        data.model = model;
        if (material) {
            data.baseColor = material->baseColor;
            data.roughness = material->roughness;
            data.metalness = material->metalness;
            data.hasAlbedoTex = material->albedoTex ? 1 : 0;
            data.hasNormalTex = material->normalTex ? 1 : 0;
            data.hasMetalTex = material->metalTex ? 1 : 0;
            data.hasRoughTex = material->roughTex ? 1 : 0;
        }
        return data;
    }
