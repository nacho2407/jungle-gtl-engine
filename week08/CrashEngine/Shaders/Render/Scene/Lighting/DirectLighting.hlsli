/*
    DirectLighting.hlsli: scene direct lighting helpers.
*/

#ifndef DIRECT_LIGHTING_HLSLI
#define DIRECT_LIGHTING_HLSLI

#include "../../../Resources/SystemResources.hlsl"
#include "../../../Resources/SystemSamplers.hlsl"
#include "LightTypes.hlsli"
#include "BRDF.hlsli"

#define TILE_SIZE                       4
#define NUM_SLICES                      32
#define MAX_LIGHTS_PER_TILE             1024
#define SHADER_ENTITY_TILE_BUCKET_COUNT (MAX_LIGHTS_PER_TILE / 32)

StructuredBuffer<FLocalLight> g_LightBuffer : register(t6);
StructuredBuffer<uint> PerTileLightMask : REGISTER_T(SLOT_TEX_LIGHT_TILE_MASK);
Texture2D g_DebugHitMapTex : REGISTER_T(SLOT_TEX_DEBUG_HIT_MAP);

Texture2D g_ShadowMap2D0 : register(t20);
Texture2D g_ShadowMap2D1 : register(t21);
Texture2D g_ShadowMap2D2 : register(t22);
Texture2D g_ShadowMap2D3 : register(t23);
Texture2D g_ShadowMap2D4 : register(t24);

TextureCube g_ShadowMapCube0 : register(t25);
TextureCube g_ShadowMapCube1 : register(t26);
TextureCube g_ShadowMapCube2 : register(t27);
TextureCube g_ShadowMapCube3 : register(t28);
TextureCube g_ShadowMapCube4 : register(t29);

cbuffer LightCullingParams : register(b2)
{
    uint2 ScreenSize;
    uint2 TileSize;

    uint Enable25DCulling;
    float NearZ;
    float FarZ;
    float NumLights;
}

static const float kShadowBias = 0.002f; // �ϴ� �ϵ��ڵ��� bias�� ������
static const float2 kShadowTexelSize = float2(1.0f / 2048.0f, 1.0f / 2048.0f); // ���������� �ٲ��ߵ�

float3 GetAmbientLightColor()
{
    return Ambient.Color * Ambient.Intensity;
}

// Get Shadow Factor from single Texture2D (Directional or Spot light)
float GetShadowFactor(int ShadowIndex, float4x4 ShadowViewProj, float3 WorldPos)
{
    if (ShadowIndex < 0 || ShadowIndex >= 5) return 1.0f;

    float4 ShadowPos = mul(float4(WorldPos, 1.0f), ShadowViewProj);
    ShadowPos.xyz /= ShadowPos.w;

    if (ShadowPos.x < -1.0f || ShadowPos.x > 1.0f ||
        ShadowPos.y < -1.0f || ShadowPos.y > 1.0f ||
        ShadowPos.z < 0.0f || ShadowPos.z > 1.0f)
    {
        return 1.0f;
    }

    float2 ShadowUV = ShadowPos.xy * 0.5f + 0.5f;
    ShadowUV.y = 1.0f - ShadowUV.y;

    float CompareDepth = ShadowPos.z + kShadowBias;

    float ShadowFactor = 1.0f;
    [branch]
    switch (ShadowIndex)
    {
    case 0: ShadowFactor = g_ShadowMap2D0.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 1: ShadowFactor = g_ShadowMap2D1.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 2: ShadowFactor = g_ShadowMap2D2.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 3: ShadowFactor = g_ShadowMap2D3.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 4: ShadowFactor = g_ShadowMap2D4.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    }

    return ShadowFactor;
}

float SampleSpotShadowCmp(int ShadowIndex, float3 ShadowPosNDC)
{
    if (ShadowIndex < 0 || ShadowIndex >= 5) return 1.0f;

    float2 ShadowUV = ShadowPosNDC.xy;
    float CompareDepth = ShadowPosNDC.z - kShadowBias;

    float ShadowFactor = 1.0f;
    [branch]
    switch (ShadowIndex)
    {
    case 0: ShadowFactor = g_ShadowMap2D0.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 1: ShadowFactor = g_ShadowMap2D1.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 2: ShadowFactor = g_ShadowMap2D2.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 3: ShadowFactor = g_ShadowMap2D3.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    case 4: ShadowFactor = g_ShadowMap2D4.SampleCmpLevelZero(ShadowSampler, ShadowUV, CompareDepth); break;
    }

    return ShadowFactor;
}

