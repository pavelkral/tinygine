cbuffer GlobalData : register(b0)
{
    matrix view;
    matrix projection;
    float4 pad[4];
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL; // not used
    float2 uv : TEXCOORD0; //   
};

struct PS_INPUT
{
    float4 sv_pos : SV_POSITION;
    float3 localPos : POSITION;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    
    // Local position for culling and sampling the cubemap in the pixel shader
    output.localPos = input.pos;
    
    // remove camera translation from the view matrix for skybox rendering
    matrix viewNoTranslation = view;
    viewNoTranslation[3][0] = 0.0f;
    viewNoTranslation[3][1] = 0.0f;
    viewNoTranslation[3][2] = 0.0f;
    
    // Scale the cube to ensure it is not clipped by the camera's "Near" plane.
    // Here we give it a size of 5000x5000x5000. (Depends on your Far plane, 5000.0 is fine)
    float4 scaledPos = float4(input.pos * 5000.0f, 1.0f);
    
    output.sv_pos = mul(mul(scaledPos, viewNoTranslation), projection);
    
    return output;
}