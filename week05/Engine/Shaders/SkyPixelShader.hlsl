#include "ShaderCommon.hlsli"

cbuffer SkyData : register(b2)
{
	float4 SkyColor; 
	float4 HorizonColor; 
	float4 GroundColor; 
};

float4 main(VS_OUTPUT Input) : SV_TARGET
{
    // normalize — Scale=2000이 Normal에 곱해지므로 반드시 필요
	float3 Normal = normalize(Input.Normal);

    // 부호 반전 — 안쪽 노멀이라 위쪽이 -Z, 부호 뒤집어야 위=하늘
	float Top = -Normal.z;

	float4 Color;
	if (Top > 0.0f)
		Color = lerp(HorizonColor, SkyColor, Top); // 위쪽 → 하늘
	else
		Color = lerp(HorizonColor, GroundColor, -Top); // 아래쪽 → 땅

	return float4(Color.rgb, 1.0f);
}
