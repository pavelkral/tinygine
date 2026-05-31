struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D colorTex : register(t0);
Texture2D texSSAO : register(t3);
SamplerState smp : register(s0);

struct PointLightData {
    float3 position;
    float radius;
    float3 color;
    float intensity;
};

cbuffer GlobalData : register(b0) {
    matrix view;
    matrix proj;
    matrix lightSpaceMatrix;
    
    float3 camPos;
    float hasIBL;
    
    float3 dirLightDir;
    float dirLightIntensity;
    float3 dirLightColor;
    int hasShadowMap;
    
    int enableSSAO;
    int numPointLights;
    float2 pad;
    
    PointLightData pointLights[16];
};

float4 PSMain(VSOutput input) : SV_Target
{
    float4 color = colorTex.Sample(smp, input.uv);
    float ssao = 1.0f;
    
    if (enableSSAO == 1)
    {
        ssao = texSSAO.Sample(smp, input.uv).r;
    }
    
    return float4(color.rgb * ssao, color.a);
}