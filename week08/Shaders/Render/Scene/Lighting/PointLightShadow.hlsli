/*
    PointLightShadow.hlsli
    포인트 라이트 그림자 전용 헤더입니다.
    큐브맵 6면 중 어느 face를 샘플링할지 결정하고, 포인트 라이트용 bias/필터링 흐름을 계산합니다.

    참고 바인딩
    - 직접 슬롯을 선언하지는 않습니다.
    - 내부적으로 `ShadowProjection.hlsli`, `ShadowSampling.hlsli`를 통해 shadow atlas 텍스처/샘플러를 사용합니다.
*/

#ifndef POINT_LIGHT_SHADOW_HLSLI
#define POINT_LIGHT_SHADOW_HLSLI

#include "LightTypes.hlsli"
#include "ShadowProjection.hlsli"

int ResolvePointShadowFaceIndex(float3 L)
{
    float3 AbsL = abs(L);
    if (AbsL.x >= AbsL.y && AbsL.x >= AbsL.z)
    {
        return (L.x >= 0.0f) ? 0 : 1;
    }
    if (AbsL.y >= AbsL.x && AbsL.y >= AbsL.z)
    {
        return (L.y >= 0.0f) ? 2 : 3;
    }
    return (L.z >= 0.0f) ? 4 : 5;
}

float3 BuildPointShadowSamplePosition(float3 LightPos, float3 SampleDir, float ReceiverDistance)
{
    return LightPos + normalize(SampleDir) * ReceiverDistance;
}

float2 ComputePointShadowBaseUV(float4x4 ShadowViewProj, float3 SampleWorldPos)
{
    float4 ShadowPos = mul(float4(SampleWorldPos, 1.0f), ShadowViewProj);
    ShadowPos.xyz /= max(ShadowPos.w, 1e-5f);

    float2 BaseUV = ShadowPos.xy * 0.5f + 0.5f;
    BaseUV.y = 1.0f - BaseUV.y;
    return BaseUV;
}

float ComputePointShadowCompareDepth(float4x4 ShadowViewProj, float3 SampleWorldPos, float TotalBias, float ShadowFarZ)
{
    float4 ShadowPos = mul(float4(SampleWorldPos, 1.0f), ShadowViewProj);
    ShadowPos.xyz /= max(ShadowPos.w, 1e-5f);

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_NONE || SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_PCF
    // Reversed-Z에서는 값이 클수록 광원에 가까우므로 바이어스를 더해야 Acne를 방지할 수 있음
    return ShadowPos.z + TotalBias;
#else
    float CompareDepth = ComputeLinearShadowCompareDepth(ShadowPos.z, SHADOW_PROJECTION_PERSPECTIVE, 1.0f, ShadowFarZ);
    // Linear depth(0~1)로 변환된 경우에는 바이어스를 빼야 광원에 가까워짐
    return saturate(CompareDepth - TotalBias);
#endif
}

float GetPointShadowKernelScale(FShadowAtlasSample ShadowSample)
{
    const float FaceResolution = max(ShadowSample.UVScale.x / max(ShadowSample.AtlasTexelSize.x, 1e-6f), 1.0f);
    return 2.0f / FaceResolution;
}

float SamplePointShadowFaceVisibility(
    FLocalLight LocalLight,
    float3 SampleWorldPos,
    float TotalBias,
    int FaceIndex,
    float ShadowSharpen,
    float ShadowESMExponent)
{
    const FShadowAtlasSample ShadowSample = DecodeShadowSample(LocalLight.ShadowSampleData[FaceIndex]);
    if (ShadowSample.PageIndex < 0)
    {
        return 1.0f;
    }

    const float2 BaseUV = ComputePointShadowBaseUV(LocalLight.ShadowViewProj[FaceIndex], SampleWorldPos);
    if (BaseUV.x < 0.0f || BaseUV.x > 1.0f || BaseUV.y < 0.0f || BaseUV.y > 1.0f)
    {
        return 1.0f;
    }

    const float2 AtlasUV = ClampAtlasUVToSampleBounds(ShadowSample, AtlasUVFromBaseUV(ShadowSample, BaseUV));
    const float CompareDepth = ComputePointShadowCompareDepth(
        LocalLight.ShadowViewProj[FaceIndex],
        SampleWorldPos,
        TotalBias,
        LocalLight.AttenuationRadius);

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_NONE || SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_PCF
    return SampleShadowAtlasCmp(ShadowSample.PageIndex, ShadowSample.SliceIndex, AtlasUV, CompareDepth);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_VSM
    return ComputeVSMVisibility(
        SampleShadowAtlasMoment(ShadowSample.PageIndex, ShadowSample.SliceIndex, AtlasUV),
        CompareDepth,
        ShadowSharpen);
#else
    return ComputeESMVisibility(
        SampleShadowAtlasMoment(ShadowSample.PageIndex, ShadowSample.SliceIndex, AtlasUV).x,
        CompareDepth,
        ShadowESMExponent);
#endif
}

