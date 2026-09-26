cbuffer OutlineData : register(b0)
{
	float4 OutlineColor;
	float OutlineThickness;
	float OutlineThreshold;
	float2 Padding;
};

struct VSOutput
{
	float4 Position : SV_Position;
	float2 UV : TEXCOORD0;
};

Texture2D MaskTexture : register(t0);
SamplerState MaskSampler : register(s0);

static const float XFilter[9] =
{
	-1.0f, 0.0f, 1.0f,
	-2.0f, 0.0f, 2.0f,
	-1.0f, 0.0f, 1.0f
};

static const float YFilter[9] =
{
	1.0f, 2.0f, 1.0f,
	0.0f, 0.0f, 0.0f,
	-1.0f, -2.0f, -1.0f
};

float SampleMask(float2 UV)
{
	return MaskTexture.Sample(MaskSampler, UV).r;
}

float4 main(VSOutput Input) : SV_Target
{
	float Width = 1.0f;
	float Height = 1.0f;
	MaskTexture.GetDimensions(Width, Height);

	float2 OffsetX = float2(OutlineThickness / Width, 0.0f);
	float2 OffsetY = float2(0.0f, OutlineThickness / Height);

	float Grid[9];
	Grid[0] = SampleMask(Input.UV - OffsetX + OffsetY);
	Grid[1] = SampleMask(Input.UV + OffsetY);
	Grid[2] = SampleMask(Input.UV + OffsetX + OffsetY);
	Grid[3] = SampleMask(Input.UV - OffsetX);
	Grid[4] = SampleMask(Input.UV);
	Grid[5] = SampleMask(Input.UV + OffsetX);
	Grid[6] = SampleMask(Input.UV - OffsetX - OffsetY);
	Grid[7] = SampleMask(Input.UV - OffsetY);
	Grid[8] = SampleMask(Input.UV + OffsetX - OffsetY);

	float Sx = 0.0f;
	float Sy = 0.0f;
	[unroll]
	for (int Index = 0; Index < 9; ++Index)
	{
		Sx += Grid[Index] * XFilter[Index];
		Sy += Grid[Index] * YFilter[Index];
	}

	float Dist = sqrt(Sx * Sx + Sy * Sy);
	float Edge = Dist > OutlineThreshold ? 1.0f : 0.0f;
	return float4(OutlineColor.rgb, OutlineColor.a * Edge);
}
