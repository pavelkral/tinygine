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

Texture2D<float4> texSkyView : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 localPos : TEXCOORD0;
};

// mrt struct for outputting to multiple render targets (color, normal, world position)
struct PixelOutput
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
};

static const float PI = 3.14159265359;

// MAIN FUNCTION
PixelOutput PSMain(PS_IN input)
{
    float3 viewDir = normalize(input.localPos);

    // 1.skyview texture mapping lut
    float azimuth = atan2(viewDir.x, viewDir.z);
    if (azimuth < 0.0)
        azimuth += 2.0 * PI;
    float elevation = asin(viewDir.y);

    float u = azimuth / (2.0 * PI);
    float v = (elevation / PI) + 0.5;

    float3 color = texSkyView.SampleLevel(g_Sampler, float2(u, v), 0).rgb;
    color *= 0.15; // exposure factor for the sky texture, adjust as needed

    // sun lightE
    float3 sunDirN = normalize(lightPos);
    float sunCos = dot(viewDir, sunDirN);
        
    // Sun disk
    float sunDisk = smoothstep(0.99995, 0.99998, sunCos);
        
    // Halo around the sun
    float sunGlow = pow(max(0.0, sunCos), 4000.0) * 0.5;

    // Sun color
    float3 sunCol = float3(1.0, 0.95, 0.8) * 15.0;
        
    // Add the sun to the sky
    color += (sunDisk + sunGlow) * sunCol;

    // 3. TONEMAPPING A GAMMA (PBR Standard ACES)
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    color = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0f, 1.0f);
    
    color = pow(abs(color), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    // 4. OUTPUT TO ALL 3 TEXTURES (MRT)
    PixelOutput output;
    output.Color = float4(max(float3(0, 0, 0), color), 1.0);
    output.Normal = float4(0, 0, 0, 0); // Zero normal ensures SSR ignores the sky
    output.WorldPos = float4(0, 0, 0, 0); // Same with position
    return output;
}