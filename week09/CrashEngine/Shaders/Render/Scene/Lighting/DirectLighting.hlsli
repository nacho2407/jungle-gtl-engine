/*
    DirectLighting.hlsli
    장면의 직접광(Direct Light) 계산을 담당하는 헤더입니다.
    direct light는 빛이 표면에 한 번 바로 도달하는 성분을 뜻하며,
    방향광, 포인트 라이트, 스포트라이트의 diffuse/specular/shadow 합산이 여기 들어 있습니다.
    반대로 indirect light는 반사, 전역 조명, bounce light처럼 다른 표면을 거친 빛입니다.

    슬롯 용도
    - t6: `g_LightBuffer`, 로컬 라이트 목록 structured buffer
    - `SLOT_TEX_LIGHT_TILE_MASK`: 타일 기반 라이트 컬링 마스크
    - `SLOT_TEX_DEBUG_HIT_MAP`: 디버그용 light hit map 텍스처
    - b2: `LightCullingParams`, 화면 크기/타일 크기/깊이 범위/라이트 개수
    - `SceneDepth`: 깊이로부터 월드 위치를 복원할 때 사용
*/

#ifndef DIRECT_LIGHTING_HLSLI
#define DIRECT_LIGHTING_HLSLI

#include "../../../Resources/SystemResources.hlsl"
#include "../../../Resources/SystemSamplers.hlsl"
#include "LightTypes.hlsli"
#include "BRDF.hlsli"
#include "ShadowProjection.hlsli"
#include "PointLightShadow.hlsli"

#define TILE_SIZE                       4
#define NUM_SLICES                      32
#define MAX_LIGHTS_PER_TILE             1024
#define SHADER_ENTITY_TILE_BUCKET_COUNT (MAX_LIGHTS_PER_TILE / 32)

StructuredBuffer<FLocalLight> g_LightBuffer : register(t6);
#ifdef USE_LIGHT_CULLING
StructuredBuffer<uint> PerTileLightMask : REGISTER_T(SLOT_TEX_LIGHT_TILE_MASK);
Texture2D g_DebugHitMapTex : REGISTER_T(SLOT_TEX_DEBUG_HIT_MAP);

cbuffer LightCullingParams : register(b2)
{
    uint2 ScreenSize;
    uint2 TileSize;

    uint Enable25DCulling;
    float NearZ;
    float FarZ;
    float NumLights;
}
#endif

// =============================================================================
// DEBUG: Cascade Visualization (Set to 1 to enable, 0 to disable)
#define DEBUG_VISUALIZE_CSM 0

float4 GetCascadeDebugColor(int Index)
{
    float4 Colors[4] = {
        float4(1.0, 0.2, 0.2, 1.0), // 0: Red
        float4(0.2, 1.0, 0.2, 1.0), // 1: Green
        float4(0.2, 0.2, 1.0, 1.0), // 2: Blue
        float4(1.0, 1.0, 0.2, 1.0)  // 3: Yellow
    };
    return Colors[uint(Index) % 4];
}

float GetDirectionalShadow(int i, float3 WorldPosition, float3 N, float4 PixelPos)
{
    float4 ViewPos = mul(float4(WorldPosition, 1.0f), View);
    float Depth = ViewPos.z;

    int CascadeIdx = 0;
    for (int j = 0; j < Directional[i].CascadeCount - 1; ++j)
    {
        float split = Directional[i].CascadeSplits[(j + 1) / 4][(j + 1) % 4];
        if (Depth > split)
        {
            CascadeIdx = j + 1;
        }
        else
        {
            break;
        }
    }

    float Shadow = GetShadowFactor(
        DecodeShadowSample(Directional[i].ShadowSampleData[CascadeIdx]),
        Directional[i].ShadowViewProj[CascadeIdx],
        WorldPosition,
        N,
        Directional[i].Direction,
        Directional[i].ShadowBias,
        Directional[i].ShadowSlopeBias,
        Directional[i].ShadowNormalBias,
        Directional[i].ShadowSharpen,
        Directional[i].ShadowESMExponent,
        PixelPos,
        SHADOW_PROJECTION_ORTHOGRAPHIC,
        0.0f,
        1.0f);

    // Cascade Blending
    if (CascadeIdx < Directional[i].CascadeCount - 1)
    {
        float nextSplit = Directional[i].CascadeSplits[(CascadeIdx + 1) / 4][(CascadeIdx + 1) % 4];
        float prevSplit = Directional[i].CascadeSplits[CascadeIdx / 4][CascadeIdx % 4];
        
        // 블렌딩 영역 설정 (캐스케이드 범위의 10%)
        float BlendZone = (nextSplit - prevSplit) * 0.1f;
        float Alpha = saturate((Depth - (nextSplit - BlendZone)) / max(BlendZone, 0.001f));

        if (Alpha > 0.0f)
        {
            float NextShadow = GetShadowFactor(
                DecodeShadowSample(Directional[i].ShadowSampleData[CascadeIdx + 1]),
                Directional[i].ShadowViewProj[CascadeIdx + 1],
                WorldPosition,
                N,
                Directional[i].Direction,
                Directional[i].ShadowBias,
                Directional[i].ShadowSlopeBias,
                Directional[i].ShadowNormalBias,
                Directional[i].ShadowSharpen,
                Directional[i].ShadowESMExponent,
                PixelPos,
                SHADOW_PROJECTION_ORTHOGRAPHIC,
                0.0f,
                1.0f);
            Shadow = lerp(Shadow, NextShadow, Alpha);
        }
    }

    return Shadow;
}

