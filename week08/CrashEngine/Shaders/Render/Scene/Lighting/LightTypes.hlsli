/*
    LightTypes.hlsli는 scene lighting helper에서 쓰는 보조 타입을 정의합니다.

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

#ifndef LIGHT_TYPES_HLSLI
#define LIGHT_TYPES_HLSLI

#include "../../../Resources/ConstantBuffers.hlsl"

struct FLocalBlinnPhongTerm
{
    float3 Diffuse;
    float3 Specular;
};

#endif
