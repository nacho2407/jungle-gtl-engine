// Shader include: Common/Lighting/LightingGouraud.hlsli
// Role: shared shader code or editor/material entry.
// Slots: declared locally or in included common resources.

#ifndef LIGHTING_GOURAUD_HLSLI
#define LIGHTING_GOURAUD_HLSLI

float4 ApplyGouraudLighting(float4 BaseColor, float4 GouraudLighting)
{
    return float4(BaseColor.rgb * GouraudLighting.rgb, BaseColor.a);
}

#endif

