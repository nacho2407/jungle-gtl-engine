
/*
    HeightFogPass.hlsl는 후처리 렌더 패스의 셰이더입니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
    - 이 파일에서 직접 선언한 슬롯: b2
*/

// Fullscreen Triangle VS (SV_VertexID) + exponential height fog PS.

#include "../../Common/Utils/Functions.hlsl"
#include "../../Common/Resources/SystemResources.hlsl"

// b2 (PerShader0): Fog parameters
cbuffer FogParams : register(b2)
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

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);

    // Sample hardware depth (Reversed-Z: 1=near, 0=far)
    float depth = SceneDepth.Load(int3(coord, 0));
    if (depth <= 0.0)
    {
        // Sky/background: no geometry to reconstruct world position
        if (FogCutoffDistance > 0.0)
            discard;
        return float4(FogInscatteringColor.rgb, FogMaxOpacity);
    }

    // Reconstruct world position from depth
    float2 ndc = float2(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0);
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 worldH = mul(clipPos, InvViewProj);
    float3 worldPos = worldH.xyz / worldH.w;

    // Camera position from FrameParams (b0)
    float3 camPos = CameraWorldPos;

    float3 rayDir = worldPos - camPos;
    float rayLength = length(rayDir);

    float effectiveLength = max(rayLength - FogStartDistance, 0.0);
    if (FogCutoffDistance > 0.0)
        effectiveLength = min(effectiveLength, FogCutoffDistance - FogStartDistance);

    // Density at height h: FogDensity * exp(-falloff * (h - FogBaseHeight))
    // Line integral along ray, computed with exp at both endpoints separately
    // to avoid float precision loss when camera is far above FogBaseHeight.
    float rayDirZ = rayDir.z / max(rayLength, 0.001);
    float falloff = max(FogHeightFalloff, 0.001);

    // Heights relative to FogBaseHeight at actual integration endpoints
    // Integration starts at FogStartDistance along the ray, not at camera
    float startHeight = camPos.z + rayDirZ * FogStartDistance - FogBaseHeight;
    float endHeight = startHeight + rayDirZ * effectiveLength;

    float dz = rayDirZ * effectiveLength;
    float lineIntegral;
    if (abs(dz * falloff) > 0.001)
    {
        lineIntegral = FogDensity * (exp(-falloff * startHeight) - exp(-falloff * endHeight)) / (falloff * rayDirZ);
    }
    else
    {
        // Near-horizontal rays: density approximately constant along ray
        lineIntegral = FogDensity * exp(-falloff * startHeight) * effectiveLength;
    }

    lineIntegral = max(lineIntegral, 0.0);

    // Final fog factor
    float fogFactor = 1.0 - exp(-lineIntegral);
    fogFactor = clamp(fogFactor, 0.0, FogMaxOpacity);

    return float4(FogInscatteringColor.rgb, fogFactor);
}

