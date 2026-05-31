cbuffer GlobalData : register(b0)
{
    matrix view;
    matrix projection;
    matrix lightSpaceMatrix;
    float3 camPos;
    // ...
};

struct VS_IN
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    
    // data for instancing (position, life, velocity, size, color)
    float4 instPosLife : TEXCOORD3;
    float4 instVelSize : TEXCOORD4;
    float4 instColor : TEXCOORD5;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;
    
    float3 worldPos = input.instPosLife.xyz;
    float size = input.instVelSize.w;
    
    // Billboard math - orient quad to camera
    float3 camRight = float3(view[0][0], view[1][0], view[2][0]);
    float3 camUp = float3(view[0][1], view[1][1], view[2][1]);
    
    // Add quad vertices oriented directly to the camera
    worldPos += (camRight * input.pos.x * size) + (camUp * input.pos.y * size);
    
    output.pos = mul(mul(float4(worldPos, 1.0), view), projection);
    output.uv = input.uv;
    output.color = input.instColor;
    
    return output;
}