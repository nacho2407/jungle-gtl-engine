
/*
    SystemSamplers.hlsl는 공용 GPU 리소스 슬롯 선언을 제공합니다.

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
    - 이 파일에서 직접 선언한 슬롯: s0, s1, s2
*/

#ifndef SYSTEM_SAMPLERS_HLSL
#define SYSTEM_SAMPLERS_HLSL


SamplerState LinearClampSampler : register(s0);
SamplerState LinearWrapSampler  : register(s1);
SamplerState PointClampSampler  : register(s2);

#endif // SYSTEM_SAMPLERS_HLSL

