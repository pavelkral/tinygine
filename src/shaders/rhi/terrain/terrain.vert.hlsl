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

#define MAX_BINDLESS_TEXTURES 4096

// Fixed-size bindless table. DX12 binds it from the shader-visible SRV heap;
// Vulkan maps t0 to binding 3 through the existing compiler shifts.
Texture2D g_Textures[MAX_BINDLESS_TEXTURES] : register(t0, space0);
SamplerState g_Sampler : register(s0);
// Dedicated terrain sampler: CLAMP addressing so an edge vertex (uv 0/1) never
// samples across a wrap boundary into the opposite tile edge.
SamplerState g_SamplerClamp : register(s2);

struct VS_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
    float3 worldPos : TEXCOORD3;
    float scale : TEXCOORD4;
    uint heightMapIndex : TEXCOORD5;
    uint colorMapIndex : TEXCOORD6;
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

PS_IN VSMain(VS_IN input)
{
    PS_IN output;
    
    // Bindless sampling of heightmap with the CLAMP terrain sampler, so the raw
    // 0..1 UV is safe at edges (no wrap to the opposite side, no hardcoded texel
    // size). Adjacent-tile edge matching is handled by CPU-side edge stitching.
    float h = g_Textures[NonUniformResourceIndex(input.heightMapIndex)].SampleLevel(g_SamplerClamp, input.uv, 0).r;

    float3 absPos;
    float skirtOffset = 0.0;
    float2 gridXZ = float2(input.pos.x, input.pos.z);
    if (input.pos.y < -0.01)
    {
        // Skirt (edge) vertices: drop straight down to cover seam gaps between
        // tiles. No inward inset anymore - skirts are emitted on only two edges
        // per tile (+X / +Z), so each shared seam is covered by exactly ONE
        // skirt. There are no coincident overlapping skirts to z-fight, and CPU
        // edge stitching keeps adjacent heights equal so the seam stays hidden.
        skirtOffset = input.pos.y * input.scale * 0.5;
    }
    absPos.x = input.worldPos.x + gridXZ.x * input.scale;
    absPos.y = input.worldPos.y + (h * HEIGHT_SCALE) + skirtOffset;
    absPos.z = input.worldPos.z + gridXZ.y * input.scale;

    float dist = length(absPos.xz - camPos.xz);
    float drop = (dist * dist) / (2.0 * EARTH_RADIUS);
    absPos.y -= drop;

    // The view matrix is a FULL LookTo (eye = camera), so it already subtracts
    // the camera position. Do NOT subtract camPos here as well, otherwise the
    // terrain is offset by camPos and "swims" relative to objects as the camera
    // moves (objects use absolute world positions + the same view).
    output.relPos = absPos - camPos; // kept relative for PS world-space (matches old behavior)

    float4 vPos = mul(float4(absPos, 1.0), view);
    output.pos = mul(vPos, projection);
    output.uv = input.uv;
    output.colorIdx = input.colorMapIndex;
    
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
