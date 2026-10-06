#ifndef FOG_HLSLI
#define FOG_HLSLI

// b7: Global Fog parameters — bound once per frame, available to all passes
// HeightFog.hlsl reads these directly; AlphaBlend shaders use ApplyFog() below.
cbuffer FogBuffer : register(b7)
{
    float4 FogInscatteringColor;
    float  FogDensity;
    float  FogHeightFalloff;
    float  FogBaseHeight;
    float  FogStartDistance;
    float  FogCutoffDistance;
    float  FogMaxOpacity;
    float  FogEnabled;   // 0 = no fog, 1 = fog active (runtime toggle)
    float  _FogPad;
};

#if defined(USE_FOG) && USE_FOG

// Compile-time fog path — included by alpha-blend shaders (particles, translucent meshes).
// Returns fog factor [0, MaxOpacity] for a world-space position.
float ComputeHeightFogFactor(float3 WorldPos, float3 CamPos)
{
    float3 RayDir    = WorldPos - CamPos;
    float  RayLength = length(RayDir);

    float EffectiveLength = max(RayLength - FogStartDistance, 0.0);
    if (FogCutoffDistance > 0.0)
        EffectiveLength = min(EffectiveLength, FogCutoffDistance - FogStartDistance);

    float RayDirZ = RayDir.z / max(RayLength, 0.001);
    float Falloff = max(FogHeightFalloff, 0.001);

    float StartHeight = CamPos.z + RayDirZ * FogStartDistance - FogBaseHeight;
    float EndHeight   = StartHeight + RayDirZ * EffectiveLength;
    float Dz          = RayDirZ * EffectiveLength;

    float LineIntegral;
    if (abs(Dz * Falloff) > 0.001)
        LineIntegral = FogDensity * (exp(-Falloff * StartHeight) - exp(-Falloff * EndHeight)) / (Falloff * RayDirZ);
    else
        LineIntegral = FogDensity * exp(-Falloff * StartHeight) * EffectiveLength;

    LineIntegral = max(LineIntegral, 0.0);
    return clamp(1.0 - exp(-LineIntegral), 0.0, FogMaxOpacity);
}

// 불투명 오브젝트용 (현재 no-op — HeightFog 풀스크린 패스가 불투명을 처리함)
float4 ApplyFog(float4 Color, float3 WorldPos, float3 CamPos)
{
    if (FogEnabled < 0.5)
        return Color;
    float F = ComputeHeightFogFactor(WorldPos, CamPos);
    Color.rgb = lerp(Color.rgb, FogInscatteringColor.rgb, F);
    return Color;
}

// 반투명(AlphaBlend) 오브젝트 전용.
// RGB를 포그 색으로 블렌딩하는 동시에, 포그 투과율(1-F)로 알파를 감쇠시켜
// 먼 파티클이 자연스럽게 희미해지도록 한다.
float4 ApplyFogTranslucent(float4 Color, float3 WorldPos, float3 CamPos)
{
    if (FogEnabled < 0.5)
        return Color;
    float F = ComputeHeightFogFactor(WorldPos, CamPos);
    Color.rgb  = lerp(Color.rgb, FogInscatteringColor.rgb, F);
    Color.a   *= saturate(1.0 - F);
    return Color;
}

#else

// Compile-time fog disabled — optimizer eliminates these entirely.
float4 ApplyFog(float4 Color, float3 WorldPos, float3 CamPos) { return Color; }
float4 ApplyFogTranslucent(float4 Color, float3 WorldPos, float3 CamPos) { return Color; }

#endif // USE_FOG

#endif // FOG_HLSLI