float OffsetLookupSpotPCF(int ShadowIndex, float3 ShadowPosNDC, float2 Offset)
{
    float3 OffsetShadowPos = ShadowPosNDC;
    OffsetShadowPos.xy += Offset * kShadowTexelSize;

    if (OffsetShadowPos.x < 0.0f || OffsetShadowPos.x > 1.0f ||
        OffsetShadowPos.y < 0.0f || OffsetShadowPos.y > 1.0f)
    {
        return 1.0f;
    }

    return SampleSpotShadowCmp(ShadowIndex, OffsetShadowPos);
}

float PCF_NvidiaOptimizedSpot(int ShadowIndex, float3 ShadowPosNDC, float4 PixelPos)
{
    float2 Offset = (float2)(frac(PixelPos.xy * 0.5f) > 0.25f);
    Offset.y += Offset.x;

    if (Offset.y > 1.1f)
    {
        Offset.y = 0.0f;
    }

    float ShadowCoeff = 0.0f;
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, ShadowPosNDC, Offset + float2(-1.5f, 0.5f));
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, ShadowPosNDC, Offset + float2(0.5f, 0.5f));
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, ShadowPosNDC, Offset + float2(-1.5f, -1.5f));
    ShadowCoeff += OffsetLookupSpotPCF(ShadowIndex, ShadowPosNDC, Offset + float2(0.5f, -1.5f));

    return ShadowCoeff * 0.25f;
}

// Get Shadow Factor from Cubemap (Point light)
float GetPointShadowFactor(int ShadowIndex, float3 LightPos, float3 WorldPos, float Radius)
{
    if (ShadowIndex < 0 || ShadowIndex >= 5) return 1.0f;

    float3 L = WorldPos - LightPos;
    float ZView = max(abs(L.x), max(abs(L.y), abs(L.z)));

    float N = 1.0f;
    float F = Radius;
    if (F <= N) F = N + 100.0f;

    float PostProjDepth = N / (N - F) - (F * N / (N - F)) / ZView;

    float CompareDepth = PostProjDepth + kShadowBias;

    float ShadowFactor = 1.0f;
    [branch]
    switch (ShadowIndex)
    {
    case 0: ShadowFactor = g_ShadowMapCube0.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
    case 1: ShadowFactor = g_ShadowMapCube1.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
    case 2: ShadowFactor = g_ShadowMapCube2.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
    case 3: ShadowFactor = g_ShadowMapCube3.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
    case 4: ShadowFactor = g_ShadowMapCube4.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
    }

    return ShadowFactor;
}

float3 ReconstructWorldPositionFromSceneDepth(float2 UV)
{
    float Depth = SceneDepth.Sample(PointClampSampler, UV).r;
    float4 Clip = float4(UV * 2.0f - 1.0f, Depth, 1.0f);
    Clip.y *= -1.0f;
    float4 World = mul(Clip, InvViewProj);
    return World.xyz / max(World.w, 0.0001f);
}

