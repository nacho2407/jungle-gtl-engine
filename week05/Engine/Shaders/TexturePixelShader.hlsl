#include "ShaderCommon.hlsli"

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

// Material 상수 버퍼 (b2)
cbuffer MaterialData : register(b2)
{
	// TODO: 틴트 적용 추가하기
	float4 BaseColor;
};

float4 main(VS_OUTPUT Input) : SV_TARGET
{
	float4 Color = Texture.Sample(Sampler, Input.UV);
	return Color;
}
