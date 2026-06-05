RWTexture3D<unorm float4> Result : register(u0);

cbuffer Params : register(b0)
{
    uint baseFreq;
    uint seed;
    uint pad0;
    uint pad1;
}

static const float INV_UINT = 1.0 / 4294967296.0;
static const float SQRT3 = 1.73205080757;

uint3 pcg3d(uint3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= (v >> 16);
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v;
}

float3 rand3(uint3 p, uint s)
{
    uint3 r = pcg3d(p + uint3(s, s * 101u, s * 10007u));
    return float3(r) * INV_UINT;
}

float Remap01(float v, float a, float b)
{
    return saturate((v - a) / max(1e-6, (b - a)));
}

float3 grad3(uint3 p, uint s)
{
    float3 g = rand3(p, s) * 2.0 - 1.0;
    return normalize(g);
}

float perlinCorner(int3 i, float3 f, int3 o, uint freq, uint s)
{
    int3 c = i + o;
    uint3 w = (uint3) ((c % (int) freq + (int) freq) % (int) freq);
    return dot(grad3(w, s), f - float3(o));
}

float TileablePerlin(float3 uv, uint freq, uint s)
{
    float3 p = uv * (float) freq;
    int3 i = (int3) floor(p);
    float3 f = frac(p);
    float3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float n000 = perlinCorner(i, f, int3(0, 0, 0), freq, s);
    float n100 = perlinCorner(i, f, int3(1, 0, 0), freq, s);
    float n010 = perlinCorner(i, f, int3(0, 1, 0), freq, s);
    float n110 = perlinCorner(i, f, int3(1, 1, 0), freq, s);
    float n001 = perlinCorner(i, f, int3(0, 0, 1), freq, s);
    float n101 = perlinCorner(i, f, int3(1, 0, 1), freq, s);
    float n011 = perlinCorner(i, f, int3(0, 1, 1), freq, s);
    float n111 = perlinCorner(i, f, int3(1, 1, 1), freq, s);

    float x00 = lerp(n000, n100, u.x);
    float x10 = lerp(n010, n110, u.x);
    float x01 = lerp(n001, n101, u.x);
    float x11 = lerp(n011, n111, u.x);
    float y0 = lerp(x00, x10, u.y);
    float y1 = lerp(x01, x11, u.y);
    float n = lerp(y0, y1, u.z);
    return saturate(n * 0.5 + 0.5);
}

float TileableWorley(float3 uv, uint freq, uint s)
{
    float3 p = uv * (float) freq;
    int3 i = (int3) floor(p);
    float3 f = frac(p);
    float minD = 1e9;

    [unroll]
    for (int z = -1; z <= 1; z++)
    [unroll]
        for (int y = -1; y <= 1; y++)
    [unroll]
            for (int x = -1; x <= 1; x++)
            {
                int3 n = int3(x, y, z);
                int3 c = i + n;
                uint3 w = (uint3) ((c % (int) freq + (int) freq) % (int) freq);
                float3 r = rand3(w, s);
                float3 d = float3(n) + r - f;
                float dist = dot(d, d);
                minD = min(minD, dist);
            }

    float d1 = sqrt(minD) / SQRT3;
    return saturate(1.0 - d1);
}

float4 MakeShape(float3 uv, uint f, uint s)
{
    float p = TileablePerlin(uv, f, s);
    float w0 = TileableWorley(uv, f, s + 17u);
    float w1 = TileableWorley(uv, f * 2, s + 29u);
    float w2 = TileableWorley(uv, f * 4, s + 43u);

    float worleyFBM = w0 * 0.625 + w1 * 0.25 + w2 * 0.125;
    float perlinWorley = Remap01(p, 1.0 - worleyFBM, 1.0);
    perlinWorley = saturate(pow(perlinWorley, 1.25));

    return float4(perlinWorley, w0, w1, w2);
}

[numthreads(8, 8, 8)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint w, h, d;
    Result.GetDimensions(w, h, d);
    if (id.x >= w || id.y >= h || id.z >= d)
        return;

    float3 denom = max(float3((float) (w - 1), (float) (h - 1), (float) (d - 1)), 1.0);
    float3 uv = float3(id) / denom;

    uint f = max(1u, baseFreq);
    Result[id] = MakeShape(uv, f, seed);
}