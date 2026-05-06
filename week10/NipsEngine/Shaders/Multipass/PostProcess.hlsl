Texture2D SceneTex : register(t0);
SamplerState Samp : register(s0);

struct VSOutput
{
	float4 Pos : SV_POSITION;
};

VSOutput mainVS(uint id : SV_VertexID)
{
	float2 pos;
	if (id == 0)
		pos = float2(-1, -1);
	else if (id == 1)
		pos = float2(-1, 3);
	else
		pos = float2(3, -1);

	VSOutput o;
	o.Pos = float4(pos, 0, 1);
	return o;
}

float4 mainPS(VSOutput input) : SV_TARGET
{
	return float4(1, 0, 0, 1);
}