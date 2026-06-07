// Bindless resources approach! 
cbuffer GlobalData : register(b0)
{
    matrix view;
    matrix projection;
    matrix lightSpaceMatrix;
    float3 camPos;
    float pad;
    // other padding depending on your GlobalData size
};

struct InstanceData
{
    float3 worldPos;
    float scale;
    uint heightMapIndex;
    uint colorMapIndex;
};

// Texture unbounded array for Bindless
Texture2D g_Textures[] : register(t0, space0);
SamplerState g_Sampler : register(s0);

struct VS_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 relPos : TEXCOORD1;
    nointerpolation uint colorIdx : COLOR_IDX;
};

static const float EARTH_RADIUS = 6371000.0;
static const float HEIGHT_SCALE = 1603.0;

PS_IN VSMain(VS_IN input, uint instanceID : SV_InstanceID, StructuredBuffer<InstanceData> g_Instances : register(t1, space0))
{
    PS_IN output;
    InstanceData inst = g_Instances[instanceID];
    
    // Bindless sampling of heightmap
    float h = g_Textures[NonUniformResourceIndex(inst.heightMapIndex)].SampleLevel(g_Sampler, input.uv, 0).r;

    float3 absPos;
    absPos.x = inst.worldPos.x + input.pos.x * inst.scale;
    float skirtOffset = 0.0;
    if (input.pos.y < -0.01)
        skirtOffset = input.pos.y * inst.scale * 4.0;

    absPos.y = inst.worldPos.y + (h * HEIGHT_SCALE) + skirtOffset;
    absPos.z = inst.worldPos.z + input.pos.z * inst.scale;

    float dist = length(absPos.xz - camPos.xz);
    float drop = (dist * dist) / (2.0 * EARTH_RADIUS);
    absPos.y -= drop;

    float3 rPos = absPos - camPos;
    output.relPos = rPos;

    float4 vPos = mul(float4(rPos, 1.0), view);
    output.pos = mul(vPos, projection);
    output.uv = input.uv;
    output.colorIdx = inst.colorMapIndex;
    
    if (input.pos.y < -0.01)
    {
        if (input.pos.z > 0.99)
            output.normal = float3(0, 0, 1);
        else if (input.pos.z < 0.01)
            output.normal = float3(0, 0, -1);
        else if (input.pos.x > 0.99)
            output.normal = float3(1, 0, 0);
        else if (input.pos.x < 0.01)
            output.normal = float3(-1, 0, 0);
        else
            output.normal = float3(0, 1, 0);
    }
    else
    {
        output.normal = float3(0, 1, 0);
    }
    return output;
}