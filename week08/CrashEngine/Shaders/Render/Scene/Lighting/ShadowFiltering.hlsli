/*
    ShadowFiltering.hlsli: shared shadow filtering helpers for direct lighting.
*/

#ifndef SHADOW_FILTERING_HLSLI
#define SHADOW_FILTERING_HLSLI

#include "../../../Resources/SystemSamplers.hlsl"

#define SHADOW_FILTER_METHOD_PCF 0
#define SHADOW_FILTER_METHOD_VSM 1
#define SHADOW_FILTER_METHOD_PCSS 2

#ifndef SHADOW_FILTER_METHOD
#define SHADOW_FILTER_METHOD SHADOW_FILTER_METHOD_PCF
#endif

Texture2D g_ShadowMap2D0 : register(t20);
Texture2D g_ShadowMap2D1 : register(t21);
Texture2D g_ShadowMap2D2 : register(t22);
Texture2D g_ShadowMap2D3 : register(t23);
Texture2D g_ShadowMap2D4 : register(t24);

TextureCube g_ShadowMapCube0 : register(t25);
TextureCube g_ShadowMapCube1 : register(t26);
TextureCube g_ShadowMapCube2 : register(t27);
TextureCube g_ShadowMapCube3 : register(t28);
TextureCube g_ShadowMapCube4 : register(t29);

static const float2 kShadowTexelSize = float2(1.0f / 2048.0f, 1.0f / 2048.0f);

float SampleSpotShadowCmp(int ShadowIndex, float2 ShadowUV, float CompareDepth)
{
    float ShadowFactor = 1.0f;
    [branch]
    switch (ShadowIndex)
    {
    case 0: ShadowFactor = g_ShadowMap2D0.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 1: ShadowFactor = g_ShadowMap2D1.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 2: ShadowFactor = g_ShadowMap2D2.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 3: ShadowFactor = g_ShadowMap2D3.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 4: ShadowFactor = g_ShadowMap2D4.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    }

    return ShadowFactor;
}

float OffsetLookupSpotPCF(int ShadowIndex, float2 BaseNDC, float CompareDepth, float2 Offset)
{
    float2 BaseUV = BaseNDC * 0.5f + 0.5f;
    BaseUV.y = 1.0f - BaseUV.y;

    float2 OffsetUV = BaseUV;
    OffsetUV.x += Offset.x * kShadowTexelSize.x;
    OffsetUV.y += Offset.y * kShadowTexelSize.y;

    if (OffsetUV.x < 0.0f || OffsetUV.x > 1.0f || OffsetUV.y < 0.0f || OffsetUV.y > 1.0f)
    {
        return 1.0f;
    }

    return SampleSpotShadowCmp(ShadowIndex, OffsetUV, CompareDepth);
}

float PCF_NvidiaOptimizedSpot(int ShadowIndex, float2 BaseNDC, float CompareDepth, float4 PixelPos)
{
    float2 Offset = (float2)(frac(PixelPos.xy * 0.5f) > 0.25f);
    Offset.y += Offset.x;

    if (Offset.y > 1.1f)
    {
        Offset.y = 0.0f;
    }

    float ShadowCoeff = 0.0f;
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, BaseNDC, CompareDepth, Offset + float2(-1.5f, 0.5f));
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, BaseNDC, CompareDepth, Offset + float2(0.5f, 0.5f));
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, BaseNDC, CompareDepth, Offset + float2(-1.5f, -1.5f));
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, BaseNDC, CompareDepth, Offset + float2(0.5f, -1.5f));

    return ShadowCoeff * 0.25f;
}



float SamplePointShadowCmp(int ShadowIndex, float3 SampleDir, float CompareDepth)
{
    float ShadowFactor = 1.0f;
    [branch]
    switch (ShadowIndex)
    {
    case 0: ShadowFactor = g_ShadowMapCube0.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
    case 1: ShadowFactor = g_ShadowMapCube1.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
    case 2: ShadowFactor = g_ShadowMapCube2.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
    case 3: ShadowFactor = g_ShadowMapCube3.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
    case 4: ShadowFactor = g_ShadowMapCube4.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
    }

    return ShadowFactor;
}

