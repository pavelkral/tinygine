Texture2D texMain : register(t0);
SamplerState smpMain : register(s0);

float4 PSMain(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float3 color = texMain.Sample(smpMain, uv).rgb;
    
    // Extract only the bright areas (threshold)
    float brightness = max(color.r, max(color.g, color.b));
    float3 bloom = float3(0.0, 0.0, 0.0);
    
    if (brightness > 1.0)
    {
        // Simple "Box Blur" for neighboring pixels
        float2 texOffset = 1.0 / float2(1920.0, 1080.0) * 3.0; // Bloom strength
        bloom += texMain.Sample(smpMain, uv + float2(-texOffset.x, -texOffset.y)).rgb;
        bloom += texMain.Sample(smpMain, uv + float2(texOffset.x, -texOffset.y)).rgb;
        bloom += texMain.Sample(smpMain, uv + float2(-texOffset.x, texOffset.y)).rgb;
        bloom += texMain.Sample(smpMain, uv + float2(texOffset.x, texOffset.y)).rgb;
        bloom *= 0.25;
    }
    
    // original color + bloom effect
    return float4(color + bloom, 1.0);
}