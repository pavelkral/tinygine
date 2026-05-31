cbuffer GlobalData : register(b0)
{
    matrix view;
    matrix projection;
    matrix lightSpaceMatrix;
    float3 camPos;
    float pad1;
    float3 lightPos;
    float pad2;
    float3 lightColor;
    float lightIntensity;
    int hasShadowMap;
    float3 pad3;
};

#ifdef VULKAN
    struct PushData {
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
    [[vk::push_constant]] PushData pc;
#define MODEL pc.model
#define BASECOLOR pc.baseColor
#define ROUGHNESS pc.roughness
#define METALNESS pc.metalness
#define HAS_ALBEDO pc.hasAlbedoTex
#define HAS_NORMAL pc.hasNormalTex
#define HAS_METAL pc.hasMetalTex
#define HAS_ROUGH pc.hasRoughTex
#define ALPHACUTOFF pc.alphaCutoff
#else
cbuffer ObjectData : register(b1)
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
#define MODEL model
#define BASECOLOR baseColor
#define ROUGHNESS roughness
#define METALNESS metalness
#define HAS_ALBEDO hasAlbedoTex
#define HAS_NORMAL hasNormalTex
#define HAS_METAL hasMetalTex
#define HAS_ROUGH hasRoughTex
#define ALPHACUTOFF alphaCutoff
#endif

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
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
    
    float4 worldPos = mul(float4(input.pos, 1.0f), MODEL);
    output.worldPos = worldPos.xyz;
    output.sv_pos = mul(mul(worldPos, view), projection);
    output.lightSpacePos = mul(worldPos, lightSpaceMatrix);
    output.normal = normalize(mul(input.normal, (float3x3) MODEL));
    output.uv = input.uv;

    output.baseColor = BASECOLOR;
    output.roughness = ROUGHNESS;
    output.metalness = METALNESS;
    output.hasAlbedoTex = HAS_ALBEDO;
    output.hasNormalTex = HAS_NORMAL;
    output.hasMetalTex = HAS_METAL;
    output.hasRoughTex = HAS_ROUGH;
    output.alphaCutoff = ALPHACUTOFF;
    
    return output;
}