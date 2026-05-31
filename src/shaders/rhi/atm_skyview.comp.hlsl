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

RWTexture2D<float4> rwSkyViewLUT : register(u0); // Output
Texture2D<float4> texTransmittance : register(t0); // Read from the first pass
SamplerState g_Sampler : register(s0);

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

float3 GetTransmittance(float r, float mu)
{
    float u = (mu + 1.0) * 0.5;
    float v = saturate((r - planetRadius) / (atmosphereRadius - planetRadius));
    return texTransmittance.SampleLevel(g_Sampler, float2(u, v), 0).rgb;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float w, h;
    rwSkyViewLUT.GetDimensions(w, h);
    if (id.x >= (uint) w || id.y >= (uint) h)
        return;

    float2 uv = (float2(id.xy) + 0.5) / float2(w, h);

    float azimuth = uv.x * 2.0 * PI;
    float elevation = (uv.y - 0.5) * PI;
    
    float3 viewDir = float3(cos(elevation) * sin(azimuth), sin(elevation), cos(elevation) * cos(azimuth));

    float camHeight = length(cameraPos);
    float3 r0 = float3(0, camHeight, 0);
    
    float2 atmHit = RaySphere(r0, viewDir, atmosphereRadius);
    float tMax = atmHit.y;
    float2 planetHit = RaySphere(r0, viewDir, planetRadius);
    if (planetHit.x > 0.0)
        tMax = planetHit.x;

    if (tMax < 0.0)
    {
        rwSkyViewLUT[id.xy] = float4(0, 0, 0, 1);
        return;
    }

    int steps = 32;
    float dt = tMax / float(steps);
    float3 p = r0 + viewDir * dt * 0.5;

    float3 L = float3(0, 0, 0);
    float3 T = float3(1, 1, 1);
    
    // - Sun direction and phase functions
    float3 sunDirN = normalize(sunDir);

    float mu_s = dot(viewDir, sunDirN);
    float phaseR = 3.0 / (16.0 * PI) * (1.0 + mu_s * mu_s);
    float phaseM = 3.0 / (8.0 * PI) * ((1.0 - mieG * mieG) * (1.0 + mu_s * mu_s)) / ((2.0 + mieG * mieG) * pow(abs(1.0 + mieG * mieG - 2.0 * mieG * mu_s), 1.5));

    for (int i = 0; i < steps; i++)
    {
        float r = length(p);
        float h_p = max(0.0, r - planetRadius);

        float dR = exp(-h_p / rayleighScaleHeight) * dt;
        float dM = exp(-h_p / mieScaleHeight) * dt;
        float dO = max(0.0, 1.0 - abs(h_p - 25000.0) / 15000.0) * dt;

        float3 ext = rayleighCoeff * dR + mieCoeff * dM + ozoneCoeff * dO;
        float3 sampleT = exp(-ext);

        float sunCos = dot(p / r, sunDirN);
        float3 sunTrans = GetTransmittance(r, sunCos);
        
        if (RaySphere(p, sunDirN, planetRadius).x > 0.0)
            sunTrans = float3(0, 0, 0);

        float3 S_total = (rayleighCoeff * dR * phaseR + mieCoeff * dM * phaseM) * sunTrans * sunIntensity;
        S_total += (rayleighCoeff * dR) * float3(0.05, 0.05, 0.05) * sunIntensity;

        float3 integFactor = (float3(1.0, 1.0, 1.0) - sampleT) / max(ext, 1e-7);
        float3 Sint = S_total * integFactor;
        
        L += T * Sint;
        T *= sampleT;

        p += viewDir * dt;
    }

    rwSkyViewLUT[id.xy] = float4(L, 1.0);
}