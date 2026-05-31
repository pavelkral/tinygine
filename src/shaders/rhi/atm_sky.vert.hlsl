cbuffer GlobalData : register(b0)
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

struct VS_IN
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float2 tex : TEXCOORD0;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 localPos : TEXCOORD0;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;
    output.localPos = input.pos;
    
    matrix viewNoTrans = view;
    viewNoTrans[3][0] = 0;
    viewNoTrans[3][1] = 0;
    viewNoTrans[3][2] = 0;
    
    float4 scaledPos = float4(input.pos * 50.0f, 1.0f);
    float4 pos = mul(mul(scaledPos, viewNoTrans), projection);
    
    output.pos = pos.xyww;
    return output;
}