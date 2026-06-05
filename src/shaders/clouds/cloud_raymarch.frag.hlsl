Texture3D<float4> g_ShapeNoise : register(t0);
Texture3D<float4> g_DetailNoise : register(t1);
Texture2D<float4> g_PosMap : register(t2);
Texture2D<float4> g_WeatherMap : register(t3);

SamplerState g_Sampler : register(s0);

// STRUKTURA NYNÍ PERFEKTNÌ LÍCUJE S C++!
cbuffer CloudCB : register(b0)
{
    float4 camForward;
    float4 camRight;
    float4 camUp;
    float3 camPosAbs; // PØESNÌ JAKO V C++
    float timeSeconds;
    float3 sunDir;
    float planetRadius;
    
    float2 weatherOffset;
    float2 shapeOffset;
    float2 detailOffset;
    float tanHalfFov;
    float aspect;

    float4 shapeParams;
    float4 typeParams;
    float4 layerParams;
    float4 lightParams;
    
    float3 cSun;
    float sunInt;
    float3 cAmbTop;
    float ambInt;
    float3 cAmbBot;
    float turbulenceMeters;
    
    float4x4 invProj;
    float horizonFadeEnd;
}

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

static const float3 NOISE_KERNEL[6] =
{
    float3(0.38, 0.92, -0.02), float3(-0.5, -0.03, -0.86),
    float3(-0.32, -0.94, 0.01), float3(0.09, -0.27, 0.95),
    float3(0.28, 0.42, -0.86), float3(-0.16, 0.14, 0.97)
};

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float HG(float costh, float g)
{
    float gg = g * g;
    float denom = pow(abs(1.0 + gg - 2.0 * g * costh), 1.5);
    return (1.0 - gg) / max(1e-6, (4.0 * 3.14159265 * denom));
}

float2 RaySphere(float3 r0, float3 rd, float sr)
{
    float a = dot(rd, rd);
    float b = 2.0 * dot(r0, rd);
    float c = dot(r0, r0) - (sr * sr);
    float d = (b * b) - 4.0 * a * c;
    if (d < 0.0)
        return float2(-1.0, -1.0);
    return float2((-b - sqrt(d)) / (2.0 * a), (-b + sqrt(d)) / (2.0 * a));
}

float Remap01(float v, float a, float b)
{
    return saturate((v - a) / max(1e-6, (b - a)));
}

float ProfileStratus(float h)
{
    return smoothstep(0.00, 0.08, h) * (1.0 - smoothstep(0.25, 0.55, h));
}
float ProfileStratoCu(float h)
{
    return smoothstep(0.00, 0.10, h) * (1.0 - smoothstep(0.45, 0.85, h));
}
float ProfileCumulus(float h)
{
    return smoothstep(0.05, 0.18, h) * (1.0 - smoothstep(0.70, 1.00, h));
}
float ProfileCumulonimbus(float h)
{
    return smoothstep(0.02, 0.12, h) * (1.0 - smoothstep(0.88, 1.00, h));
}

float HeightProfile(float h, float type)
{
    float t = saturate(type) * 3.0;
    float i = floor(t);
    float f = frac(t);
    float p0 = (i < 0.5) ? ProfileStratus(h) : ((i < 1.5) ? ProfileStratoCu(h) : ((i < 2.5) ? ProfileCumulus(h) : ProfileCumulonimbus(h)));
    float p1 = (i < 0.5) ? ProfileStratoCu(h) : ((i < 1.5) ? ProfileCumulus(h) : ((i < 2.5) ? ProfileCumulonimbus(h) : ProfileCumulonimbus(h)));
    return lerp(p0, p1, f);
}

