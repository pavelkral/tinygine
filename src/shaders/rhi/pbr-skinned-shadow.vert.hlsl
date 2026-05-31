cbuffer GlobalData : register(b0) 
{
    matrix view;
    matrix projection;
    float4 pad[4];
};

// --- REGISTRY BINDINGS VULKAN I DX12 ---
#ifdef VULKAN
    cbuffer SkinnedObjectData : register(b1) { matrix model; float4 pad2[4]; }; // BINDING 1
    cbuffer BoneData : register(b2) { matrix finalBones[100]; };                // BINDING 2
#else
cbuffer SkinnedObjectData : register(b1) // BINDING 1 v DX12
{
    matrix model;
    float4 pad2[4];
};
cbuffer BoneData : register(b2) // BINDING 2 v DX12
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

float4 VSMain(VS_INPUT input) : SV_POSITION
{
    matrix boneTransform = finalBones[input.boneIDs.x] * input.boneWeights.x;
    boneTransform += finalBones[input.boneIDs.y] * input.boneWeights.y;
    boneTransform += finalBones[input.boneIDs.z] * input.boneWeights.z;
    boneTransform += finalBones[input.boneIDs.w] * input.boneWeights.w;
    float4 worldPos = mul(mul(float4(input.pos, 1.0f), boneTransform), model);
    return mul(mul(worldPos, view), projection);
}