/*
    SystemSamplers.hlsl는 공용 시스템 샘플러 슬롯 선언을 제공합니다.

    바인딩 컨벤션
    - s0: LinearClamp
    - s1: LinearWrap
    - s2: PointClamp
    - 이 파일에서 직접 선언하는 슬롯: s0, s1, s2
*/

#ifndef SYSTEM_SAMPLERS_HLSL
#define SYSTEM_SAMPLERS_HLSL

SamplerState LinearClampSampler : register(s0);
SamplerState LinearWrapSampler  : register(s1);
SamplerState PointClampSampler  : register(s2);

#endif // SYSTEM_SAMPLERS_HLSL
