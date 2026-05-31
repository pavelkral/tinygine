struct PS_INPUT
{
    float4 sv_pos : SV_POSITION;
    float3 color : COLOR;
};

// mrt struct for outputting to multiple render targets (color, normal, world position)
struct PixelOutput
{
    float4 Color : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
};

// change the pixel shader to output to multiple render targets (MRT)
PixelOutput PSMain(PS_INPUT input)
{
    PixelOutput output;
    output.Color = float4(input.color, 1.0f);
    output.Normal = float4(0, 0, 0, 0);
    output.WorldPos = float4(0, 0, 0, 0);
    
    return output;
}