int SelectCascadeIndex(float3 WorldPos, float4 CascadeSplits[2], int CascadeCount)
{
    float4 ViewPos = mul(float4(WorldPos, 1.0f), View);
    float Depth = ViewPos.z;

    int Index = 0;
    for (int i = 0; i < CascadeCount - 1; ++i)
    {
        float nextCascadeSplit = CascadeSplits[(i + 1) / 4][(i + 1) % 4];
        if (Depth > nextCascadeSplit)
        {
            Index = i + 1;
        }
        else
        {
            break;
        }
    }
    return Index;
}
// =============================================================================

float3 GetAmbientLightColor()
{
    return Ambient.Color * Ambient.Intensity;
}

float3 ReconstructWorldPositionFromSceneDepth(float2 UV)
{
    float Depth = SceneDepth.Sample(PointClampSampler, UV).r;
    float4 Clip = float4(UV * 2.0f - 1.0f, Depth, 1.0f);
    Clip.y *= -1.0f;
    float4 World = mul(Clip, InvViewProj);
    return World.xyz / max(World.w, 0.0001f);
}

float3 LocalLightLambertTerm(FLocalLight LocalLight, float3 N, float3 WorldPosition, float4 PixelPos)
{
    float3 LightVector = LocalLight.Position - WorldPosition;
    float Distance = length(LightVector);

    if (Distance >= LocalLight.AttenuationRadius || LocalLight.AttenuationRadius <= 0.001f)
        return 0;

    float3 L = LightVector / Distance;
    float Diffuse = saturate(dot(N, L));

    float Attenuation = saturate(1.0f - (Distance / LocalLight.AttenuationRadius));
    Attenuation *= Attenuation;

    float Shadow = 1.0f;
    if (LocalLight.LightType == 2)  // Point light (see LightProxyInfo.h)
    {
        Shadow = GetPointShadowFactor(LocalLight, WorldPosition, N, PixelPos);
    }
    else                            // Spot light
    {
        float3 SpotDir = normalize(LocalLight.Direction);
        Attenuation *= smoothstep(
            cos(radians(LocalLight.OuterConeAngle)),
            cos(radians(LocalLight.InnerConeAngle)),
            dot(-L, SpotDir));
        Shadow = GetShadowFactor(
            DecodeShadowSample(LocalLight.ShadowSampleData[0]),
            LocalLight.ShadowViewProj[0],
            WorldPosition,
            N,
            LocalLight.Direction,
            LocalLight.ShadowBias,
            LocalLight.ShadowSlopeBias,
            LocalLight.ShadowNormalBias,
            LocalLight.ShadowSharpen,
            LocalLight.ShadowESMExponent,
            PixelPos,
            SHADOW_PROJECTION_PERSPECTIVE,
            1.0f,
            LocalLight.AttenuationRadius);
    }

    return Diffuse * LocalLight.Color * LocalLight.Intensity * Attenuation * Shadow;
}

