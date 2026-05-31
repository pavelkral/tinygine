Texture2D texSSAO : register(t0);
SamplerState samp : register(s0);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    float result = 0.0;
    float2 texelSize = 1.0 / float2(1920.0, 1080.0);
    
    float count = 0.0;
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            float2 offset = float2(float(x), float(y)) * texelSize;
            result += texSSAO.SampleLevel(samp, input.uv + offset, 0).r;
            count += 1.0;
        }
    }
    
    result = result / count;
    return float4(result, result, result, 1.0);
}