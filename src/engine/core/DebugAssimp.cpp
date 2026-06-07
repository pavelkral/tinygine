#include "engine/core/DebugAssimp.h"

#include <assimp/anim.h>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <iostream>
#include <set>

void DebugPrintAssimpScene(const aiScene* scene, const std::string& filepath) {
    if (!scene) {
        std::cout << "[DEBUG] Cannot read scene for file: " << filepath << "\n";
        return;
    }

    std::cout << "\n====================================================\n";
    std::cout << "ASSIMP DEBUG REPORT: " << filepath << "\n";
    std::cout << "======================================================\n";

    std::cout << "\n--- GLOBAL STATS ---\n";
    std::cout << " Meshes:            " << scene->mNumMeshes << "\n";
    std::cout << " Materials:         " << scene->mNumMaterials << "\n";
    std::cout << " Animations:        " << scene->mNumAnimations << "\n";
    std::cout << " Cameras:           " << scene->mNumCameras << "\n";
    std::cout << " Lights:            " << scene->mNumLights << "\n";
    std::cout << " Embedded textures: " << scene->mNumTextures << "\n";

    std::cout << "\n--- MESHES & GEOMETRY ---\n";
    std::set<std::string> uniqueBones;
    uint32_t totalVertices = 0;
    uint32_t totalFaces = 0;

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        totalVertices += mesh->mNumVertices;
        totalFaces += mesh->mNumFaces;

        std::cout << " [" << i << "] Mesh: '" << mesh->mName.C_Str() << "'\n";
        std::cout << "     - Vertices: " << mesh->mNumVertices << "\n";
        std::cout << "     - Faces:    " << mesh->mNumFaces << "\n";
        std::cout << "     - MatIdx:   " << mesh->mMaterialIndex << "\n";
        std::cout << "     - Normals:  " << (mesh->HasNormals() ? "YES" : "NO") << "\n";
        std::cout << "     - UV Coords:" << (mesh->HasTextureCoords(0) ? "YES" : "NO") << "\n";
        std::cout << "     - Bones:    " << mesh->mNumBones << "\n";

        for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
            uniqueBones.insert(mesh->mBones[b]->mName.C_Str());
        }
    }

    std::cout << "\n [GEOMETRY SUMMARY]\n";
    std::cout << " -> Total vertices: " << totalVertices << "\n";
    std::cout << " -> Total faces:    " << totalFaces << "\n";
    std::cout << " -> Unique bones:   " << uniqueBones.size() << "\n";

    std::cout << "\n--- MATERIALS ---\n";
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* mat = scene->mMaterials[i];
        aiString name;
        mat->Get(AI_MATKEY_NAME, name);
        std::cout << " [" << i << "] Material: '" << name.C_Str() << "'\n";

        auto printTexCount = [&](aiTextureType type, const char* typeName) {
            unsigned int count = mat->GetTextureCount(type);
            if (count > 0) {
                aiString path;
                mat->GetTexture(type, 0, &path);
                std::cout << "     - " << typeName << " (" << count << "x): " << path.C_Str() << "\n";
            }
        };

        printTexCount(aiTextureType_DIFFUSE, "Diffuse");
        printTexCount(aiTextureType_NORMALS, "Normals");
        printTexCount(aiTextureType_METALNESS, "Metalness");
        printTexCount(aiTextureType_DIFFUSE_ROUGHNESS, "Roughness");
        printTexCount(aiTextureType_SHININESS, "Shininess");
        printTexCount(aiTextureType_SPECULAR, "Specular");
        printTexCount(aiTextureType_UNKNOWN, "Unknown");
    }

    std::cout << "\n--- ANIMATIONS ---\n";
    if (scene->mNumAnimations == 0) {
        std::cout << " Model has no animations.\n";
    } else {
        for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
            aiAnimation* anim = scene->mAnimations[i];
            std::cout << " [" << i << "] Animation: '" << anim->mName.C_Str() << "'\n";
            std::cout << "     - Duration: " << anim->mDuration << " ticks\n";
            std::cout << "     - Ticks Per Second: " << anim->mTicksPerSecond << "\n";
            std::cout << "     - Channels: " << anim->mNumChannels << "\n";
        }
    }

    std::cout << "======================================================\n\n";
}
