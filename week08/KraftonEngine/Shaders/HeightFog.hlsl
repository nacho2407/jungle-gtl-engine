// HeightFog.hlsl
// Fullscreen Triangle VS (SV_VertexID) + Exponential Height Fog PS

#include "Common/Functions.hlsl"
#include "Common/SystemResources.hlsl"

// b2 (PerShader0): Fog parameters
cbuffer FogBuffer : register(b2)
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

// SceneDepth (t10) is declared in Common/ConstantBuffers.hlsl

// ── VS: Fullscreen Triangle ──
PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

// ── PS: Exponential Height Fog ──
float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);

    // Sample hardware depth (Reversed-Z: 1=near, 0=far)
    float depth = SceneDepth.Load(int3(coord, 0));
    if (depth <= 0.0)
        depth = 0.001f;

    // Reconstruct world position from depth
    float2 ndc = float2(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0);
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 worldH = mul(clipPos, InvViewProj);
    float3 worldPos = worldH.xyz / worldH.w;

    // Reconstruct camera world position at near plane (Reversed-Z: near=1)
    float4 camClip = float4(0, 0, 1, 1);
    float4 camWorldH = mul(camClip, InvViewProj);
    float3 camPos = camWorldH.xyz / camWorldH.w;

    float3 rayDir = worldPos - camPos;
    float rayLength = length(rayDir);

    // Start distance culling
    if (rayLength < FogStartDistance)
        discard;

    // Cutoff distance
    float effectiveLength = rayLength - FogStartDistance;
    if (FogCutoffDistance > 0.0)
        effectiveLength = min(effectiveLength, FogCutoffDistance - FogStartDistance);

    // Exponential height fog (UE-style)
    // f = FogDensity * exp(-FogHeightFalloff * (camPos.z - FogBaseHeight))
    // integrated along ray direction
    float rayDirZ = rayDir.z / max(rayLength, 0.001);
    float falloff = max(FogHeightFalloff, 0.001);

    // Height factor at camera
    float camRelHeight = camPos.z - FogBaseHeight;
    float densityAtCam = FogDensity * exp(-falloff * camRelHeight);

    // Analytical line integral of exp(-falloff * (camH + t * rayDirZ)) dt from 0 to effectiveLength
    float dz = rayDirZ * effectiveLength;
    float lineIntegral;
    if (abs(dz * falloff) > 0.001)
    {
        lineIntegral = densityAtCam * (1.0 - exp(-falloff * dz)) / (falloff * rayDirZ);
    }
    else
    {
        // Taylor approximation for near-horizontal rays
        lineIntegral = densityAtCam * effectiveLength;
    }

    lineIntegral = max(lineIntegral, 0.0);

    // Final fog factor
    float fogFactor = 1.0 - exp(-lineIntegral);
    fogFactor = clamp(fogFactor, 0.0, FogMaxOpacity);

    return float4(FogInscatteringColor.rgb, fogFactor);
}
