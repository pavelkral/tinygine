
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
    float3 color : NORMAL; // line color stored in normal attribute for simplicity
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 sv_pos : SV_POSITION;
    float3 color : COLOR;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    
    // Position is already in World Space (calculated on CPU), so we only multiply by View and Projection
    float4 viewPos = mul(float4(input.pos, 1.0f), view);
    output.sv_pos = mul(viewPos, projection);
    
    output.color = input.color;
    return output;
}