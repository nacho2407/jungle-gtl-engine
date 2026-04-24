
/*
    LightingBlinnPhong.hlsli는 조명 계산에 쓰는 공용 함수를 제공합니다.

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

#ifndef LIGHTING_BLINNPHONG_HLSLI
#define LIGHTING_BLINNPHONG_HLSLI

float ComputeBlinnPhongSpecular(float3 Normal, float3 LightDirection, float3 ViewDirection, float Shininess)
{
    float3 H = normalize(normalize(ViewDirection) + normalize(LightDirection));
    return pow(saturate(dot(normalize(Normal), H)), max(Shininess, 1.0f));
}

#endif

