/*
    ShadowSampling.hlsli
    섀도우 아틀라스에서 값을 읽고, PCF/VSM/ESM 방식으로 가시성을 계산하는 공용 헤더입니다.
    아틀라스 페이지/슬라이스 해석, UV 변환, 경계 clamp, 최종 필터링 진입점을 포함합니다.

    슬롯 용도
    - t20~t23: 비교 샘플링용 depth shadow atlas 배열 텍스처
    - t48~t51: VSM/ESM용 moment shadow atlas 배열 텍스처
    - `ShadowSampler`: depth compare 샘플링
    - `LinearClampSampler`: moment 텍스처 선형 샘플링
*/

#ifndef SHADOW_SAMPLING_HLSLI
#define SHADOW_SAMPLING_HLSLI

#include "../../../Resources/BindingSlots.hlsli"
#include "../../../Resources/SystemSamplers.hlsl"

#define SHADOW_FILTER_METHOD_NONE 0
#define SHADOW_FILTER_METHOD_PCF 1
#define SHADOW_FILTER_METHOD_VSM 2
#define SHADOW_FILTER_METHOD_ESM 3

#ifndef SHADOW_FILTER_METHOD
#define SHADOW_FILTER_METHOD SHADOW_FILTER_METHOD_PCF
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

FShadowAtlasSample DecodeShadowSample(FShadowAtlasSampleData Data)
{
    FShadowAtlasSample Sample;
    Sample.PageIndex = Data.PageIndex;
    Sample.SliceIndex = Data.SliceIndex;
    Sample.AtlasTexelSize = Data.AtlasTexelSize;
    Sample.UVScale = Data.UVScale;
    Sample.UVOffset = Data.UVOffset;
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

float2 ClampAtlasUVToSampleBounds(FShadowAtlasSample Sample, float2 AtlasUV)
{
    const float2 HalfTexel = Sample.AtlasTexelSize * 0.5f;
    const float2 MinUV = Sample.UVOffset + HalfTexel;
    const float2 MaxUV = Sample.UVOffset + Sample.UVScale - HalfTexel;
    return clamp(AtlasUV, MinUV, MaxUV);
}

float ComputeVSMVisibility(float2 Moments, float CompareDepth, float ShadowSharpen)
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
    const float Sharpen = saturate(ShadowSharpen);
    PMax = saturate((PMax - Sharpen) / max(1.0f - Sharpen, 1e-5f));

    return PMax;
}

float ComputeESMVisibility(float EncodedMoment, float CompareDepth, float ShadowESMExponent)
{
    if (EncodedMoment <= 0.0f)
    {
        return 1.0f;
    }

    float Receiver = exp(-max(ShadowESMExponent, 0.01f) * CompareDepth);
    return saturate(Receiver / EncodedMoment);
}

float FilterShadowAtlas(FShadowAtlasSample Sample, float2 BaseUV, float CompareDepth, float ShadowSharpen, float ShadowESMExponent, float4 PixelPos)
{
    if (Sample.PageIndex < 0 || Sample.SliceIndex < 0)
    {
        return 1.0f;
    }

    if (BaseUV.x < 0.0f || BaseUV.x > 1.0f || BaseUV.y < 0.0f || BaseUV.y > 1.0f)
    {
        return 1.0f;
    }

    const float2 AtlasUV = ClampAtlasUVToSampleBounds(Sample, AtlasUVFromBaseUV(Sample, BaseUV));

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_NONE
    return SampleShadowAtlasCmp(Sample.PageIndex, Sample.SliceIndex, AtlasUV, CompareDepth);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_VSM
    return ComputeVSMVisibility(SampleShadowAtlasMoment(Sample.PageIndex, Sample.SliceIndex, AtlasUV), CompareDepth, ShadowSharpen);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_ESM
    return ComputeESMVisibility(SampleShadowAtlasMoment(Sample.PageIndex, Sample.SliceIndex, AtlasUV).x, CompareDepth, ShadowESMExponent);
#else
    // 4-tap PCF (Bilinear samples cover 4x4 area)
    // Dither pattern based on pixel position
    float2 Dither = float2(
        (uint(PixelPos.x) & 1) ^ (uint(PixelPos.y) & 1),
        (uint(PixelPos.y) & 1)
    ) - 0.5f;

    float ShadowCoeff = 0.0f;
    const float2 Kernel[4] = {
        float2(-1.0f,  1.0f),
        float2( 1.0f,  1.0f),
        float2(-1.0f, -1.0f),
        float2( 1.0f, -1.0f)
    };

    [unroll]
    for (int KernelIndex = 0; KernelIndex < 4; ++KernelIndex)
    {
        const float2 SampleUV = ClampAtlasUVToSampleBounds(
            Sample,
            AtlasUV + (Kernel[KernelIndex] + Dither) * Sample.AtlasTexelSize);
        ShadowCoeff += SampleShadowAtlasCmp(Sample.PageIndex, Sample.SliceIndex, SampleUV, CompareDepth);
    }

    return ShadowCoeff * 0.25f;
#endif
}

#endif
