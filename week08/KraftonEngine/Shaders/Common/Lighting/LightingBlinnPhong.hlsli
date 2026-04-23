// Shader include: Common/Lighting/LightingBlinnPhong.hlsli
// Role: shared shader code or editor/material entry.
// Slots: declared locally or in included common resources.

#ifndef LIGHTING_BLINNPHONG_HLSLI
#define LIGHTING_BLINNPHONG_HLSLI

float ComputeBlinnPhongSpecular(float3 Normal, float3 LightDirection, float3 ViewDirection, float Shininess)
{
    float3 H = normalize(normalize(ViewDirection) + normalize(LightDirection));
    return pow(saturate(dot(normalize(Normal), H)), max(Shininess, 1.0f));
}

#endif

