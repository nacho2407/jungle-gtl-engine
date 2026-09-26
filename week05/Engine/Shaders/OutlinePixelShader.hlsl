#include "ShaderCommon.hlsli"

float4 main(VS_OUTPUT Input) : SV_TARGET
{
	return float4(1.0f, 0.5f, 0.0f, 1.0f); // 주황색 아웃라인
}
