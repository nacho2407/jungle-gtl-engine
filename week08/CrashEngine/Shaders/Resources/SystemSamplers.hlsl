#ifndef SYSTEM_SAMPLERS_HLSL
#define SYSTEM_SAMPLERS_HLSL

SamplerState LinearClampSampler : register(s0);
SamplerState LinearWrapSampler  : register(s1);
SamplerState           PointClampSampler  : register(s2);
SamplerComparisonState ShadowSampler      : register(s3);

#endif

