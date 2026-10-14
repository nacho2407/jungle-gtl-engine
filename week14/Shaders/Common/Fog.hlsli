#ifndef FOG_HLSLI
#define FOG_HLSLI

#include "Common/Functions.hlsli"

cbuffer FogBuffer : register(b7)
{
    float4 FogInscatteringColor;
    float FogDensity;
    float FogHeightFalloff;
    float FogBaseHeight;
    float FogStartDistance;
    float FogCutoffDistance;
    float FogMaxOpacity;
    float2 _fogPad;
};

float ComputeHeightFogFactor(float3 worldPos)
{
    float3 camPos = CameraWorldPos;
    float3 ray = worldPos - camPos;
    float rayLength = length(ray);
    
    float effectiveLength = max(rayLength - FogStartDistance, 0.0);
    if (FogCutoffDistance > 0.0)
        effectiveLength = min(effectiveLength, FogCutoffDistance - FogStartDistance);
    
    float rayDirZ = ray.z / max(rayLength, 0.001);
    float falloff = max(FogHeightFalloff, 0.001);
    
    float startHeight = camPos.z + rayDirZ * FogStartDistance - FogBaseHeight;
    float endHeight = startHeight + rayDirZ * effectiveLength;
    
    float dz = rayDirZ * effectiveLength;
    float lineIntegral;
    
    if (abs(dz * falloff) > 0.001)
        lineIntegral = FogDensity * (exp(-falloff * startHeight) - exp(-falloff * endHeight)) / (falloff * rayDirZ);
    else
        lineIntegral = FogDensity * exp(-falloff * startHeight) * effectiveLength;
    
    lineIntegral = max(lineIntegral, 0.0);
    
    return clamp(1.0 - exp(-lineIntegral), 0.0, FogMaxOpacity);
}

float3 ApplyHeightFog(float3 color, float3 worldPos)
{
    float fogFactor = ComputeHeightFogFactor(worldPos);
    return lerp(color, FogInscatteringColor.rgb, fogFactor);
}

float4 GetHeightFogOverlay(float3 worldPos)
{
    float fogFactor = ComputeHeightFogFactor(worldPos);
    return float4(FogInscatteringColor.rgb, fogFactor);
}

#endif
