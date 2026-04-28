/*
    DirectLighting.hlsli: scene direct lighting helpers.
*/

#ifndef DIRECT_LIGHTING_HLSLI
#define DIRECT_LIGHTING_HLSLI

#include "../../../Resources/SystemResources.hlsl"
#include "../../../Resources/SystemSamplers.hlsl"
#include "LightTypes.hlsli"
#include "BRDF.hlsli"
#include "ShadowFiltering.hlsli"

#define TILE_SIZE                       4
#define NUM_SLICES                      32
#define MAX_LIGHTS_PER_TILE             1024
#define SHADER_ENTITY_TILE_BUCKET_COUNT (MAX_LIGHTS_PER_TILE / 32)

StructuredBuffer<FLocalLight> g_LightBuffer : register(t6);
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

float3 GetAmbientLightColor()
{
    return Ambient.Color * Ambient.Intensity;
}

#define SHADOW_PROJECTION_ORTHOGRAPHIC 0
#define SHADOW_PROJECTION_PERSPECTIVE 1

float LinearizeReversedZPerspectiveDepth01(float ReverseDepth, float NearZ, float FarZ)
{
    const float Denominator = max(NearZ + ReverseDepth * (FarZ - NearZ), 1e-5f);
    const float ViewZ = (NearZ * FarZ) / Denominator;
    return saturate((ViewZ - NearZ) / max(FarZ - NearZ, 1e-5f));
}

float ComputeLinearShadowCompareDepth(float ReverseDepth, uint ShadowProjectionType, float ShadowNearZ, float ShadowFarZ)
{
    if (ShadowProjectionType == SHADOW_PROJECTION_PERSPECTIVE)
    {
        return LinearizeReversedZPerspectiveDepth01(ReverseDepth, ShadowNearZ, ShadowFarZ);
    }

    return saturate(1.0f - ReverseDepth);
}

float GetShadowFactor(FShadowAtlasSample ShadowSample, float4x4 ShadowViewProj, float3 WorldPos, float3 Normal, float3 LightDir, float Bias, float SlopeBias, float NormalBias, float4 PixelPos, uint ShadowProjectionType, float ShadowNearZ, float ShadowFarZ)
{
    if (ShadowSample.PageIndex < 0)
        return 1.0f;

    // Apply Normal Bias: Push world position along normal to reduce shadow acne
    float3 BiasedWorldPos = WorldPos + Normal * NormalBias;
    
    float4 ShadowPos = mul(float4(BiasedWorldPos, 1.0f), ShadowViewProj);
    ShadowPos.xyz /= ShadowPos.w;

    if (abs(ShadowPos.x) > 1.0f || abs(ShadowPos.y) > 1.0f ||
        ShadowPos.z < 0.0f || ShadowPos.z > 1.0f)
    {
        return 1.0f;
    }

    float2 ShadowVector = ShadowPos.xy;

    // Apply slope-scaled depth bias to reduce shadow acne at grazing angles.
    float CosTheta = saturate(dot(Normal, -LightDir));
    float SlopeFactor = sqrt(1.0f - CosTheta * CosTheta) / max(CosTheta, 0.0001f);
    float TotalBias = Bias + saturate(SlopeBias * SlopeFactor);
    // Reversed-Z에서는 더 작은 depth가 더 멀기 때문에, bias는 compare depth를 줄이는 방향으로 적용합니다.
#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_PCF
    float CompareDepth = ShadowPos.z - TotalBias;
#else
    float CompareDepth = ComputeLinearShadowCompareDepth(ShadowPos.z, ShadowProjectionType, ShadowNearZ, ShadowFarZ);
    CompareDepth = saturate(CompareDepth - TotalBias);
#endif

    float2 BaseUV = ShadowVector * 0.5f + 0.5f;
    BaseUV.y = 1.0f - BaseUV.y;
    return FilterShadowAtlas(ShadowSample, BaseUV, CompareDepth, PixelPos);
}

int ResolvePointShadowFaceIndex(float3 L)
{
    float3 AbsL = abs(L);
    if (AbsL.x >= AbsL.y && AbsL.x >= AbsL.z)
    {
        return (L.x >= 0.0f) ? 0 : 1;
    }
    if (AbsL.y >= AbsL.x && AbsL.y >= AbsL.z)
    {
        return (L.y >= 0.0f) ? 2 : 3;
    }
    return (L.z >= 0.0f) ? 4 : 5;
}