FLocalBlinnPhongTerm LocalLightBlinnPhongTerm(
    FLocalLight LocalLight,
    float3 N,
    float3 WorldPosition,
    float3 V,
    float Shininess,
    float SpecularStrength,
    float4 PixelPos)
{
    FLocalBlinnPhongTerm Out;
    Out.Diffuse = 0;
    Out.Specular = 0;

    float3 LightVector = LocalLight.Position - WorldPosition;
    float Distance = length(LightVector);

    if (Distance >= LocalLight.AttenuationRadius || LocalLight.AttenuationRadius <= 0.001f)
        return Out;

    float3 L = LightVector / Distance;
    float3 H = normalize(V + L);

    float Diffuse = saturate(dot(N, L));
    float Specular = pow(saturate(dot(N, H)), Shininess) * SpecularStrength;

    float Attenuation = saturate(1.0f - (Distance / LocalLight.AttenuationRadius));
    Attenuation *= Attenuation;

    float Shadow = 1.0f;
    if (LocalLight.LightType == 2)  // Point light (see LightProxyInfo.h)
    {
        Shadow = GetPointShadowFactor(LocalLight, WorldPosition, N, PixelPos);
    }
    else                            // Spot light
    {
        float3 SpotDir = normalize(LocalLight.Direction);
        Attenuation *= smoothstep(
            cos(radians(LocalLight.OuterConeAngle)),
            cos(radians(LocalLight.InnerConeAngle)),
            dot(-L, SpotDir));

        Shadow = GetShadowFactor(
            DecodeShadowSample(LocalLight.ShadowSampleData[0]),
            LocalLight.ShadowViewProj[0],
            WorldPosition,
            N,
            LocalLight.Direction,
            LocalLight.ShadowBias,
            LocalLight.ShadowSlopeBias,
            LocalLight.ShadowNormalBias,
            LocalLight.ShadowSharpen,
            LocalLight.ShadowESMExponent,
            PixelPos,
            SHADOW_PROJECTION_PERSPECTIVE,
            1.0f,
            LocalLight.AttenuationRadius);
    }

    float3 LightColor = LocalLight.Color * LocalLight.Intensity;
    Out.Diffuse = Diffuse * LightColor * Attenuation * Shadow;
    Out.Specular = Specular * LightColor * Attenuation * Shadow;
    return Out;
}

float4 ComputeGouraudLighting(float4 BaseColor, float4 GouraudL)
{
    return ComputeGouraudLitColor(BaseColor, GouraudL);
}

float3 ComputeGouraudLightingColor(float3 Normal, float3 WorldPosition, float4 PixelPos)
{
    float3 N = normalize(Normal);
    float3 TotalLight = GetAmbientLightColor();

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 L = normalize(Directional[i].Direction);
        float Diffuse = saturate(dot(N, -L));
        float Shadow = GetDirectionalShadow(i, WorldPosition, N, PixelPos);
        TotalLight += Diffuse * Directional[i].Color * Directional[i].Intensity * Shadow;
#if DEBUG_VISUALIZE_CSM
        if (i == 0) return GetCascadeDebugColor(SelectCascadeIndex(WorldPosition, Directional[i].CascadeSplits, Directional[i].CascadeCount)).rgb;
#endif
    }

    for (int j = 0; j < NumLocalLights; ++j)
    {
        TotalLight += LocalLightLambertTerm(g_LightBuffer[j], N, WorldPosition, PixelPos);
    }

    return saturate(TotalLight);
}

float4 ComputeLambertLighting(float4 BaseColor, float3 Normal, float3 WorldPosition, float4 PixelPos)
{
    float3 N = normalize(Normal);
    float3 TotalLight = GetAmbientLightColor();

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 L = normalize(Directional[i].Direction);
        float Diffuse = saturate(dot(N, -L));
        float Shadow = GetDirectionalShadow(i, WorldPosition, N, PixelPos);
        TotalLight += Diffuse * Directional[i].Color * Directional[i].Intensity * Shadow;
#if DEBUG_VISUALIZE_CSM
        if (i == 0) return float4(GetCascadeDebugColor(SelectCascadeIndex(WorldPosition, Directional[i].CascadeSplits, Directional[i].CascadeCount)).rgb, BaseColor.a);
#endif
    }

    for (int j = 0; j < NumLocalLights; ++j)
    {
        TotalLight += LocalLightLambertTerm(g_LightBuffer[j], N, WorldPosition, PixelPos);
    }

    return float4(BaseColor.rgb * saturate(TotalLight), BaseColor.a);
}

