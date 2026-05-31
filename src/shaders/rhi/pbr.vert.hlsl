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

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;

    float4 modelRow0 : TEXCOORD3;
    float4 modelRow1 : TEXCOORD4;
    float4 modelRow2 : TEXCOORD5;
    float4 modelRow3 : TEXCOORD6;
    float4 baseColor : TEXCOORD7;
    float4 matParams1 : TEXCOORD8;
    float4 matParams2 : TEXCOORD9;
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
    matrix model = matrix(input.modelRow0, input.modelRow1, input.modelRow2, input.modelRow3);
    
    float4 worldPos = mul(float4(input.pos, 1.0f), model);
    output.worldPos = worldPos.xyz;
    output.sv_pos = mul(mul(worldPos, view), projection);
    output.lightSpacePos = mul(worldPos, lightSpaceMatrix);
    output.normal = normalize(mul(input.normal, (float3x3) model));
    output.uv = input.uv;

    output.baseColor = input.baseColor;
    output.roughness = input.matParams1.x;
    output.metalness = input.matParams1.y;
    output.hasAlbedoTex = input.matParams1.z;
    output.hasNormalTex = input.matParams1.w;
    output.hasMetalTex = input.matParams2.x;
    output.hasRoughTex = input.matParams2.y;
    
    // Alpha not supported instantly, but we can set cutoff to 0 to avoid discarding pixels in the shader
    output.alphaCutoff = 0.0f;
    
    return output;
}