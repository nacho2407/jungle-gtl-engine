#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "CommonTypes.hlsli"

#define TILE_SIZE                       4
#define NUM_SLICES                      32
#define MAX_LIGHTS_PER_TILE             1024
#define SHADER_ENTITY_TILE_BUCKET_COUNT (MAX_LIGHTS_PER_TILE / 32)   // 32

// LocalLights StructuredBuffer - t6 slot
StructuredBuffer<FLocalLightInfo> g_LightBuffer : register(t6);
//Lighting Masking Buffers
StructuredBuffer<uint> PerTileLightMask : register(t7);
Texture2D g_DebugHitMapTex : register(t8);
//Light Culling Parameters - b2 slot
cbuffer LightCullingParams : register(b2)
{
    uint2 ScreenSize;
    uint2 TileSize; // == TILE_SIZE

    uint Enable25DCulling;
    float NearZ;
    float FarZ;
    float NumLights;
};

struct FLocalBlinnPhongTerm
{
    float3 Diffuse;
    float3 Specular;
};

float3 GetAmbientLightColor()
{
    return Ambient.Color * Ambient.Intensity;
}

// 특정 인덱스의 Directional Light 방향 반환
float3 GetDirectionalLightDirection(int Index)
{
    if (Index < NumDirectionalLights)
    {
        float3 Dir = Directional[Index].Direction;
        return (length(Dir) > 0.0001f) ? normalize(Dir) : float3(0, -1, 0);
    }
    return float3(0, -1, 0);
}

// 특정 인덱스의 Directional Light 색상 반환
float3 GetDirectionalLightColor(int Index)
{
    if (Index < NumDirectionalLights)
    {
        return Directional[Index].Color * Directional[Index].Intensity;
    }
    return float3(0, 0, 0);
}

float ComputeLambertTerm(float3 Normal, float3 L)
{
    // Normal과 L은 호출부에서 정규화되어 넘어온다고 가정 (성능 최적화)
    return saturate(dot(Normal, -L));
}

float4 ComputeGouraudLighting(float4 BaseColor, float4 GouraudL)
{
    return float4(BaseColor.rgb * GouraudL.rgb, BaseColor.a);
}

float3 ComputeGouraudLightingColor(float3 Normal, float3 WorldPosition)
{

    float3 N = normalize(Normal);
    float3 TotalLight = GetAmbientLightColor();

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 L = normalize(Directional[i].Direction);
        float Diffuse = saturate(dot(N, -L));
        TotalLight += Diffuse * Directional[i].Color * Directional[i].Intensity;
    }

    for (int j = 0; j < NumLocalLights; ++j)
    {
        FLocalLightInfo LocalLight = g_LightBuffer[j];
        float3 LightVector = LocalLight.Position - WorldPosition;
        float Distance = length(LightVector);
        
        if (Distance < LocalLight.AttenuationRadius && LocalLight.AttenuationRadius > 0.001f)
        {
            float3 L = LightVector / Distance;
            float Diffuse = saturate(dot(N, L));
            float Attenuation = saturate(1.0f - (Distance / LocalLight.AttenuationRadius));
            Attenuation *= Attenuation;
            
            if (dot(LocalLight.Direction, LocalLight.Direction) > 0.0001f)
            {
                float3 SpotDir = normalize(LocalLight.Direction);
                float CosAngle = dot(-L, SpotDir);
                float CosInner = cos(radians(LocalLight.InnerConeAngle));
                float CosOuter = cos(radians(LocalLight.OuterConeAngle));
                Attenuation *= smoothstep(CosOuter, CosInner, CosAngle);
            }
            
            TotalLight += Diffuse * LocalLight.Color * LocalLight.Intensity * Attenuation;
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
        TotalDiffuse += Diffuse * LightColor;
        TotalSpecular += Specular * LightColor;
    }

    return float4(BaseColor.rgb * saturate(TotalDiffuse) + TotalSpecular * 0.2, BaseColor.a);
}

FLocalBlinnPhongTerm LocalLightBlinnPhongTerm(
    FLocalLightInfo LocalLight,
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

    if (dot(LocalLight.Direction, LocalLight.Direction) > 0.0001f)
    {
        float3 SpotDir = normalize(LocalLight.Direction);
        Attenuation *= smoothstep(
            cos(radians(LocalLight.OuterConeAngle)),
            cos(radians(LocalLight.InnerConeAngle)),
            dot(-L, SpotDir));
    }

    float3 LightColor = LocalLight.Color * LocalLight.Intensity;
    Out.Diffuse  = Diffuse  * LightColor * Attenuation;
    Out.Specular = Specular * LightColor * 0.2f * Attenuation;
    return Out;
}


float3 LocalLightLambertTerm(FLocalLightInfo LocalLight, float3 N, float3 WorldPosition)
{
    float3 LightVector = LocalLight.Position - WorldPosition;
    float Distance = length(LightVector);

    if (Distance >= LocalLight.AttenuationRadius || LocalLight.AttenuationRadius <= 0.001f)
        return 0;

    float3 L = LightVector / Distance;
    float Diffuse = saturate(dot(N, L));

    float Attenuation = saturate(1.0f - (Distance / LocalLight.AttenuationRadius));
    Attenuation *= Attenuation;

    if (dot(LocalLight.Direction, LocalLight.Direction) > 0.0001f)
    {
        float3 SpotDir = normalize(LocalLight.Direction);
        Attenuation *= smoothstep(
            cos(radians(LocalLight.OuterConeAngle)),
            cos(radians(LocalLight.InnerConeAngle)),
            dot(-L, SpotDir));
    }

    return Diffuse * LocalLight.Color * LocalLight.Intensity * Attenuation;
}
#endif