Texture2D texPosition : register(t0);
Texture2D texNormal : register(t1);
SamplerState samp : register(s0);

cbuffer SSAOData : register(b0)
{
    matrix viewProjection;
    float4 samples[64];
    float3 camPos;
    float radius;
    float bias;
    float2 screenSize;
    float pad1;
    float pad2;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// Generator noise for on GPU
float rand(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    float3 fragPos = texPosition.SampleLevel(samp, input.uv, 0).xyz;
    if (length(fragPos) < 0.1)
        return float4(1.0, 1.0, 1.0, 1.0);

    float3 normal = normalize(texNormal.SampleLevel(samp, input.uv, 0).xyz);

    float2 noiseScale = screenSize / 4.0;
    float3 randomVec = normalize(float3(
        rand(input.uv * noiseScale) * 2.0 - 1.0,
        rand(input.uv * noiseScale * 1.23) * 2.0 - 1.0,
        0.0
    ));

    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < 64; ++i)
    {
        float3 sampleDir = mul(samples[i].xyz, TBN);
        float3 samplePos = fragPos + sampleDir * radius;
        
        float4 offset = float4(samplePos, 1.0);
        offset = mul(offset, viewProjection);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        offset.y = 1.0 - offset.y;

        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
            continue;

        float3 hitPos = texPosition.SampleLevel(samp, offset.xy, 0).xyz;
        if (length(hitPos) < 0.1)
            continue;

        float sampleDepth = distance(camPos, hitPos);
        float currentDepth = distance(camPos, samplePos);

        float rangeCheck = smoothstep(0.0, 1.0, radius / distance(fragPos, hitPos));
        occlusion += (sampleDepth <= currentDepth - bias ? 1.0 : 0.0) * rangeCheck;
    }
    
    occlusion = 1.0 - (occlusion / 64.0);
    occlusion = pow(occlusion, 1.5);
    
    return float4(occlusion, occlusion, occlusion, 1.0);
}