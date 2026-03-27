#include "ShaderCommon.hlsli"

// Material 상수 버퍼 (b2) — VS에서 사용
cbuffer MaterialData : register(b2)
{
	float4 ColorTint;
};

VS_OUTPUT main(VS_INPUT Input)
{
	VS_OUTPUT Output;
	float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
	float4 ViewPos = mul(WorldPos, View);
	Output.Position = mul(ViewPos, Projection);
	Output.Color = Input.Color * ColorTint;
	Output.Normal = mul(Input.Normal, (float3x3) World);
	Output.UV = Input.UV;
	return Output;
}
