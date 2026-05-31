// --- ADDED GLOBAL DATA ---
cbuffer GlobalData : register(b0) //  DX  VK  to binding 0
{
    matrix view;
    matrix projection;
    matrix lightSpaceMatrix;
    float3 camPos;
    float hasIBL;
    float3 lightPos;
    float pad2;
    float3 lightColor;
    float lightIntensity;
    int hasShadowMap;
    float3 pad3;
};

#ifdef VULKAN
    // VULKAN: consistent BindGlobalDescriptors (Binding 1 a 2)
    cbuffer SkinnedObjectData : register(b1) {
        matrix model; float4 baseColor; float roughness; float metalness;
        float hasAlbedoTex; float hasNormalTex; float hasMetalTex; float hasRoughTex; float alphaCutoff; float pad;
    };
    cbuffer BoneData : register(b2) { matrix finalBones[100]; };
#else
//  DX12  Root Signature:
// rp[1] = ObjectData (b1), rp[10] = BoneData (b2)
cbuffer SkinnedObjectData : register(b1)
{
    matrix model;
    float4 baseColor;
    float roughness;
    float metalness;
    float hasAlbedoTex;
    float hasNormalTex;
    float hasMetalTex;
    float hasRoughTex;
    float alphaCutoff;
    float pad;
};
cbuffer BoneData : register(b2)
{
    matrix finalBones[100];
};
#endif

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    int4 boneIDs : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 sv_pos : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 lightSpacePos : TEXCOORD1;
    
    float4 baseColor : COLOR0;
    float roughness : TEXCOORD3;
    float metalness : TEXCOORD4;
    float hasAlbedoTex : TEXCOORD5;
    float hasNormalTex : TEXCOORD6;
    float hasMetalTex : TEXCOORD7;
    float hasRoughTex : TEXCOORD8;
    float alphaCutoff : TEXCOORD9;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    
    matrix boneTransform = finalBones[input.boneIDs.x] * input.boneWeights.x;
    boneTransform += finalBones[input.boneIDs.y] * input.boneWeights.y;
    boneTransform += finalBones[input.boneIDs.z] * input.boneWeights.z;
    boneTransform += finalBones[input.boneIDs.w] * input.boneWeights.w;

    float4 localPos = mul(float4(input.pos, 1.0f), boneTransform);
    float3 localNormal = mul(input.normal, (float3x3) boneTransform);

    float4 worldPos = mul(localPos, model);
    output.worldPos = worldPos.xyz;
    output.sv_pos = mul(mul(worldPos, view), projection);
    output.lightSpacePos = mul(worldPos, lightSpaceMatrix);
    output.normal = normalize(mul(localNormal, (float3x3) model));
    output.uv = input.uv;
    
    output.baseColor = baseColor;
    output.alphaCutoff = alphaCutoff;
    output.roughness = roughness;
    output.metalness = metalness;
    output.hasAlbedoTex = hasAlbedoTex;
    output.hasNormalTex = hasNormalTex;
    output.hasMetalTex = hasMetalTex;
    output.hasRoughTex = hasRoughTex;
    return output;
}