float3 LocalLightLambertTerm(FLocalLight LocalLight, float3 N, float3 WorldPosition)
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
        Shadow = GetPointShadowFactor(LocalLight.ShadowMapIndex, LocalLight.Position, WorldPosition, LocalLight.AttenuationRadius);
    }
    else                            // Spot light
    {
        float3 SpotDir = normalize(LocalLight.Direction);
        Attenuation *= smoothstep(
            cos(radians(LocalLight.OuterConeAngle)),
            cos(radians(LocalLight.InnerConeAngle)),
            dot(-L, SpotDir));
        Shadow = GetShadowFactor(LocalLight.ShadowMapIndex, LocalLight.ShadowViewProj, WorldPosition);
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
        Shadow = GetPointShadowFactor(LocalLight.ShadowMapIndex, LocalLight.Position, WorldPosition, LocalLight.AttenuationRadius);
    }
    else                            // Spot light
    {
        float3 SpotDir = normalize(LocalLight.Direction);
        Attenuation *= smoothstep(
            cos(radians(LocalLight.OuterConeAngle)),
            cos(radians(LocalLight.InnerConeAngle)),
            dot(-L, SpotDir));

        float4 ShadowPos = mul(float4(WorldPosition, 1.0f), LocalLight.ShadowViewProj);
        ShadowPos.xyz /= ShadowPos.w;

        if (ShadowPos.x < -1.0f || ShadowPos.x > 1.0f ||
            ShadowPos.y < -1.0f || ShadowPos.y > 1.0f ||
            ShadowPos.z < 0.0f || ShadowPos.z > 1.0f)
        {
            Shadow = 1.0f;
        }
        else
        {
            float3 ShadowPosNDC = 0.0f;
            ShadowPosNDC.xy = ShadowPos.xy * 0.5f + 0.5f;
            ShadowPosNDC.y = 1.0f - ShadowPosNDC.y;
            ShadowPosNDC.z = ShadowPos.z;
            Shadow = PCF_NvidiaOptimizedSpot(LocalLight.ShadowMapIndex, ShadowPosNDC, PixelPos);
        }
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

float3 ComputeGouraudLightingColor(float3 Normal, float3 WorldPosition)
{
    float3 N = normalize(Normal);
    float3 TotalLight = GetAmbientLightColor();

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 L = normalize(Directional[i].Direction);
        float Diffuse = saturate(dot(N, -L));
        float Shadow = GetShadowFactor(Directional[i].ShadowMapIndex, Directional[i].ShadowViewProj, WorldPosition);
        TotalLight += Diffuse * Directional[i].Color * Directional[i].Intensity * Shadow;
    }

    for (int j = 0; j < NumLocalLights; ++j)
    {
        TotalLight += LocalLightLambertTerm(g_LightBuffer[j], N, WorldPosition);
    }

    return saturate(TotalLight);
}

float4 ComputeLambertLighting(float4 BaseColor, float3 Normal, float3 WorldPosition)
{
    float3 N = normalize(Normal);
    float3 TotalLight = GetAmbientLightColor();

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 L = normalize(Directional[i].Direction);
        float Diffuse = saturate(dot(N, -L));
        float Shadow = GetShadowFactor(Directional[i].ShadowMapIndex, Directional[i].ShadowViewProj, WorldPosition);
        TotalLight += Diffuse * Directional[i].Color * Directional[i].Intensity * Shadow;
    }

    for (int j = 0; j < NumLocalLights; ++j)
    {
        TotalLight += LocalLightLambertTerm(g_LightBuffer[j], N, WorldPosition);
    }

    return float4(BaseColor.rgb * saturate(TotalLight), BaseColor.a);
}

float3 ComputeLambertGlobalLight(float3 Normal)
{
    float3 N = normalize(Normal);
    float3 TotalLight = GetAmbientLightColor();

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 L = normalize(Directional[i].Direction);
        TotalLight += saturate(dot(N, -L)) * Directional[i].Color * Directional[i].Intensity;
    }

    return TotalLight;
}

float4 ComputeLambertLightingGlobalOnly(float4 BaseColor, float3 Normal)
{
    return float4(BaseColor.rgb * saturate(ComputeLambertGlobalLight(Normal)), BaseColor.a);
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
        float Shadow = GetShadowFactor(Directional[i].ShadowMapIndex, Directional[i].ShadowViewProj, WorldPosition);

        TotalDiffuse += Diffuse * LightColor * Shadow;
        TotalSpecular += Specular * LightColor * Shadow;
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

FLocalBlinnPhongTerm ComputeBlinnPhongGlobalLight(float3 Normal, float4 MaterialParam, float3 ViewDirection)
{
    FLocalBlinnPhongTerm Out;
    Out.Diffuse = GetAmbientLightColor();
    Out.Specular = 0.0f;

    float3 N = normalize(Normal);
    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 L = normalize(-Directional[i].Direction);
        float3 H = normalize(ViewDirection + L);

        float Diffuse = saturate(dot(N, L));
        float Specular = pow(saturate(dot(N, H)), Shininess) * SpecularStrength;

        float3 LightColor = Directional[i].Color * Directional[i].Intensity;
        Out.Diffuse += Diffuse * LightColor;
        Out.Specular += Specular * LightColor;
    }

    return Out;
}

float4 ComputeBlinnPhongLightingGlobalOnly(float4 BaseColor, float3 Normal, float4 MaterialParam, float3 ViewDirection)
{
    FLocalBlinnPhongTerm GlobalTerm = ComputeBlinnPhongGlobalLight(Normal, MaterialParam, ViewDirection);
    return float4(BaseColor.rgb * saturate(GlobalTerm.Diffuse) + GlobalTerm.Specular * 0.2f, BaseColor.a);
}

#endif


