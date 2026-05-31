Texture2D texSprite : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 PSMain(PS_IN input) : SV_TARGET
{
    // Pøeèteme texturu kouøe/ohnì
    float4 texColor = texSprite.Sample(g_Sampler, input.uv);
    
    // Vynásobíme barvu textury naší barvou z Compute Shaderu
    return texColor * input.color;
}