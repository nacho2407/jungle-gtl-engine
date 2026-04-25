/*
    DecalTypes.hlsli는 decal 처리에서 사용하는 surface 타입을 정의합니다.

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

#ifndef DECAL_TYPES_HLSLI
#define DECAL_TYPES_HLSLI

struct FDecalSurfaceData
{
    float4 BaseColor;
    float4 Surface1;
    float4 Surface2;
};

#endif