float GetPointShadowFactor(FLocalLight LocalLight, float3 WorldPos, float3 Normal, float4 PixelPos)
{
    float3 BiasedWorldPos = WorldPos + Normal * LocalLight.ShadowNormalBias;
    float3 L = BiasedWorldPos - LocalLight.Position;
    if (dot(L, L) <= 1e-6f) return 1.0f;

    const int FaceIndex = ResolvePointShadowFaceIndex(L);
    const FShadowAtlasSample ShadowSample = DecodeShadowSample(LocalLight.ShadowSampleData[FaceIndex][0], LocalLight.ShadowSampleData[FaceIndex][1]);
    return GetShadowFactor(
        ShadowSample,
        LocalLight.ShadowViewProj[FaceIndex],
        WorldPos,
        Normal,
        normalize(-L),
        LocalLight.ShadowBias,
        LocalLight.ShadowSlopeBias,
        LocalLight.ShadowNormalBias,
        PixelPos,
        SHADOW_PROJECTION_PERSPECTIVE,
        1.0f,
        LocalLight.AttenuationRadius);
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
            DecodeShadowSample(LocalLight.ShadowSampleData[0][0], LocalLight.ShadowSampleData[0][1]),
            LocalLight.ShadowViewProj[0],
            WorldPosition,
            N,
            LocalLight.Direction,
            LocalLight.ShadowBias,
            LocalLight.ShadowSlopeBias,
            LocalLight.ShadowNormalBias,
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
            DecodeShadowSample(LocalLight.ShadowSampleData[0][0], LocalLight.ShadowSampleData[0][1]),
            LocalLight.ShadowViewProj[0],
            WorldPosition,
            N,
            LocalLight.Direction,
            LocalLight.ShadowBias,
            LocalLight.ShadowSlopeBias,
            LocalLight.ShadowNormalBias,
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
        float Shadow = GetShadowFactor(
            DecodeShadowSample(Directional[i].ShadowSampleData[0][0], Directional[i].ShadowSampleData[0][1]),
            Directional[i].ShadowViewProj[0],
            WorldPosition,
            N,
            Directional[i].Direction,
            Directional[i].ShadowBias,
            Directional[i].ShadowSlopeBias,
            Directional[i].ShadowNormalBias,
            PixelPos,
            SHADOW_PROJECTION_ORTHOGRAPHIC,
            0.0f,
            1.0f);
        TotalLight += Diffuse * Directional[i].Color * Directional[i].Intensity * Shadow;
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
        float Shadow = GetShadowFactor(
            DecodeShadowSample(Directional[i].ShadowSampleData[0][0], Directional[i].ShadowSampleData[0][1]),
            Directional[i].ShadowViewProj[0],
            WorldPosition,
            N,
            Directional[i].Direction,
            Directional[i].ShadowBias,
            Directional[i].ShadowSlopeBias,
            Directional[i].ShadowNormalBias,
            PixelPos,
            SHADOW_PROJECTION_ORTHOGRAPHIC,
            0.0f,
            1.0f);
        TotalLight += Diffuse * Directional[i].Color * Directional[i].Intensity * Shadow;
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
        float Shadow = GetShadowFactor(
            DecodeShadowSample(Directional[i].ShadowSampleData[0][0], Directional[i].ShadowSampleData[0][1]),
            Directional[i].ShadowViewProj[0],
            WorldPosition,
            N,
            Directional[i].Direction,
            Directional[i].ShadowBias,
            Directional[i].ShadowSlopeBias,
            Directional[i].ShadowNormalBias,
            pixelPos,
            SHADOW_PROJECTION_ORTHOGRAPHIC,
            0.0f,
            1.0f);
        TotalLight += saturate(dot(N, -L)) * Directional[i].Color * Directional[i].Intensity * Shadow;
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
        float Shadow = GetShadowFactor(
            DecodeShadowSample(Directional[i].ShadowSampleData[0][0], Directional[i].ShadowSampleData[0][1]),
            Directional[i].ShadowViewProj[0],
            WorldPosition,
            N,
            Directional[i].Direction,
            Directional[i].ShadowBias,
            Directional[i].ShadowSlopeBias,
            Directional[i].ShadowNormalBias,
            PixelPos,
            SHADOW_PROJECTION_ORTHOGRAPHIC,
            0.0f,
            1.0f);

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
        float Shadow = GetShadowFactor(
            DecodeShadowSample(Directional[i].ShadowSampleData[0][0], Directional[i].ShadowSampleData[0][1]),
            Directional[i].ShadowViewProj[0],
            WorldPosition,
            N,
            Directional[i].Direction,
            Directional[i].ShadowBias,
            Directional[i].ShadowSlopeBias,
            Directional[i].ShadowNormalBias,
            PixelPos,
            SHADOW_PROJECTION_ORTHOGRAPHIC,
            0.0f,
            1.0f);

        Out.Diffuse += Diffuse * LightColor * Shadow;
        Out.Specular += Specular * LightColor * Shadow;
    }

    return Out;
}

float4 ComputeBlinnPhongLightingGlobalOnly(float4 BaseColor, float3 Normal, float4 MaterialParam, float3 ViewDirection, float3 WorldPos, float4 PixelPos)
{
    FLocalBlinnPhongTerm GlobalTerm = ComputeBlinnPhongGlobalLight(Normal, MaterialParam, ViewDirection, WorldPos, PixelPos);
    return float4(BaseColor.rgb * saturate(GlobalTerm.Diffuse) + GlobalTerm.Specular * 0.2f, BaseColor.a);
}

#endif
