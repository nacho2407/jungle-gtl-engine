/*
    DecalSampling.hlsli는 deferred decal 투영에 필요한 sampling helper를 제공합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
*/

#ifndef DECAL_SAMPLING_HLSLI
#define DECAL_SAMPLING_HLSLI

float3 ReconstructPositionFromDepth(float2 UV, float Depth, float4x4 InverseViewProjection)
{
    float4 Clip = float4(UV * 2.0f - 1.0f, Depth, 1.0f);
    Clip.y *= -1.0f;
    float4 World = mul(Clip, InverseViewProjection);
    return World.xyz / max(World.w, 0.0001f);
}

bool IsInsideDecalBounds(float3 LocalPosition)
{
    return abs(LocalPosition.x) <= 0.5f
        && abs(LocalPosition.y) <= 0.5f
        && abs(LocalPosition.z) <= 0.5f;
}

float2 ProjectDecalUV(float3 LocalPosition)
{
    return float2(LocalPosition.y + 0.5f, 0.5f - LocalPosition.z);
}

#endif
