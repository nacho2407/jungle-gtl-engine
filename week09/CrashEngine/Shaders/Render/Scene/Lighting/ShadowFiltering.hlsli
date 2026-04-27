/*
    ShadowFiltering.hlsli: shared shadow filtering helpers for direct lighting.
*/

#ifndef SHADOW_FILTERING_HLSLI
#define SHADOW_FILTERING_HLSLI

#include "../../../Resources/SystemSamplers.hlsl"

#define SHADOW_FILTER_METHOD_PCF 0
#define SHADOW_FILTER_METHOD_VSM 1
#define SHADOW_FILTER_METHOD_ESM 2

#ifndef SHADOW_FILTER_METHOD
#define SHADOW_FILTER_METHOD SHADOW_FILTER_METHOD_PCF
#endif

#ifndef SHADOW_ESM_EXPONENT
#define SHADOW_ESM_EXPONENT 80.0f
#endif

// Pixel shaders use implicit derivatives so generated moment mips are selected
// by the sampler. Vertex shaders cannot use derivative-based sampling.
#ifndef SHADOW_MOMENT_MIP_BIAS
#define SHADOW_MOMENT_MIP_BIAS 0.0f
#endif

#ifndef SHADOW_MOMENT_MAX_MIP_LEVEL
#define SHADOW_MOMENT_MAX_MIP_LEVEL 1.0f
#endif

#ifndef SHADOW_MOMENT_VERTEX_LOD
#ifdef SHADOW_MOMENT_MIP_LEVEL
#define SHADOW_MOMENT_VERTEX_LOD SHADOW_MOMENT_MIP_LEVEL
#else
#define SHADOW_MOMENT_VERTEX_LOD 0.0f
#endif
#endif

#if defined(SHADER_STAGE_PIXEL)
#define SHADOW_MOMENT_LOD_2D(Texture, UV) clamp(Texture.CalculateLevelOfDetail(LinearClampSampler, UV) + SHADOW_MOMENT_MIP_BIAS, 0.0f, SHADOW_MOMENT_MAX_MIP_LEVEL)
#define SHADOW_MOMENT_LOD_CUBE(Texture, Dir) clamp(Texture.CalculateLevelOfDetail(LinearClampSampler, Dir) + SHADOW_MOMENT_MIP_BIAS, 0.0f, SHADOW_MOMENT_MAX_MIP_LEVEL)
#define SAMPLE_SHADOW_MOMENT_2D(Texture, UV) Texture.SampleLevel(LinearClampSampler, UV, SHADOW_MOMENT_LOD_2D(Texture, UV)).rg
#define SAMPLE_SHADOW_MOMENT_CUBE(Texture, Dir) Texture.SampleLevel(LinearClampSampler, Dir, SHADOW_MOMENT_LOD_CUBE(Texture, Dir)).rg
#else
#define SAMPLE_SHADOW_MOMENT_2D(Texture, UV) Texture.SampleLevel(LinearClampSampler, UV, SHADOW_MOMENT_VERTEX_LOD).rg
#define SAMPLE_SHADOW_MOMENT_CUBE(Texture, Dir) Texture.SampleLevel(LinearClampSampler, Dir, SHADOW_MOMENT_VERTEX_LOD).rg
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

Texture2D<float2> g_ShadowMoment2D0 : register(t48);
Texture2D<float2> g_ShadowMoment2D1 : register(t49);
Texture2D<float2> g_ShadowMoment2D2 : register(t50);
Texture2D<float2> g_ShadowMoment2D3 : register(t51);
Texture2D<float2> g_ShadowMoment2D4 : register(t52);

TextureCube<float2> g_ShadowMomentCube0 : register(t53);
TextureCube<float2> g_ShadowMomentCube1 : register(t54);
TextureCube<float2> g_ShadowMomentCube2 : register(t55);
TextureCube<float2> g_ShadowMomentCube3 : register(t56);
TextureCube<float2> g_ShadowMomentCube4 : register(t57);

float2 ResolveShadowTexelSize(float2 ShadowTexelSize)
{
    if (ShadowTexelSize.x > 0.0f && ShadowTexelSize.y > 0.0f)
    {
        return ShadowTexelSize;
    }

    return float2(1.0f / 2048.0f, 1.0f / 2048.0f);
}

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

