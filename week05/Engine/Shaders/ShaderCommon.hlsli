#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

// b0: 프레임당 1회 (카메라)
cbuffer FrameData : register(b0)
{
	float4x4 View;
	float4x4 Projection;
};

// b1: 오브젝트당
cbuffer ObjectData : register(b1)
{
	float4x4 World;
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float4 Color : COLOR;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
	float3 Normal : NORMAL;
	float2 UV : TEXCOORD0;
};

#endif
