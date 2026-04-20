#ifndef SYSTEM_SAMPLERS_HLSL
#define SYSTEM_SAMPLERS_HLSL

// ── System Samplers ──
// Renderer가 프레임 시작 시 s0-s2에 영구 바인딩하는 공용 샘플러.
// 슬롯 번호는 C++ ESamplerSlot (RenderConstants.h)과 1:1 대응.
// s3-s4는 셰이더별 커스텀 용도로 자유 사용.

SamplerState LinearClampSampler : register(s0);  // PostProcess, UI, 기본
SamplerState LinearWrapSampler  : register(s1);  // 메시 텍스처, 데칼
SamplerState PointClampSampler  : register(s2);  // 폰트, 깊이/스텐실 정밀 읽기

#endif // SYSTEM_SAMPLERS_HLSL
