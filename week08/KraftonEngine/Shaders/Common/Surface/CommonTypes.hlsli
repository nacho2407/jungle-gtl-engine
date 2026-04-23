// Shader include: Common/Surface/CommonTypes.hlsli
// Role: shared shader code or editor/material entry.
// Slots: declared locally or in included common resources.

#ifndef COMMON_TYPES_HLSLI
#define COMMON_TYPES_HLSLI

#include "../Utils/Functions.hlsl"
#include "../Resources/SystemSamplers.hlsl"
#include "../Resources/SystemResources.hlsl"
#include "../Material/StaticMeshMaterialCommon.hlsli"

struct FOpaqueVSOutput
{
    float4 position     : SV_POSITION;
    float3 worldNormal  : TEXCOORD0;
    float4 worldTangent : TEXCOORD3;
    float4 color        : COLOR0;
    float2 texcoord     : TEXCOORD1;
    float4 gouraud      : TEXCOORD2;
};

struct FOpaqueOutput2
{
    float4 BaseColor : SV_TARGET0;
    float4 Surface1  : SV_TARGET1;
};

struct FOpaqueOutput3
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

