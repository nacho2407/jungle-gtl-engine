#include "ShaderCommon.hlsli"

float4 main(VS_OUTPUT Input) : SV_TARGET
{
	float3 FinalColor = Input.Color.rgb;
	return float4(FinalColor, 1.0f);
}
