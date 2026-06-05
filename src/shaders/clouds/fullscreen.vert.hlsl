struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

static const float2 positions[3] = { float2(-1.0, 1.0), float2(3.0, 1.0), float2(-1.0, -3.0) };
static const float2 uvs[3] = { float2(0.0, 0.0), float2(2.0, 0.0), float2(0.0, 2.0) };

PS_INPUT VSMain(uint id : SV_VertexID)
{
	PS_INPUT o;
	o.pos = float4(positions[id], 0.0, 1.0);
	o.uv = uvs[id];
	return o;
}