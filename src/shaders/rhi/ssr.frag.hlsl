struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D colorTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D posTex : register(t2);
Texture2D texSSAO : register(t3);
SamplerState smp : register(s0);

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

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float4 PSMain(VSOutput input) : SV_Target
{
    float4 albedoData = colorTex.Sample(smp, input.uv);
    float3 albedo = albedoData.rgb;
    float roughness = albedoData.a;
    float ssao = 1.0f;
    
    if (enableSSAO == 1)
    {
        ssao = texSSAO.Sample(smp, input.uv).r;
    }

    float3 normal = normalTex.Sample(smp, input.uv).xyz;
    float3 worldPos = posTex.Sample(smp, input.uv).xyz;

    albedo *= ssao;

    if (length(worldPos) < 0.1f || length(normal) < 0.1f || roughness > 0.8f)
    {
        return float4(albedo, 1.0f);
    }

    float3 viewDir = normalize(worldPos - camPos);
    float3 reflectDir = normalize(reflect(viewDir, normal));
    matrix viewProj = mul(view, proj);
    
    float stepSize = 0.8f;
    int maxSteps = 40;
    int binarySearchSteps = 6;
    float thickness = 0.6f;

    float jitter = Hash(input.uv * worldPos.xz) * stepSize;
    float3 currentPos = worldPos + reflectDir * (stepSize + jitter);
    
    float hitVisibility = 0.0f;
    float3 reflectColor = float3(0, 0, 0);
    bool hitFound = false;

    for (int i = 0; i < maxSteps; i++)
    {
        float4 projPos = mul(float4(currentPos, 1.0f), viewProj);
        if (projPos.w <= 0.0f)
            break;
        
        projPos.xyz /= projPos.w;
        float2 sampleUV = float2(projPos.x, -projPos.y) * 0.5f + 0.5f;

        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            break;

        float3 sampledPos = posTex.SampleLevel(smp, sampleUV, 0).xyz;
        
        if (length(sampledPos) > 0.1f)
        {
            float distCamToCurrent = distance(currentPos, camPos);
            float distCamToSampled = distance(sampledPos, camPos);
            float depthDiff = distCamToCurrent - distCamToSampled;
            
            if (depthDiff > 0.0f && depthDiff < thickness)
            {
                float3 minPos = currentPos - reflectDir * stepSize;
                float3 maxPos = currentPos;
                float3 midPos;
                
                for (int j = 0; j < binarySearchSteps; j++)
                {
                    midPos = lerp(minPos, maxPos, 0.5f);
                    
                    float4 mProj = mul(float4(midPos, 1.0f), viewProj);
                    mProj.xyz /= mProj.w;
                    float2 mUV = float2(mProj.x, -mProj.y) * 0.5f + 0.5f;
                    
                    float3 sPos = posTex.SampleLevel(smp, mUV, 0).xyz;
                    float mDiff = distance(midPos, camPos) - distance(sPos, camPos);
                    
                    if (mDiff > 0.0f && mDiff < thickness)
                        maxPos = midPos;
                    else
                        minPos = midPos;
                }
                
                float4 fProj = mul(float4(midPos, 1.0f), viewProj);
                fProj.xyz /= fProj.w;
                float2 finalUV = float2(fProj.x, -fProj.y) * 0.5f + 0.5f;

                reflectColor = colorTex.SampleLevel(smp, finalUV, 0).rgb;
                
                float2 edgeFade = smoothstep(0.0f, 0.05f, finalUV) * smoothstep(1.0f, 0.95f, finalUV);
                hitVisibility = edgeFade.x * edgeFade.y;
                hitVisibility *= (1.0f - roughness);
                
                hitFound = true;
                break;
            }
        }
        currentPos += reflectDir * stepSize;
    }
    
    if (hitFound)
    {
        return float4(lerp(albedo, reflectColor * ssao, hitVisibility * 0.4f), 1.0f);
    }
    
    return float4(albedo, 1.0f);
}