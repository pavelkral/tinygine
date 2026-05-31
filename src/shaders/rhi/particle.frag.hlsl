Texture2D texSprite : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};


struct PixelOutput
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
};


PixelOutput PSMain(PS_IN input)
{
    float2 centerOffset = input.uv - 0.5;
    float dist = length(centerOffset) * 2.0;
    float angle = atan2(centerOffset.y, centerOffset.x);
    
    float noise = sin(angle * 5.0) * 0.1 + sin(angle * 13.0) * 0.05;
    dist += noise;
    
    float alpha = saturate(1.0 - dist);
    alpha = pow(alpha, 2.5);
    
    float4 finalColor = input.color * float4(1.0, 1.0, 1.0, alpha);

    // output mrt for particles, we only care about color, normal and world position can be null
    PixelOutput output;
    output.Color = finalColor;
    output.Normal = float4(0, 0, 0, 0); // null normal for particles 
    output.WorldPos = float4(0, 0, 0, 0);
    
    return output;
}