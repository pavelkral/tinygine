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
    
    // Bindless sampling of heightmap. Clamp the UV half a texel inside the tile
    // so an edge vertex (uv 0/1) never samples across the sampler's WRAP boundary
    // (which would pull in the opposite edge and create huge seams/walls).
    float2 hUV = clamp(input.uv, 0.5 / 256.0, 1.0 - 0.5 / 256.0);
    float h = g_Textures[NonUniformResourceIndex(input.heightMapIndex)].SampleLevel(g_Sampler, hUV, 0).r;

    float3 absPos;
    float skirtOffset = 0.0;
    float2 gridXZ = float2(input.pos.x, input.pos.z);
    if (input.pos.y < -0.01)
    {
        // Skirt (edge) vertices: drop down enough to cover seam gaps between
        // tiles (was * 4.0 ~= 5 km walls). 0.5 still covers any realistic seam.
        skirtOffset = input.pos.y * input.scale * 0.5;
        // Recess the skirt bottom slightly toward the tile centre so adjacent
        // tiles' skirts diverge below the shared edge instead of overlapping
        // exactly -> removes the z-fighting between coincident skirts.
        gridXZ = lerp(gridXZ, float2(0.5, 0.5), 0.02);
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
