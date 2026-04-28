/*
    ShadowFiltering.hlsli: shared shadow filtering helpers for atlas-backed shadow maps.
*/

#ifndef SHADOW_FILTERING_HLSLI
#define SHADOW_FILTERING_HLSLI

#include "../../../Resources/BindingSlots.hlsli"
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

Texture2DArray g_ShadowAtlas0 : register(t20);
Texture2DArray g_ShadowAtlas1 : register(t21);
Texture2DArray g_ShadowAtlas2 : register(t22);
Texture2DArray g_ShadowAtlas3 : register(t23);

Texture2DArray<float2> g_ShadowMomentAtlas0 : register(t48);
Texture2DArray<float2> g_ShadowMomentAtlas1 : register(t49);
Texture2DArray<float2> g_ShadowMomentAtlas2 : register(t50);
Texture2DArray<float2> g_ShadowMomentAtlas3 : register(t51);

struct FShadowAtlasSample
{
    int PageIndex;
    int SliceIndex;
    float2 AtlasTexelSize;
    float2 UVScale;
    float2 UVOffset;
};

FShadowAtlasSample DecodeShadowSample(float4 Data0, float4 Data1)
{
    FShadowAtlasSample Sample;
    Sample.PageIndex = (int)Data0.x;
    Sample.SliceIndex = (int)Data0.y;
    Sample.AtlasTexelSize = Data0.zw;
    Sample.UVScale = Data1.xy;
    Sample.UVOffset = Data1.zw;
    return Sample;
}

float SampleShadowAtlasCmp(int PageIndex, int SliceIndex, float2 ShadowUV, float CompareDepth)
{
    switch (PageIndex)
    {
    case 0: return g_ShadowAtlas0.SampleCmpLevelZero(ShadowSampler, float3(ShadowUV, SliceIndex), CompareDepth);
    case 1: return g_ShadowAtlas1.SampleCmpLevelZero(ShadowSampler, float3(ShadowUV, SliceIndex), CompareDepth);
    case 2: return g_ShadowAtlas2.SampleCmpLevelZero(ShadowSampler, float3(ShadowUV, SliceIndex), CompareDepth);
    case 3: return g_ShadowAtlas3.SampleCmpLevelZero(ShadowSampler, float3(ShadowUV, SliceIndex), CompareDepth);
    default: return 1.0f;
    }
}

float2 SampleShadowAtlasMoment(int PageIndex, int SliceIndex, float2 ShadowUV)
{
    switch (PageIndex)
    {
    case 0: return g_ShadowMomentAtlas0.SampleLevel(LinearClampSampler, float3(ShadowUV, SliceIndex), 0.0f).rg;
    case 1: return g_ShadowMomentAtlas1.SampleLevel(LinearClampSampler, float3(ShadowUV, SliceIndex), 0.0f).rg;
    case 2: return g_ShadowMomentAtlas2.SampleLevel(LinearClampSampler, float3(ShadowUV, SliceIndex), 0.0f).rg;
    case 3: return g_ShadowMomentAtlas3.SampleLevel(LinearClampSampler, float3(ShadowUV, SliceIndex), 0.0f).rg;
    default: return float2(0.0f, 0.0f);
    }
}

float2 AtlasUVFromBaseUV(FShadowAtlasSample Sample, float2 BaseUV)
{
    return BaseUV * Sample.UVScale + Sample.UVOffset;
}

float ComputeVSMVisibility(float2 Moments, float CompareDepth)
{
    const float ReceiverDepth = saturate(CompareDepth);
    float Mean = Moments.x;
    float Variance = max(Moments.y - Mean * Mean, 1e-5f);

    if (ReceiverDepth <= Mean)
    {
        return 1.0f;
    }

    float Delta = ReceiverDepth - Mean;
    float PMax = Variance / (Variance + Delta * Delta);
    return saturate(PMax);
}

float ComputeESMVisibility(float EncodedMoment, float CompareDepth)
{
    float Receiver = exp(-SHADOW_ESM_EXPONENT * CompareDepth);
    return saturate(Receiver / max(EncodedMoment, 1e-5f));
}

float FilterShadowAtlas(FShadowAtlasSample Sample, float2 BaseUV, float CompareDepth, float4 PixelPos)
{
    if (Sample.PageIndex < 0 || Sample.SliceIndex < 0)
    {
        return 1.0f;
    }

    if (BaseUV.x < 0.0f || BaseUV.x > 1.0f || BaseUV.y < 0.0f || BaseUV.y > 1.0f)
    {
        return 1.0f;
    }

    const float2 AtlasUV = AtlasUVFromBaseUV(Sample, BaseUV);

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_VSM
    return ComputeVSMVisibility(SampleShadowAtlasMoment(Sample.PageIndex, Sample.SliceIndex, AtlasUV), CompareDepth);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_ESM
    return ComputeESMVisibility(SampleShadowAtlasMoment(Sample.PageIndex, Sample.SliceIndex, AtlasUV).x, CompareDepth);
#else
    float2 Offset = (float2)(frac(PixelPos.xy * 0.5f) > 0.25f);
    Offset.y += Offset.x;
    if (Offset.y > 1.1f)
    {
        Offset.y = 0.0f;
    }

    float ShadowCoeff = 0.0f;
    const float2 Kernel[4] = {
        float2(-1.5f, 0.5f),
        float2(0.5f, 0.5f),
        float2(-1.5f, -1.5f),
        float2(0.5f, -1.5f)
    };

    [unroll]
    for (int KernelIndex = 0; KernelIndex < 4; ++KernelIndex)
    {
        const float2 SampleUV = AtlasUV + (Kernel[KernelIndex] + Offset) * Sample.AtlasTexelSize;
        ShadowCoeff += SampleShadowAtlasCmp(Sample.PageIndex, Sample.SliceIndex, SampleUV, CompareDepth);
    }

    return ShadowCoeff * 0.25f;
#endif
}

#endif
