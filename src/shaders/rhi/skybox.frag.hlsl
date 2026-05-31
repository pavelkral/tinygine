TextureCube skyboxTex : register(t0);
SamplerState samp : register(s0);

struct PS_INPUT
{
    float4 sv_pos : SV_POSITION;
    float3 localPos : POSITION; 
};


struct PixelOutput
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
};


PixelOutput PSMain(PS_INPUT input)
{
  
    float3 envColor = skyboxTex.Sample(samp, normalize(input.localPos)).rgb;
    

    float exposure = 0.5f;
    envColor *= exposure;
    
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    envColor = clamp((envColor * (a * envColor + b)) / (envColor * (c * envColor + d) + e), 0.0f, 1.0f);
    
    envColor = pow(envColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    PixelOutput output;
    output.Color = float4(envColor, 1.0f);
    output.Normal = float4(0, 0, 0, 0); //null normal = ignored in SSR
    output.WorldPos = float4(0, 0, 0, 0); // null position = ignored inv SSR
    
    return output;
}