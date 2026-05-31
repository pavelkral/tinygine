// ============================================================================
// 1. CONSTANT BUFFERS
// ============================================================================

struct PointLightData
{
    float3 position;
    float radius;
    float3 color;
    float intensity;
};

cbuffer GlobalData : register(b0)
{
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

#ifdef VULKAN
    cbuffer SkinnedObjectData : register(b1) { 
        matrix model; 
        float4 baseColor; 
        float roughness; 
        float metalness;
        float hasAlbedoTex; 
        float hasNormalTex; 
        float hasMetalTex; 
        float hasRoughTex; 
        float alphaCutoff; 
        float padObj; 
    };
#else
cbuffer SkinnedObjectData : register(b1)
{
    matrix model;
    float4 baseColor;
    float roughness;
    float metalness;
    float hasAlbedoTex;
    float hasNormalTex;
    float hasMetalTex;
    float hasRoughTex;
    float alphaCutoff;
    float padObj;
};
#endif

// ============================================================================
// 2. TEXTURES & SAMPLERS
// ============================================================================

Texture2D texAlbedo : register(t0);
Texture2D texNormal : register(t1);
Texture2D texMetal : register(t2);
Texture2D texRough : register(t3);
Texture2D shadowMap : register(t4);

TextureCube irradianceMap : register(t5);
TextureCube prefilterMap : register(t6);
Texture2D brdfLUT : register(t7);

SamplerState samp : register(s0);
SamplerComparisonState shadowSampler : register(s1);

static const float PI = 3.14159265359;

// ============================================================================
// 3. I/O STRUCTURES
// ============================================================================

struct PS_INPUT
{
    float4 sv_pos : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 lightSpacePos : TEXCOORD1;
};

struct PixelOutput
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
};

// ============================================================================
// 4. PBR HELPER FUNCTIONS
// ============================================================================

float3 PerturbNormal(float3 N, float3 worldPos, float2 texcoord)
{
    float3 dp1 = ddx(worldPos);
    float3 dp2 = ddy(worldPos);
    float2 duv1 = ddx(texcoord);
    float2 duv2 = ddy(texcoord);
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    float3x3 TBN = float3x3(T * invmax, B * invmax, N);
    float3 map = texNormal.Sample(samp, texcoord).rgb;
    map = map * 2.0 - 1.0;
    return normalize(mul(map, TBN));
}

float DistributionGGX(float3 N, float3 H, float r)
{
    float a = r * r;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float k)
{
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float r)
{
    float k = pow(r + 1.0, 2.0) / 8.0;
    return GeometrySchlickGGX(max(dot(N, V), 0.0), k) * GeometrySchlickGGX(max(dot(N, L), 0.0), k);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(float4 fragPosLight, float3 N, float3 L)
{
    float3 projCoords = fragPosLight.xyz / fragPosLight.w;
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = -projCoords.y * 0.5 + 0.5;
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;
    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);
    float currentDepth = projCoords.z - bias;
    float shadow = 0.0;
    const float2 texelSize = 1.0 / 2048.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, projCoords.xy + float2(x, y) * texelSize, currentDepth);
        }
    }
    return 1.0 - (shadow / 9.0);
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

//============================================================================
float3 CalculateLight(float3 L, float3 V, float3 N, float3 radiance, float3 albedo, float r, float m, float3 F0)
{
    float3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, r);
    float G = GeometrySmith(N, V, L, r);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    float3 specular = numerator / denominator;
    
    float3 kS = F;
    float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - m);
    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// ============================================================================
// 5. PIXEL SHADER MAIN
// ============================================================================

PixelOutput PSMain(PS_INPUT input)
{
    float4 albedoColor = baseColor;
    if (hasAlbedoTex > 0.5f)
        albedoColor *= texAlbedo.Sample(samp, input.uv);

    clip(albedoColor.a - alphaCutoff);

    float3 albedo = pow(abs(albedoColor.rgb), 2.2);
    
    float r = roughness;
    if (hasRoughTex > 0.5f)
        r *= texRough.Sample(samp, input.uv).r;
        
    float m = metalness;
    if (hasMetalTex > 0.5f)
        m *= texMetal.Sample(samp, input.uv).r;

    float3 N = normalize(input.normal);
    float3 V = normalize(camPos - input.worldPos);
    
    if (hasNormalTex > 0.5f)
        N = PerturbNormal(N, input.worldPos, input.uv);
        
    float3 R = reflect(-V, N);
    
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, m);
    
    float3 Lo = float3(0.0, 0.0, 0.0);
    float mainShadow = 0.0;

    // --- 1. DIRECTIONAL LIGHT (sun) ---
    if (dirLightIntensity > 0.0)
    {
        float3 L = normalize(-dirLightDir);
        float3 radiance = dirLightColor * dirLightIntensity;
        
        if (hasShadowMap == 1)
            mainShadow = ShadowCalculation(input.lightSpacePos, N, L);

        Lo += CalculateLight(L, V, N, radiance, albedo, r, m, F0) * (1.0 - mainShadow);
    }

    // --- 2. POINT LIGHTS ---
    for (int i = 0; i < numPointLights; i++)
    {
        float3 lightVec = pointLights[i].position - input.worldPos;
        float dist = length(lightVec);
        
        // calculation of light 
        if (dist < pointLights[i].radius)
        {
            float3 L = normalize(lightVec);
            
            // Attenuation
            float attenuation = clamp(1.0 - (dist * dist) / (pointLights[i].radius * pointLights[i].radius), 0.0, 1.0);
            attenuation *= attenuation; // Smooth falloff
            
            float3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation;
            Lo += CalculateLight(L, V, N, radiance, albedo, r, m, F0);
        }
    }

    // --- 3. AMBIENT (IBL) ---
    float3 ambient = float3(0.0, 0.0, 0.0);
    
    if (hasIBL > 0.5f)
    {
        float3 F_ibl = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, r);
        float3 kS_ibl = F_ibl;
        float3 kD_ibl = 1.0 - kS_ibl;
        kD_ibl *= 1.0 - m;
        
        float3 irradiance = irradianceMap.Sample(samp, N).rgb;
        float3 diffuseIBL = irradiance * albedo;
        
        const float MAX_REFLECTION_LOD = 4.0;
        float3 prefilteredColor = prefilterMap.SampleLevel(samp, R, r * MAX_REFLECTION_LOD).rgb;
        float2 envBRDF = brdfLUT.Sample(samp, float2(max(dot(N, V), 0.0), r)).rg;
        float3 specularIBL = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);
        
        ambient = (kD_ibl * diffuseIBL + specularIBL);
        // Sun shadow subtly affects ambient reflections (so objects don't appear bright in darkness)
        ambient *= lerp(1.0f, (1.0f - mainShadow), 0.6f);
    }
    else
    {
        ambient = float3(0.03, 0.03, 0.03) * albedo;
    }

    float3 color = ambient + Lo;
    
    // Tonemapping and Gamma correction
    float exposure = 0.2f;
    color *= exposure;
    color = ACESFilm(color);
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    
    // --- OUTPUT TO G-BUFFER (MRT) ---
    PixelOutput output;
    output.Color = float4(color, r);
    output.Normal = float4(N, 1.0f);
    output.WorldPos = float4(input.worldPos, 1.0f);
    
    return output;
}