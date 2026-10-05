#include "../Common/Common.hlsli"

Texture2D DiffuseMap : register(t0);
SamplerState SampleState : register(s0);

cbuffer ParticleMaterialBuffer : register(b2)
{
	float Opacity;
	float3 ParticleMaterialPadding;
};

struct VSInput
{
	float3 QuadPosition   : POSITION0;
	float2 QuadUV         : TEXCOORD0;

	float3 Start          : POSITION1;
	float  HalfWidthStart : TEXCOORD1;
	float3 End            : POSITION2;
	float  HalfWidthEnd   : TEXCOORD2;
	float4 StartColor     : COLOR0;
	float4 EndColor       : COLOR1;
	float2 UVStartEnd     : TEXCOORD3;
	float3 StartSide      : TEXCOORD4;
	float3 EndSide        : TEXCOORD5;
};

struct PSInput
{
	float4 Position : SV_POSITION;
	float2 UV       : TEXCOORD0;
	float4 Color    : COLOR0;
};

float3 SafeNormalize(float3 V, float3 Fallback)
{
	float LenSq = dot(V, V);
	return LenSq > 1.0e-8f ? V * rsqrt(LenSq) : Fallback;
}

PSInput VS(VSInput Input)
{
	PSInput Output;

	float T = saturate(Input.QuadUV.x);
	float SideSign = lerp(-1.0f, 1.0f, saturate(Input.QuadUV.y));

	float3 Center = lerp(Input.Start, Input.End, T);
	float HalfWidth = lerp(Input.HalfWidthStart, Input.HalfWidthEnd, T);
	float3 Side = SafeNormalize(lerp(Input.StartSide, Input.EndSide, T), float3(0.0f, 1.0f, 0.0f));

	float3 WorldPosition = Center + Side * HalfWidth * SideSign;
	float4 ViewPosition = mul(float4(WorldPosition, 1.0f), View);
	Output.Position = mul(ViewPosition, Projection);
	Output.UV = float2(lerp(Input.UVStartEnd.x, Input.UVStartEnd.y, T), saturate(Input.QuadUV.y));
	Output.Color = lerp(Input.StartColor, Input.EndColor, T);
	return Output;
}

float4 PS(PSInput Input) : SV_TARGET
{
	float4 TexColor = DiffuseMap.Sample(SampleState, Input.UV);
	float4 Color = TexColor * Input.Color;
	Color.a *= Opacity;

	if (Color.a <= 0.001f)
	{
		discard;
	}

	if (bIsWireframe > 0.5f)
	{
		return float4(WireframeRGB, Color.a);
	}

	return Color;
}
