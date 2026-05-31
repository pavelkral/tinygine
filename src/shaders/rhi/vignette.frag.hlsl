Texture2D texMain : register(t0);
SamplerState smpMain : register(s0);

float4 PSMain(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float3 color = texMain.Sample(smpMain, uv).rgb;
    
    // calculate distance from centerb of texture to pixel position uv (0.5, 0.5)
    float dist = distance(uv, float2(0.5, 0.5));
    
    // vignette more far more dark
    float vignette = smoothstep(0.8, 0.35, dist);
    
    return float4(color * vignette, 1.0);
}