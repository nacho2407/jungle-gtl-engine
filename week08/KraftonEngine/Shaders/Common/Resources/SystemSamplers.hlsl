// Shader include: Common/Resources/SystemSamplers.hlsl
// Role: shared shader code or editor/material entry.
// Slots: declared locally or in included common resources.

#ifndef SYSTEM_SAMPLERS_HLSL
#define SYSTEM_SAMPLERS_HLSL


SamplerState LinearClampSampler : register(s0);
SamplerState LinearWrapSampler  : register(s1);
SamplerState PointClampSampler  : register(s2);

#endif // SYSTEM_SAMPLERS_HLSL

