#include "ShaderCommon.hlsli"

cbuffer FOutlineConstans : register(b2)
{
	float4x4 WorldInvTranspose;
	
	float OutlineWidth;
	float3 Padding0;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	
	float3 worldPos = mul(float4(input.Position, 1.0f), World).xyz;
	
	float3 worldNormal = mul(float4(input.Normal, 0.f), WorldInvTranspose).xyz;
	worldNormal = normalize(worldNormal);
	
	worldPos += worldNormal * OutlineWidth;
	float4 viewPos = mul(float4(worldPos, 1.0f), View);
	
	output.Position = mul(viewPos, Projection);
	
	return output;
}