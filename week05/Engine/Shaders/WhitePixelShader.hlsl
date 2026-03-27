#include "ShaderCommon.hlsli"

// Material 상수 버퍼 (b2)
cbuffer MaterialData : register(b2)
{
	float4 BaseColor;
};

float4 main(VS_OUTPUT Input) : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