float2 SampleSpotShadowMoment(int ShadowIndex, float2 ShadowUV)
{
    float2 Moments = float2(0.0f, 0.0f);
    [branch]
    switch (ShadowIndex)
    {
    case 0: Moments = SAMPLE_SHADOW_MOMENT_2D(g_ShadowMoment2D0, ShadowUV); break;
    case 1: Moments = SAMPLE_SHADOW_MOMENT_2D(g_ShadowMoment2D1, ShadowUV); break;
    case 2: Moments = SAMPLE_SHADOW_MOMENT_2D(g_ShadowMoment2D2, ShadowUV); break;
    case 3: Moments = SAMPLE_SHADOW_MOMENT_2D(g_ShadowMoment2D3, ShadowUV); break;
    case 4: Moments = SAMPLE_SHADOW_MOMENT_2D(g_ShadowMoment2D4, ShadowUV); break;
    }

    return Moments;
}

float OffsetLookupSpotPCF(int ShadowIndex, float2 BaseNDC, float CompareDepth, float2 Offset, float2 ShadowTexelSize)
{
    float2 BaseUV = BaseNDC * 0.5f + 0.5f;
    BaseUV.y = 1.0f - BaseUV.y;

    float2 OffsetUV = BaseUV;
    OffsetUV.x += Offset.x * ShadowTexelSize.x;
    OffsetUV.y += Offset.y * ShadowTexelSize.y;

    if (OffsetUV.x < 0.0f || OffsetUV.x > 1.0f || OffsetUV.y < 0.0f || OffsetUV.y > 1.0f)
    {
        return 1.0f;
    }

    return SampleSpotShadowCmp(ShadowIndex, OffsetUV, CompareDepth);
}

float PCF_NvidiaOptimizedSpot(int ShadowIndex, float2 BaseNDC, float CompareDepth, float4 PixelPos, float2 ShadowTexelSize)
{
    float2 Offset = (float2)(frac(PixelPos.xy * 0.5f) > 0.25f);
    Offset.y += Offset.x;

    if (Offset.y > 1.1f)
    {
        Offset.y = 0.0f;
    }

    float ShadowCoeff = 0.0f;
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, BaseNDC, CompareDepth, Offset + float2(-1.5f, 0.5f), ShadowTexelSize);
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, BaseNDC, CompareDepth, Offset + float2(0.5f, 0.5f), ShadowTexelSize);
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, BaseNDC, CompareDepth, Offset + float2(-1.5f, -1.5f), ShadowTexelSize);
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, BaseNDC, CompareDepth, Offset + float2(0.5f, -1.5f), ShadowTexelSize);

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

float2 SamplePointShadowMoment(int ShadowIndex, float3 SampleDir)
{
    float2 Moments = float2(0.0f, 0.0f);
    [branch]
    switch (ShadowIndex)
    {
    case 0: Moments = SAMPLE_SHADOW_MOMENT_CUBE(g_ShadowMomentCube0, SampleDir); break;
    case 1: Moments = SAMPLE_SHADOW_MOMENT_CUBE(g_ShadowMomentCube1, SampleDir); break;
    case 2: Moments = SAMPLE_SHADOW_MOMENT_CUBE(g_ShadowMomentCube2, SampleDir); break;
    case 3: Moments = SAMPLE_SHADOW_MOMENT_CUBE(g_ShadowMomentCube3, SampleDir); break;
    case 4: Moments = SAMPLE_SHADOW_MOMENT_CUBE(g_ShadowMomentCube4, SampleDir); break;
    }

    return Moments;
}

float OffsetLookupPointPCF(int ShadowIndex, float3 SampleDir, float3 Tangent, float3 Bitangent, float CompareDepth, float2 Offset, float2 ShadowTexelSize)
{
    float2 OffsetUV = Offset * ShadowTexelSize * 2.0f;
    float3 OffsetDir = normalize(SampleDir + Tangent * OffsetUV.x + Bitangent * OffsetUV.y);

    return SamplePointShadowCmp(ShadowIndex, OffsetDir, CompareDepth);
}

float PCF_NvidiaOptimizedPoint(int ShadowIndex, float3 SampleDir, float CompareDepth, float4 PixelPos, float2 ShadowTexelSize)
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
    ShadowCoeff += OffsetLookupPointPCF(ShadowIndex, SampleDir, Tangent, Bitangent, CompareDepth, Offset + float2(-1.5f, 0.5f), ShadowTexelSize);
    ShadowCoeff += OffsetLookupPointPCF(ShadowIndex, SampleDir, Tangent, Bitangent, CompareDepth, Offset + float2(0.5f, 0.5f), ShadowTexelSize);
    ShadowCoeff += OffsetLookupPointPCF(ShadowIndex, SampleDir, Tangent, Bitangent, CompareDepth, Offset + float2(-1.5f, -1.5f), ShadowTexelSize);
    ShadowCoeff += OffsetLookupPointPCF(ShadowIndex, SampleDir, Tangent, Bitangent, CompareDepth, Offset + float2(0.5f, -1.5f), ShadowTexelSize);

    return ShadowCoeff * 0.25f;
}

