#define MAX_BINDLESS_TEXTURES 4096

Texture2D g_Textures[MAX_BINDLESS_TEXTURES] : register(t0, space0);
SamplerState g_Sampler : register(s0);
// Dedicated terrain sampler: CLAMP addressing (see terrain.vert.hlsl).
SamplerState g_SamplerClamp : register(s2);

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 relPos : TEXCOORD1;
    nointerpolation uint colorIdx : COLOR_IDX;
};

struct PixelOutput
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
};

PixelOutput PSMain(PS_IN input)
{
    // Generate real surface normals on the fly (saves memory over uploading normal maps)
    float3 ddxPos = ddx(input.relPos);
    float3 ddyPos = ddy(input.relPos);
    float3 realNormal = normalize(cross(ddyPos, ddxPos));
    
    if (realNormal.y < 0.0)
        realNormal = -realNormal; // Make sure it points up

    // Bindless color sampling with the CLAMP terrain sampler (raw 0..1 UV; the
    // edge texel never wraps to the opposite side at the seam).
    float4 color = g_Textures[NonUniformResourceIndex(input.colorIdx)].Sample(g_SamplerClamp, input.uv);

    PixelOutput output;
    output.Color = color;
    output.Normal = float4(realNormal, 1.0);
    output.WorldPos = float4(input.relPos, 1.0);

    return output;
}
