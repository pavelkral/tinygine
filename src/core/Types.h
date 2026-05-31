#pragma once

#include "pch/Pch.h"

/// =============================================================

#define MAX_BONE_INFLUENCE 4
#define MAX_BONES 100
#define MAX_POINT_LIGHTS 16

struct SkinnedVertex {
    XMFLOAT3 pos; XMFLOAT3 normal; XMFLOAT2 uv;
    int boneIDs[MAX_BONE_INFLUENCE]; float boneWeights[MAX_BONE_INFLUENCE];
};

struct BoneInfo { int id; SM::Matrix offsetMatrix; };

SM::Matrix AssimpToSimpleMathMatrix(const aiMatrix4x4& m);

struct Vertex { XMFLOAT3 pos; XMFLOAT3 normal; XMFLOAT2 uv; };


struct PointLightData {
    XMFLOAT3 position;
    float radius;
    XMFLOAT3 color;
    float intensity;
};

// global data
struct GlobalData {
    XMMATRIX view;
    XMMATRIX projection;
    XMMATRIX lightSpaceMatrix;

    XMFLOAT3 camPos;
    float hasIBL;

    //  (Directional Light)
    XMFLOAT3 dirLightDir;
    float dirLightIntensity;
    XMFLOAT3 dirLightColor;
    int hasShadowMap;

    // settings engine
    int enableSSAO;
    int numPointLights;
    float pad[2]; // 

    // local lights
    PointLightData pointLights[MAX_POINT_LIGHTS];
};
struct ObjectData {
    XMMATRIX model; XMFLOAT4 baseColor; float roughness; float metalness;
    float hasAlbedoTex; float hasNormalTex; float hasMetalTex; float hasRoughTex; float pad[2];
};

struct SkinnedObjectData {
    SM::Matrix model; XMFLOAT4 baseColor; float roughness; float metalness;
    float hasAlbedoTex; float hasNormalTex; float hasMetalTex; float hasRoughTex; float alphaCutoff; float pad[3];
};
