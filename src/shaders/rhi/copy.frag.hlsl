Texture2D texMain : register(t0);
SamplerState smpMain : register(s0);

float4 PSMain(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    return texMain.Sample(smpMain, uv);
}