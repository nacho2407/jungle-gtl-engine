#ifndef COMMON_TYPES_HLSLI
#define COMMON_TYPES_HLSLI

#include "Common/Functions.hlsl"
#include "Common/SystemSamplers.hlsl"
#include "Common/SystemResources.hlsl"

cbuffer PerShader1 : register(b2)
{
    float4 SectionColor;
}

float4 GetSectionColorOrWhite()
{
    float Magnitude = abs(SectionColor.x) + abs(SectionColor.y) + abs(SectionColor.z) + abs(SectionColor.w);
    return (Magnitude < 0.0001f) ? float4(1.0f, 1.0f, 1.0f, 1.0f) : SectionColor;
}

float4 SampleBaseTexture(Texture2D TextureRef, float2 UV)
{
    float4 Sampled = TextureRef.Sample(LinearWrapSampler, UV);
    float Magnitude = abs(Sampled.x) + abs(Sampled.y) + abs(Sampled.z) + abs(Sampled.w);
    return (Magnitude < 0.0001f) ? float4(1.0f, 1.0f, 1.0f, 1.0f) : Sampled;
}

struct FBaseDrawVSOutput
{
    float4 position     : SV_POSITION;
    float3 worldNormal  : TEXCOORD0;
    float4 worldTangent : TEXCOORD3;
    float4 color        : COLOR0;
    float2 texcoord     : TEXCOORD1;
    float4 gouraud      : TEXCOORD2;
};

struct FBaseDrawOutput2
{
    float4 BaseColor : SV_TARGET0;
    float4 Surface1  : SV_TARGET1;
};

struct FBaseDrawOutput3
{
    float4 BaseColor : SV_TARGET0;
    float4 Surface1  : SV_TARGET1;
    float4 Surface2  : SV_TARGET2;
};

struct FDecalOutput2
{
    float4 ModifiedBaseColor : SV_TARGET0;
    float4 ModifiedSurface1  : SV_TARGET1;
};

struct FDecalOutput3
{
    float4 ModifiedBaseColor : SV_TARGET0;
    float4 ModifiedSurface1  : SV_TARGET1;
    float4 ModifiedSurface2  : SV_TARGET2;
};

#endif
