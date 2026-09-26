#include "ShaderCommon.hlsli"

VS_OUTPUT main(VS_INPUT Input)
{
	VS_OUTPUT Output;
	float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
	float4 ViewPos = mul(WorldPos, View);
	float4 ClipPos = mul(ViewPos, Projection);
	ClipPos.z = ClipPos.w * 0.9999f;

	Output.Position = ClipPos; 
	Output.Color = Input.Color;
	Output.Normal = mul(Input.Normal, (float3x3) World);
	return Output;
}
