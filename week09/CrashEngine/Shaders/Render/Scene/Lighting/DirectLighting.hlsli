/*
    DirectLighting.hlsli는 scene 직접 조명 계산 helper를 제공합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - t20~24: ShadowMap
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp, s3: Shadow
    - u#: Compute/후처리용 UAV
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

// t20 ~ t24: Shadow Maps
TextureCube g_ShadowMap0 : register(t20);
TextureCube g_ShadowMap1 : register(t21);
TextureCube g_ShadowMap2 : register(t22);
TextureCube g_ShadowMap3 : register(t23);
TextureCube g_ShadowMap4 : register(t24);

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

float GetShadowFactor(int ShadowIndex, float4x4 ShadowViewProj, float3 WorldPos)
{
    if (ShadowIndex < 0 || ShadowIndex >= 5) return 1.0f;

    float4 ShadowPos = mul(float4(WorldPos, 1.0f), ShadowViewProj);
    ShadowPos.xyz /= ShadowPos.w;

    if (ShadowPos.x < -1.0f || ShadowPos.x > 1.0f || ShadowPos.y < -1.0f || ShadowPos.y > 1.0f || ShadowPos.z < 0.0f || ShadowPos.z > 1.0f) return 1.0f;

    float2 ShadowUV = ShadowPos.xy * 0.5f + 0.5f;
    ShadowUV.y = 1.0f - ShadowUV.y;
    
    float2 UV_norm = ShadowUV * 2.0f - 1.0f;
    float3 SampleDir = float3(1.0f, -UV_norm.y, -UV_norm.x);

    // Reversed-Z Shadow Bias
    float Bias = 0.002f;
    float CompareDepth = ShadowPos.z + Bias;

    float ShadowFactor = 1.0f;
    [branch] switch(ShadowIndex)
    {
        case 0: ShadowFactor = g_ShadowMap0.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
        case 1: ShadowFactor = g_ShadowMap1.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
        case 2: ShadowFactor = g_ShadowMap2.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
        case 3: ShadowFactor = g_ShadowMap3.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
        case 4: ShadowFactor = g_ShadowMap4.SampleCmpLevelZero(ShadowSampler, SampleDir, CompareDepth); break;
    }
    
    return ShadowFactor;
}

float GetPointShadowFactor(int ShadowIndex, float3 LightPos, float3 WorldPos, float Radius)
{
    if (ShadowIndex < 0 || ShadowIndex >= 5) return 1.0f;

    float3 L = WorldPos - LightPos;
    float z_view = max(abs(L.x), max(abs(L.y), abs(L.z)));
    
    float N = 1.0f;
    float F = Radius;
    if (F <= N) F = N + 100.0f;
    
    float PostProjDepth = N / (N - F) - (F * N / (N - F)) / z_view;

    // Reversed-Z Shadow Bias
    float Bias = 0.005f;
    float CompareDepth = PostProjDepth + Bias;

    float ShadowFactor = 1.0f;
    [branch] switch(ShadowIndex)
    {
        case 0: ShadowFactor = g_ShadowMap0.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
        case 1: ShadowFactor = g_ShadowMap1.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
        case 2: ShadowFactor = g_ShadowMap2.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
        case 3: ShadowFactor = g_ShadowMap3.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
        case 4: ShadowFactor = g_ShadowMap4.SampleCmpLevelZero(ShadowSampler, L, CompareDepth); break;
    }

    return ShadowFactor;
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
        FLocalLight LocalLight = g_LightBuffer[j];
        float3 LightVector = LocalLight.Position - WorldPosition;
        float Distance = length(LightVector);

        if (Distance < LocalLight.AttenuationRadius && LocalLight.AttenuationRadius > 0.001f)
        {
            float3 L = LightVector / Distance;
            float Diffuse = saturate(dot(N, L));
            float Attenuation = saturate(1.0f - (Distance / LocalLight.AttenuationRadius));
            Attenuation *= Attenuation;

            float Shadow = 1.0f;
            if (dot(LocalLight.Direction, LocalLight.Direction) > 0.0001f)
            {
                float3 SpotDir = normalize(LocalLight.Direction);
                float CosAngle = dot(-L, SpotDir);
                float CosInner = cos(radians(LocalLight.InnerConeAngle));
                float CosOuter = cos(radians(LocalLight.OuterConeAngle));
                Attenuation *= smoothstep(CosOuter, CosInner, CosAngle);
                Shadow = GetShadowFactor(LocalLight.ShadowMapIndex, LocalLight.ShadowViewProj, WorldPosition);
            }
            else
            {
                Shadow = GetPointShadowFactor(LocalLight.ShadowMapIndex, LocalLight.Position, WorldPosition, LocalLight.AttenuationRadius);
            }

            TotalLight += Diffuse * LocalLight.Color * LocalLight.Intensity * Attenuation * Shadow;
        }
    }

    return saturate(TotalLight);
}

float3 ReconstructWorldPositionFromSceneDepth(float2 UV)
{
    float Depth = SceneDepth.Sample(PointClampSampler, UV).r;
    float4 Clip = float4(UV * 2.0f - 1.0f, Depth, 1.0f);
    Clip.y *= -1.0f;
    float4 World = mul(Clip, InvViewProj);
    return World.xyz / max(World.w, 0.0001f);
}

float4 ComputeLambertLighting(float4 BaseColor, float3 Normal)
{
    float3 N = normalize(Normal);
    float3 TotalLight = GetAmbientLightColor();

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 L = normalize(Directional[i].Direction);
        TotalLight += saturate(dot(N, -L)) * Directional[i].Color * Directional[i].Intensity;
    }

    return float4(BaseColor.rgb * saturate(TotalLight), BaseColor.a);
}

float4 ComputeBlinnPhongLighting(float4 BaseColor, float3 Normal, float4 MaterialParam, float3 WorldPosition, float3 ViewDirection)
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

    return float4(BaseColor.rgb * saturate(TotalDiffuse) + TotalSpecular * 0.2f, BaseColor.a);
}

FLocalBlinnPhongTerm LocalLightBlinnPhongTerm(
    FLocalLight LocalLight,
    float3 N,
    float3 WorldPosition,
    float3 V,
    float Shininess,
    float SpecularStrength)
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
    if (dot(LocalLight.Direction, LocalLight.Direction) > 0.0001f)
    {
        float3 SpotDir = normalize(LocalLight.Direction);
        Attenuation *= smoothstep(
            cos(radians(LocalLight.OuterConeAngle)),
            cos(radians(LocalLight.InnerConeAngle)),
            dot(-L, SpotDir));
        Shadow = GetShadowFactor(LocalLight.ShadowMapIndex, LocalLight.ShadowViewProj, WorldPosition);
    }
    else
    {
        Shadow = GetPointShadowFactor(LocalLight.ShadowMapIndex, LocalLight.Position, WorldPosition, LocalLight.AttenuationRadius);
    }

    float3 LightColor = LocalLight.Color * LocalLight.Intensity;
    Out.Diffuse  = Diffuse * LightColor * Attenuation * Shadow;
    Out.Specular = Specular * LightColor * 0.2f * Attenuation * Shadow;
    return Out;
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
    if (dot(LocalLight.Direction, LocalLight.Direction) > 0.0001f)
    {
        float3 SpotDir = normalize(LocalLight.Direction);
        Attenuation *= smoothstep(
            cos(radians(LocalLight.OuterConeAngle)),
            cos(radians(LocalLight.InnerConeAngle)),
            dot(-L, SpotDir));
        Shadow = GetShadowFactor(LocalLight.ShadowMapIndex, LocalLight.ShadowViewProj, WorldPosition);
    }
    else
    {
        Shadow = GetPointShadowFactor(LocalLight.ShadowMapIndex, LocalLight.Position, WorldPosition, LocalLight.AttenuationRadius);
    }

    return Diffuse * LocalLight.Color * LocalLight.Intensity * Attenuation * Shadow;
}

#endif
