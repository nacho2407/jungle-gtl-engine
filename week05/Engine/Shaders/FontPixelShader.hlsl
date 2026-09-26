#include "ShaderCommon.hlsli"

Texture2D FontTexture : register(t0);
SamplerState FontSampler : register(s0);

cbuffer TextData : register(b2)
{
	float4 TextColor;
};

float4 main(VS_OUTPUT Input) : SV_TARGET
{
	float4 SampledColor = FontTexture.Sample(FontSampler, Input.UV);

	float Alpha = SampledColor.r;
	clip(Alpha - 0.01f); // 알파가 거의 0인 픽셀은 버림. -> 글자 바깥

	return float4(TextColor.rgb, TextColor.a * Alpha); // 현재 픽셀이 글자 모양 안에 얼마나 속하는지 곱해서 최종 투명도	계산
}