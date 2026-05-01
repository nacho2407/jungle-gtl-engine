struct VSInput
{
	float2 Position : POSITION;
	float4 Color : COLOR;
	float2 TexCoord : TEXCOORD;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
	float2 TexCoord : TEXCOORD;
};

cbuffer UIRenderCB : register(b0)
{
	float2 ViewportSize;
	float2 Translation;
};

Texture2D UITexture : register(t0);
SamplerState UISampler : register(s0);

VSOutput VS(VSInput Input)
{
	VSOutput Output;
	float2 PixelPosition = Input.Position + Translation;
	float2 NdcPosition = float2(
		(PixelPosition.x / ViewportSize.x) * 2.0f - 1.0f,
		1.0f - (PixelPosition.y / ViewportSize.y) * 2.0f
	);
	Output.Position = float4(NdcPosition, 0.0f, 1.0f);
	Output.Color = Input.Color;
	Output.TexCoord = Input.TexCoord;
	return Output;
}

float4 PS(VSOutput Input) : SV_Target
{
	float4 TextureColor = UITexture.Sample(UISampler, Input.TexCoord);
	return Input.Color * TextureColor;
}
