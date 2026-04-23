// Shader include: Common/Lighting/LightingLambert.hlsli
// Role: shared shader code or editor/material entry.
// Slots: declared locally or in included common resources.

#ifndef LIGHTING_LAMBERT_HLSLI
#define LIGHTING_LAMBERT_HLSLI

float ComputeLambertDiffuse(float3 Normal, float3 LightDirection)
{
    return saturate(dot(normalize(Normal), normalize(LightDirection)));
}

#endif