float OffsetLookupPointPCF(int ShadowIndex, float3 SampleDir, float3 Tangent, float3 Bitangent, float CompareDepth, float2 Offset)
{
    float2 OffsetUV = Offset * kShadowTexelSize * 2.0f;
    float3 OffsetDir = normalize(SampleDir + Tangent * OffsetUV.x + Bitangent * OffsetUV.y);

    return SamplePointShadowCmp(ShadowIndex, OffsetDir, CompareDepth);
}

float PCF_NvidiaOptimizedPoint(int ShadowIndex, float3 SampleDir, float CompareDepth, float4 PixelPos)
{
    float2 Offset = (float2)(frac(PixelPos.xy * 0.5f) > 0.25f);
    Offset.y += Offset.x;
    if (Offset.y > 1.1f)
    {
        Offset.y = 0.0f;
    }

    float3 Up = (abs(SampleDir.z) < 0.999f) ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    float3 Tangent = normalize(cross(Up, SampleDir));
    float3 Bitangent = cross(SampleDir, Tangent);

    float ShadowCoeff = 0.0f;
    ShadowCoeff += OffsetLookupPointPCF(ShadowIndex, SampleDir, Tangent, Bitangent, CompareDepth, Offset + float2(-1.5f, 0.5f));
    ShadowCoeff += OffsetLookupPointPCF(ShadowIndex, SampleDir, Tangent, Bitangent, CompareDepth, Offset + float2(0.5f, 0.5f));
    ShadowCoeff += OffsetLookupPointPCF(ShadowIndex, SampleDir, Tangent, Bitangent, CompareDepth, Offset + float2(-1.5f, -1.5f));
    ShadowCoeff += OffsetLookupPointPCF(ShadowIndex, SampleDir, Tangent, Bitangent, CompareDepth, Offset + float2(0.5f, -1.5f));

    return ShadowCoeff * 0.25f;
}

float FilterSpotShadow(int ShadowIndex, float2 ShadowVector, float CompareDepth, float4 PixelPos)
{
    float2 BaseNDC = ShadowVector;

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_PCF
    return PCF_NvidiaOptimizedSpot(ShadowIndex, BaseNDC, CompareDepth, PixelPos);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_VSM
    // TODO: add VSM implementation.
    return PCF_NvidiaOptimizedSpot(ShadowIndex, BaseNDC, CompareDepth, PixelPos);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_PCSS
    // TODO: add PCSS implementation.
    return PCF_NvidiaOptimizedSpot(ShadowIndex, BaseNDC, CompareDepth, PixelPos);
#else
    float2 BaseUV = BaseNDC * 0.5f + 0.5f;
    BaseUV.y = 1.0f - BaseUV.y;
    return SampleSpotShadowCmp(ShadowIndex, BaseUV, CompareDepth);
#endif
}

float FilterPointShadow(int ShadowIndex, float3 ShadowVector, float CompareDepth, float4 PixelPos)
{
    float3 SampleDir = ShadowVector;

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_PCF
    return PCF_NvidiaOptimizedPoint(ShadowIndex, SampleDir, CompareDepth, PixelPos);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_VSM
    // TODO: add VSM implementation.
    return PCF_NvidiaOptimizedPoint(ShadowIndex, SampleDir, CompareDepth, PixelPos);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_PCSS
    // TODO: add PCSS implementation.
    return PCF_NvidiaOptimizedPoint(ShadowIndex, SampleDir, CompareDepth, PixelPos);
#else
    return SamplePointShadowCmp(ShadowIndex, SampleDir, CompareDepth);
#endif
}

#endif