float SampleDensity(float3 localPos, bool cheap)
{
    float3 worldPos = localPos + camPosAbs;
    float3 planetCenter = float3(0, -planetRadius - camPosAbs.y, 0);
    float3 pFromCenter = localPos - planetCenter;
    float dist = length(pFromCenter);
    
    float rMin = planetRadius + layerParams.x;
    float rMax = planetRadius + layerParams.y;
    
    if (dist < rMin || dist > rMax)
        return 0.0;
    float h = saturate((dist - rMin) / max(1e-6, (rMax - rMin)));

    float weatherMapSize = shapeParams.z;
    float2 weatherUV = (worldPos.xz + weatherOffset) / weatherMapSize;
    float4 weatherData = g_WeatherMap.SampleLevel(g_Sampler, weatherUV, 0);

    float coverage = saturate(weatherData.r - shapeParams.x);
    if (coverage <= 0.001)
        return 0.0;

    float typeW = saturate(weatherData.b);
    float type = lerp(typeW, saturate(typeParams.x), saturate(shapeParams.w));

    float profile = HeightProfile(h, type);
    float baseSignal = saturate(coverage * profile);
    if (baseSignal <= 0.001)
        return 0.0;

    float detailSize = layerParams.w;
    float3 detailPos = float3(
        worldPos.x + detailOffset.x,
        worldPos.y,
        worldPos.z + detailOffset.y
    );
    
    float3 detailUV = detailPos / detailSize;
    float4 dN = g_DetailNoise.SampleLevel(g_Sampler, detailUV, 0);
    
    float erosionNoise = dN.r * 0.65 + dN.g * 0.35;
    float turbulenceForce = max(0.0, turbulenceMeters);
    float3 warp = (dN.gba * 2.0 - 1.0) * turbulenceForce;

    float shapeSize = layerParams.z;
    float3 shapePos = float3(worldPos.x + shapeOffset.x, worldPos.y, worldPos.z + shapeOffset.y);
    float anvilMask = smoothstep(0.6, 1.0, h) * typeParams.z;
    shapePos.x += anvilMask * 3000.0;
    
    float3 shapeUV = (shapePos + warp) / shapeSize;
    float4 sN = g_ShapeNoise.SampleLevel(g_Sampler, shapeUV, 0);
    float shape = sN.r;

    float billow = (sN.g * 0.55 + sN.b * 0.30 + sN.a * 0.15);
    float puffy = smoothstep(0.30, 0.85, type);

    float dens = Remap01(shape, 1.0 - baseSignal, 1.0);
    dens *= lerp(1.0, billow, 0.65 * puffy);
    dens *= coverage;

    if (dens > 0.001 && !cheap)
    {
        float erosion = 1.0 - erosionNoise;
        float edge = saturate(1.0 - dens);
        float eStr = typeParams.y * lerp(0.25, 1.0, puffy) * lerp(0.35, 1.0, edge);
        dens = Remap01(dens, eStr * erosion, 1.0);
        dens = saturate(pow(dens, lerp(1.0, 1.35, typeParams.w)));
    }

    dens *= smoothstep(0.0, 0.05, h);
    dens *= 1.0 - smoothstep(0.92, 1.0, h);

    return saturate(dens * shapeParams.y);
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    float2 ndc = input.uv * 2.0 - 1.0;
    ndc.y *= -1.0;

    // --- SPRÁVNÉ ÈTENÍ VZDÁLENOSTI Z G-BUFFERU ---
    float4 pixelData = g_PosMap.SampleLevel(g_Sampler, input.uv, 0);
    float geomDist = 1e9;
    
    // Pokud pixel není prázdná obloha (0,0,0)
    if (abs(pixelData.x) > 0.001 || abs(pixelData.y) > 0.001 || abs(pixelData.z) > 0.001)
    {
        // Spoèítá skuteènou vzdálenost stìny/objektu od kamery
        geomDist = length(pixelData.xyz - camPosAbs);
        if (geomDist > 60000.0)
            geomDist = 1e9;
    }

    float3 viewRay = camForward.xyz
        + camRight.xyz * (ndc.x * tanHalfFov * aspect)
        + camUp.xyz * (ndc.y * tanHalfFov);

    float3 rd = normalize(viewRay);
    
    // Záchrana pøesnosti: Lokální raymarching
    float3 ro = float3(0, 0, 0);

    // --- NEPRÙSTØELNÁ LOGIKA OØEZU ---
    float camHeight = planetRadius + camPosAbs.y;
    float3 planetCenter = float3(0, -camHeight, 0);
    float rMin = planetRadius + layerParams.x;
    float rMax = planetRadius + layerParams.y;
    float3 localUp = normalize(ro - planetCenter);
    float horizonFade = 1.0;

    float2 tAtm = RaySphere(ro - planetCenter, rd, rMax);
    float2 tSrf = RaySphere(ro - planetCenter, rd, rMin);

    float tStart = 0.0;
    float tEnd = 0.0;

    if (tAtm.y < 0.0)
        return float4(0, 0, 0, 0); // Díváme se úplnì do vesmíru

    if (camHeight < rMin)
    {
        float viewUp = dot(rd, localUp);
        if (viewUp <= 0.001)
            return float4(0, 0, 0, 0);
        horizonFade = smoothstep(0.001, max(0.001, horizonFadeEnd), viewUp);

        // KAMERA JE POD MRAKY (Zabranuje renderovani mraku v zemi!)
        if (tSrf.y < 0.0)
            return float4(0, 0, 0, 0); // Díváme se do zemì
        tStart = max(0.0, tSrf.y);
        tEnd = tAtm.y;
    }
    else if (camHeight > rMax)
    {
        // KAMERA JE NAD MRAKY
        if (tAtm.x < 0.0)
            return float4(0, 0, 0, 0);
        tStart = max(0.0, tAtm.x);
        tEnd = (tSrf.x > 0.0) ? tSrf.x : tAtm.y;
    }
    else
    {
        // KAMERA JE UVNITØ MRAKÙ
        tStart = 0.0;
        tEnd = (tSrf.x > 0.0) ? tSrf.x : tAtm.y;
    }

    if (tEnd <= tStart)
        return float4(0, 0, 0, 0);

    // Oøez G-Bufferem
    if (tStart >= geomDist)
        return float4(0, 0, 0, 0);
    tEnd = min(tEnd, geomDist);

    float rayLen = tEnd - tStart;
    if (rayLen <= 0.0)
        return float4(0, 0, 0, 0);

    int minSteps = 64;
    int maxSteps = 128;
    int steps = (int) clamp(rayLen / 120.0, (float) minSteps, (float) maxSteps);

    float stepSize = rayLen / (float) steps;
    float jitter = Hash12(input.pos.xy + timeSeconds * 60.0);
    float t = tStart + stepSize * jitter;

    float3 sunN = normalize(sunDir);
    float mu = dot(rd, sunN);

    float gF = saturate(lightParams.w);
    float gB = -0.25;
    float phase = lerp(HG(mu, gF), HG(mu, gB), 0.35);

    float3 acc = 0.0;
    float trans = 1.0;

    float extinction = lightParams.x;
    float powderStr = lightParams.y;
    float msStr = lightParams.z;

    float lightStep = 180.0;
    float coneSpread = 0.45;

    [loop]
    for (int i = 0; i < steps; i++)
    {
        float3 p = ro + rd * t;
        float dens = SampleDensity(p, false);

        if (dens > 0.001)
        {
            float opticalDepth = dens * stepSize * extinction;
            float stepAlpha = 1.0 - exp(-opticalDepth);

            float shadowSum = 0.0;
            [unroll]
            for (int j = 0; j < 6; j++)
            {
                float3 lp = p + sunN * lightStep * (j + 1);
                lp += NOISE_KERNEL[j] * (lightStep * (j + 1) * coneSpread);
                shadowSum += SampleDensity(lp, true);
            }
            
            float shadow = exp(-shadowSum * lightStep * extinction * 0.15);
            float powder = 1.0 - exp(-opticalDepth * powderStr);
            float ms = 1.0 + msStr * (1.0 - shadow) * powder;

            float h = saturate((length(p - planetCenter) - rMin) / max(1e-6, (rMax - rMin)));
            float3 amb = lerp(cAmbBot, cAmbTop, h) * ambInt;

            float3 sunL = cSun * sunInt * phase * shadow;
            float3 lighting = (sunL * powder * ms) + amb;

            acc += lighting * stepAlpha * trans;
            trans *= exp(-opticalDepth);

            if (trans < 0.01)
            {
                trans = 0.0;
                break;
            }
        }

        t += stepSize;
        if (t > tEnd)
            break;
    }

    return float4(acc * horizonFade, (1.0 - trans) * horizonFade);
}