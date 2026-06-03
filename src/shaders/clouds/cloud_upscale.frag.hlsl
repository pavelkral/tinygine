Texture2D<float4> tHalfResClouds : register(t0);
Texture2D<float> tDepth : register(t1);
SamplerState sLinear : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float Luma(float3 c)
{
    return dot(c, float3(0.299, 0.587, 0.114));
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    float w, h;
    tHalfResClouds.GetDimensions(w, h);
    float2 texel = 1.0 / float2(w, h);

    float4 c0 = tHalfResClouds.SampleLevel(sLinear, input.uv, 0);
    float a0 = c0.a;
    float l0 = Luma(c0.rgb);

    float4 sum = c0;
    float wsum = 1.0;

    const float kA = 18.0;
    const float kL = 6.0;

    float2 offs[4] =
    {
        float2(1, 0), float2(-1, 0),
        float2(0, 1), float2(0, -1)
    };

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float2 uv = input.uv + offs[i] * texel;
        float4 s = tHalfResClouds.SampleLevel(sLinear, uv, 0);

        float wa = 1.0 / (1.0 + kA * abs(s.a - a0));
        float wl = 1.0 / (1.0 + kL * abs(Luma(s.rgb) - l0));
        float wgt = wa * wl;

        sum += s * wgt;
        wsum += wgt;
    }

    return sum / wsum;
}