float GetPointShadowFactor(FLocalLight LocalLight, float3 WorldPos, float3 Normal, float4 PixelPos)
{
    float3 ReceiverVector = WorldPos - LocalLight.Position;
    const float ReceiverDistance = length(ReceiverVector);
    if (ReceiverDistance <= 1e-6f)
    {
        return 1.0f;
    }

    const float3 ReceiverDir = ReceiverVector / ReceiverDistance;
    const float3 BiasedWorldPos = WorldPos + Normal * LocalLight.ShadowNormalBias;
    const float3 BiasedReceiverVector = BiasedWorldPos - LocalLight.Position;
    const float BiasedReceiverDistance = max(length(BiasedReceiverVector), 1e-5f);

    const float CosTheta = saturate(dot(Normal, normalize(LocalLight.Position - WorldPos)));
    const float SlopeFactor = sqrt(1.0f - CosTheta * CosTheta) / max(CosTheta, 0.0001f);
    const float TotalBias = LocalLight.ShadowBias + saturate(LocalLight.ShadowSlopeBias * SlopeFactor);

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_NONE
    const int FaceIndex = ResolvePointShadowFaceIndex(ReceiverDir);
    const float3 SampleWorldPos = BuildPointShadowSamplePosition(LocalLight.Position, ReceiverDir, BiasedReceiverDistance);
    return SamplePointShadowFaceVisibility(
        LocalLight,
        SampleWorldPos,
        TotalBias,
        FaceIndex,
        LocalLight.ShadowSharpen,
        LocalLight.ShadowESMExponent);
#else
    float3 UpHint = (abs(ReceiverDir.z) < 0.999f) ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    float3 TangentX = normalize(cross(UpHint, ReceiverDir));
    float3 TangentY = normalize(cross(ReceiverDir, TangentX));

    float2 Offset = (float2)(frac(PixelPos.xy * 0.5f) > 0.25f);
    Offset.y += Offset.x;
    if (Offset.y > 1.1f)
    {
        Offset.y = 0.0f;
    }

    const float2 Kernel[4] = {
        float2(-1.5f, 0.5f),
        float2(0.5f, 0.5f),
        float2(-1.5f, -1.5f),
        float2(0.5f, -1.5f)
    };

    float ShadowCoeff = 0.0f;
    const int CenterFaceIndex = ResolvePointShadowFaceIndex(ReceiverDir);
    const FShadowAtlasSample CenterShadowSample = DecodeShadowSample(LocalLight.ShadowSampleData[CenterFaceIndex]);
    const float KernelScale = GetPointShadowKernelScale(CenterShadowSample);

    [unroll]
    for (int KernelIndex = 0; KernelIndex < 4; ++KernelIndex)
    {
        const float2 KernelOffset = (Kernel[KernelIndex] + Offset) * KernelScale;
        const float3 SampleDir = normalize(ReceiverDir + TangentX * KernelOffset.x + TangentY * KernelOffset.y);
        const int FaceIndex = ResolvePointShadowFaceIndex(SampleDir);
        const float3 SampleWorldPos = BuildPointShadowSamplePosition(LocalLight.Position, SampleDir, BiasedReceiverDistance);

        ShadowCoeff += SamplePointShadowFaceVisibility(
            LocalLight,
            SampleWorldPos,
            TotalBias,
            FaceIndex,
            LocalLight.ShadowSharpen,
            LocalLight.ShadowESMExponent);
    }

    return ShadowCoeff * 0.25f;
#endif
}

#endif