float3 ComputeLambertGlobalLight(float3 Normal, float3 WorldPosition, float4 pixelPos)
{
    float3 N = normalize(Normal);
    float3 TotalLight = GetAmbientLightColor();

    [unroll]
    for (int i = 0; i < MAX_DIRECTIONAL_LIGHTS; ++i)
    {
        if (i >= NumDirectionalLights) break;
        float3 L = normalize(Directional[i].Direction);
        float Shadow = GetDirectionalShadow(i, WorldPosition, N, pixelPos);
        TotalLight += saturate(dot(N, -L)) * Directional[i].Color * Directional[i].Intensity * Shadow;
#if DEBUG_VISUALIZE_CSM
        if (i == 0) return GetCascadeDebugColor(SelectCascadeIndex(WorldPosition, Directional[i].CascadeSplits, Directional[i].CascadeCount)).rgb;
#endif
    }

    return TotalLight;
}

float4 ComputeLambertLightingGlobalOnly(float4 BaseColor, float3 Normal, float3 WorldPos, float4 PixelPos)
{
    return float4(BaseColor.rgb * saturate(ComputeLambertGlobalLight(Normal, WorldPos, PixelPos)), BaseColor.a);
}

float4 ComputeBlinnPhongLighting(float4 BaseColor, float3 Normal, float4 MaterialParam, float3 WorldPosition, float3 ViewDirection, float4 PixelPos)
{
    float3 N = normalize(Normal);
    float3 TotalDiffuse = GetAmbientLightColor();
    float3 TotalSpecular = 0;

    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 L = normalize(-Directional[i].Direction);
        float3 H = normalize(ViewDirection + L);

        float Diffuse = saturate(dot(N, L));
        float Specular = pow(saturate(dot(N, H)), Shininess) * SpecularStrength;

        float3 LightColor = Directional[i].Color * Directional[i].Intensity;
        float Shadow = GetDirectionalShadow(i, WorldPosition, N, PixelPos);

        TotalDiffuse += Diffuse * LightColor * Shadow;
        TotalSpecular += Specular * LightColor * Shadow;
#if DEBUG_VISUALIZE_CSM
        if (i == 0) return float4(GetCascadeDebugColor(SelectCascadeIndex(WorldPosition, Directional[i].CascadeSplits, Directional[i].CascadeCount)).rgb, BaseColor.a);
#endif
    }

    for (int j = 0; j < NumLocalLights; ++j)
    {
        FLocalBlinnPhongTerm LocalTerm = LocalLightBlinnPhongTerm(
            g_LightBuffer[j],
            N,
            WorldPosition,
            ViewDirection,
            Shininess,
            SpecularStrength,
            PixelPos);
        TotalDiffuse += LocalTerm.Diffuse;
        TotalSpecular += LocalTerm.Specular;
    }

    return float4(BaseColor.rgb * saturate(TotalDiffuse) + TotalSpecular * 0.2f, BaseColor.a);
}

FLocalBlinnPhongTerm ComputeBlinnPhongGlobalLight(float3 Normal, float4 MaterialParam, float3 ViewDirection, float3 WorldPosition, float4 PixelPos)
{
    FLocalBlinnPhongTerm Out;
    Out.Diffuse = GetAmbientLightColor();
    Out.Specular = 0.0f;

    float3 N = normalize(Normal);
    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);

    [unroll]
    for (int i = 0; i < MAX_DIRECTIONAL_LIGHTS; ++i)
    {
        if (i >= NumDirectionalLights) break;
        float3 L = normalize(-Directional[i].Direction);
        float3 H = normalize(ViewDirection + L);

        float Diffuse = saturate(dot(N, L));
        float Specular = pow(saturate(dot(N, H)), Shininess) * SpecularStrength;

        float3 LightColor = Directional[i].Color * Directional[i].Intensity;
        float Shadow = GetDirectionalShadow(i, WorldPosition, N, PixelPos);

        Out.Diffuse += Diffuse * LightColor * Shadow;
        Out.Specular += Specular * LightColor * Shadow;
#if DEBUG_VISUALIZE_CSM
        if (i == 0) return Out; // This is a bit tricky, let's just return a fake term
#endif
    }

    return Out;
}

float4 ComputeBlinnPhongLightingGlobalOnly(float4 BaseColor, float3 Normal, float4 MaterialParam, float3 ViewDirection, float3 WorldPos, float4 PixelPos)
{
    FLocalBlinnPhongTerm GlobalTerm = ComputeBlinnPhongGlobalLight(Normal, MaterialParam, ViewDirection, WorldPos, PixelPos);
    return float4(BaseColor.rgb * saturate(GlobalTerm.Diffuse) + GlobalTerm.Specular * 0.2f, BaseColor.a);
}

#endif
