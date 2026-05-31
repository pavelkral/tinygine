cbuffer AtmosphereParams : register(b0)
{
    float3 sunDir;
    float planetRadius;
    float3 rayleighCoeff;
    float atmosphereRadius;
    float3 mieCoeff;
    float rayleighScaleHeight;
    float3 ozoneCoeff;
    float mieScaleHeight;
    float3 cameraPos;
    float mieG;
    float sunIntensity;
    float3 pad;
};

// write-only to texture UAV (Unordered Access View)
RWTexture2D<float4> rwTransmittanceLUT : register(u0);

static const float PI = 3.14159265359;

float2 RaySphere(float3 ro, float3 rd, float radius)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float h = b * b - c;
    if (h < 0.0)
        return float2(-1.0, -1.0);
    h = sqrt(h);
    return float2(-b - h, -b + h);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float w, h;
    rwTransmittanceLUT.GetDimensions(w, h);
    if (id.x >= (uint) w || id.y >= (uint) h)
        return;

    float2 uv = (float2(id.xy) + 0.5) / float2(w, h);

    float r = lerp(planetRadius, atmosphereRadius, uv.y);
    float mu = lerp(-1.0, 1.0, uv.x);

    float3 ro = float3(0, r, 0);
    float3 rd = float3(sqrt(max(0.0, 1.0 - mu * mu)), mu, 0);

    float dist = RaySphere(ro, rd, atmosphereRadius).y;
    if (RaySphere(ro, rd, planetRadius).x > 0.0)
    {
        dist = RaySphere(ro, rd, planetRadius).x;
    }

    int steps = 40;
    float dt = dist / float(steps);
    float3 p = ro + rd * dt * 0.5;

    float optR = 0, optM = 0, optO = 0;

    for (int i = 0; i < steps; i++)
    {
        float height = max(0.0, length(p) - planetRadius);
        
        optR += exp(-height / rayleighScaleHeight) * dt;
        optM += exp(-height / mieScaleHeight) * dt;
        optO += max(0.0, 1.0 - abs(height - 25000.0) / 15000.0) * dt;
        p += rd * dt;
    }

    float3 transmittance = exp(-(rayleighCoeff * optR + mieCoeff * optM + ozoneCoeff * optO));
    rwTransmittanceLUT[id.xy] = float4(transmittance, 1.0);
}