float ComputeVSMVisibility(float2 Moments, float CompareDepth)
{
    const float ReceiverDepth = saturate(CompareDepth);
    float Mean = Moments.x;
    float Variance = max(Moments.y - Mean * Mean, 1e-5f);

    // Reversed-Z: larger depth is closer to the light, so GREATER_EQUAL means lit.
    if (ReceiverDepth >= Mean)
    {
        return 1.0f;
    }

    float Delta = Mean - ReceiverDepth;
    float PMax = Variance / (Variance + Delta * Delta);
    return saturate(PMax);
}

float ComputeESMVisibility(float EncodedMoment, float CompareDepth)
{
    float Receiver = exp(-SHADOW_ESM_EXPONENT * CompareDepth);
    return saturate(Receiver / max(EncodedMoment, 1e-5f));
}

float FilterSpotShadowVSM(int ShadowIndex, float2 BaseNDC, float CompareDepth)
{
    float2 BaseUV = BaseNDC * 0.5f + 0.5f;
    BaseUV.y = 1.0f - BaseUV.y;

    if (BaseUV.x < 0.0f || BaseUV.x > 1.0f || BaseUV.y < 0.0f || BaseUV.y > 1.0f)
    {
        return 1.0f;
    }

    return ComputeVSMVisibility(SampleSpotShadowMoment(ShadowIndex, BaseUV), CompareDepth);
}

float FilterPointShadowVSM(int ShadowIndex, float3 SampleDir, float CompareDepth)
{
    return ComputeVSMVisibility(SamplePointShadowMoment(ShadowIndex, SampleDir), CompareDepth);
}

float FilterSpotShadowESM(int ShadowIndex, float2 BaseNDC, float CompareDepth)
{
    float2 BaseUV = BaseNDC * 0.5f + 0.5f;
    BaseUV.y = 1.0f - BaseUV.y;

    if (BaseUV.x < 0.0f || BaseUV.x > 1.0f || BaseUV.y < 0.0f || BaseUV.y > 1.0f)
    {
        return 1.0f;
    }

    const float EncodedMoment = SampleSpotShadowMoment(ShadowIndex, BaseUV).x;
    return ComputeESMVisibility(EncodedMoment, CompareDepth);
}

float FilterPointShadowESM(int ShadowIndex, float3 SampleDir, float CompareDepth)
{
    const float EncodedMoment = SamplePointShadowMoment(ShadowIndex, SampleDir).x;
    return ComputeESMVisibility(EncodedMoment, CompareDepth);
}

float FilterSpotShadow(int ShadowIndex, float2 ShadowVector, float CompareDepth, float4 PixelPos, float2 ShadowTexelSize)
{
    float2 BaseNDC = ShadowVector;
    float2 ResolvedShadowTexelSize = ResolveShadowTexelSize(ShadowTexelSize);

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_VSM
    return FilterSpotShadowVSM(ShadowIndex, BaseNDC, CompareDepth);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_ESM
    return FilterSpotShadowESM(ShadowIndex, BaseNDC, CompareDepth);
#else
    return PCF_NvidiaOptimizedSpot(ShadowIndex, BaseNDC, CompareDepth, PixelPos, ResolvedShadowTexelSize);
#endif
}

float FilterPointShadow(int ShadowIndex, float3 ShadowVector, float CompareDepth, float4 PixelPos, float2 ShadowTexelSize)
{
    float3 SampleDir = ShadowVector;
    float2 ResolvedShadowTexelSize = ResolveShadowTexelSize(ShadowTexelSize);

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_VSM
    return FilterPointShadowVSM(ShadowIndex, SampleDir, CompareDepth);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_ESM
    return FilterPointShadowESM(ShadowIndex, SampleDir, CompareDepth);
#else
    return PCF_NvidiaOptimizedPoint(ShadowIndex, SampleDir, CompareDepth, PixelPos, ResolvedShadowTexelSize);
#endif
}

#endif
