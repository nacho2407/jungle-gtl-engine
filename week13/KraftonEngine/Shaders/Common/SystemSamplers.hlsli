#ifndef SYSTEM_SAMPLERS_HLSL
#define SYSTEM_SAMPLERS_HLSL

// ── System Samplers ──
// Renderer가 프레임 시작 시 s0-s4에 영구 바인딩하는 공용 샘플러.
// 슬롯 번호는 C++ ESamplerSlot (RenderConstants.h)과 1:1 대응.

SamplerState LinearClampSampler : register(s0);             // PostProcess, UI, 기본
SamplerState LinearWrapSampler  : register(s1);             // 메시 텍스처, 데칼
SamplerState PointClampSampler  : register(s2);             // 폰트, 깊이/스텐실 정밀 읽기
SamplerComparisonState ShadowComparisonSampler : register(s3); // PCF shadow (Comparison)
SamplerState ShadowLinearSampler : register(s4);            // VSM shadow (Linear)

#endif // SYSTEM_SAMPLERS_HLSL
