/*
    ShadowProjection.hlsli
    방향광/스포트라이트 그림자용 투영 좌표 계산과 compare depth 생성을 담당하는 헤더입니다.
    월드 위치를 shadow clip space로 보낸 뒤 bias를 적용하고, 최종 shadow visibility를 계산합니다.

    참고 바인딩
    - 직접 슬롯을 선언하지는 않습니다.
    - 내부적으로 `ShadowSampling.hlsli`를 포함하므로 shadow atlas 텍스처와 샘플러를 간접 사용합니다.
*/

#ifndef SHADOW_PROJECTION_HLSLI
#define SHADOW_PROJECTION_HLSLI

#include "ShadowSampling.hlsli"

#define SHADOW_PROJECTION_ORTHOGRAPHIC 0
#define SHADOW_PROJECTION_PERSPECTIVE 1

float LinearizeReversedZPerspectiveDepth01(float ReverseDepth, float NearZ, float FarZ)
{
    const float Denominator = max(NearZ + ReverseDepth * (FarZ - NearZ), 1e-5f);
    const float ViewZ = (NearZ * FarZ) / Denominator;
    return saturate((ViewZ - NearZ) / max(FarZ - NearZ, 1e-5f));
}

float ComputeLinearShadowCompareDepth(float ReverseDepth, uint ShadowProjectionType, float ShadowNearZ, float ShadowFarZ)
{
    if (ShadowProjectionType == SHADOW_PROJECTION_PERSPECTIVE)
    {
        return LinearizeReversedZPerspectiveDepth01(ReverseDepth, ShadowNearZ, ShadowFarZ);
    }

    return saturate(1.0f - ReverseDepth);
}

float GetShadowFactor(FShadowAtlasSample ShadowSample, float4x4 ShadowViewProj, float3 WorldPos, float3 Normal, float3 LightDir, float Bias, float SlopeBias, float NormalBias, float ShadowSharpen, float ShadowESMExponent, float4 PixelPos, uint ShadowProjectionType, float ShadowNearZ, float ShadowFarZ)
{
    if (ShadowSample.PageIndex < 0)
        return 1.0f;

    float3 BiasedWorldPos = WorldPos + Normal * NormalBias;

    float4 ShadowPos = mul(float4(BiasedWorldPos, 1.0f), ShadowViewProj);
    ShadowPos.xyz /= ShadowPos.w;

    if (abs(ShadowPos.x) > 1.0f || abs(ShadowPos.y) > 1.0f ||
        ShadowPos.z < 0.0f || ShadowPos.z > 1.0f)
    {
        return 1.0f;
    }

    float CosTheta = saturate(dot(Normal, -LightDir));
    float SlopeFactor = sqrt(1.0f - CosTheta * CosTheta) / max(CosTheta, 0.0001f);
    float TotalBias = Bias + saturate(SlopeBias * SlopeFactor);
#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_NONE || SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_PCF
    float CompareDepth = ShadowPos.z - TotalBias;
#else
    float CompareDepth = ComputeLinearShadowCompareDepth(ShadowPos.z, ShadowProjectionType, ShadowNearZ, ShadowFarZ);
    CompareDepth = saturate(CompareDepth - TotalBias);
#endif

    float2 BaseUV = ShadowPos.xy * 0.5f + 0.5f;
    BaseUV.y = 1.0f - BaseUV.y;
    return FilterShadowAtlas(ShadowSample, BaseUV, CompareDepth, ShadowSharpen, ShadowESMExponent, PixelPos);
}

#endif
