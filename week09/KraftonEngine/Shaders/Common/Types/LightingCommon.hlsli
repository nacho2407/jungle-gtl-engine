#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "CommonTypes.hlsli"

float3 GetMainLightDirection()
{
    return normalize(float3(Directional[0].Direction));
    return normalize(float3(0.4f, -0.8f, 0.2f));
}

float3 GetMainLightColor()
{
    return float3(1.0f, 0.98f, 0.95f);
}

float ComputeLambertTerm(float3 Normal)
{
    return saturate(dot(normalize(Normal), -GetMainLightDirection()));
}

float4 ComputeGouraudLighting(float4 BaseColor, float4 GouraudL)
{
    return float4(BaseColor.rgb * GouraudL.rgb, BaseColor.a);
}

float ComputeGouraudLightingFactor(float3 Normal)
{
    return 0.2f + ComputeLambertTerm(Normal) * 0.8f;
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
    float Diffuse = ComputeLambertTerm(Normal);
    float3 LightColor = GetMainLightColor();
    float3 LitColor = BaseColor.rgb * (0.2f + Diffuse * LightColor);
    return float4(LitColor, BaseColor.a);
}

float4 ComputeBlinnPhongLighting(float4 BaseColor, float3 Normal, float4 MaterialParam, float2 UV)
{
    float3 WorldPosition = ReconstructWorldPositionFromSceneDepth(UV);
    float3 ViewDirection = normalize(CameraWorldPos - WorldPosition);
    float3 LightDirection = normalize(-GetMainLightDirection());
    float3 HalfVector = normalize(ViewDirection + LightDirection);

    float Diffuse = ComputeLambertTerm(Normal);
    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);
    float Specular = pow(saturate(dot(normalize(Normal), HalfVector)), Shininess) * SpecularStrength;

    float3 LightColor = GetMainLightColor();
    float3 DiffuseColor = BaseColor.rgb * (0.2f + Diffuse * LightColor);
    float3 SpecularColor = LightColor * Specular;

    return float4(DiffuseColor + SpecularColor, BaseColor.a);
}

float4 LocalLightLambert(FLocalLightInfo LocalLight, float3 Normal, float4 BaseColor, float2 UV)
{
    // 1. 픽셀의 월드 좌표 복원
    float3 WorldPosition = ReconstructWorldPositionFromSceneDepth(UV);
    
    // 2. 빛의 방향(L)과 거리(Distance) 계산
    float3 LightVector = LocalLight.Position - WorldPosition;
    float Distance = length(LightVector);
    
    // 정규화된 빛의 방향 벡터 (L)
    float3 L = LightVector / Distance;

    float Diffuse = saturate(dot(normalize(Normal), L));
    
    // 4. 거리 감쇠 (Distance Attenuation)
    // 거리가 AttenuationRadius에 가까워질수록 빛이 0으로 부드럽게 사라집니다.
    float DistanceFalloff = saturate(1.0f - (Distance / LocalLight.AttenuationRadius));
    DistanceFalloff *= DistanceFalloff; // 물리적으로 좀 더 자연스러운 역제곱(Inverse Square) 형태의 근사치

    // 5. 스팟 라이트 원뿔 감쇠 (Spotlight Cone Falloff)
    // 픽셀을 향하는 빛의 역방향(-L)과 스팟 라이트가 비추는 방향(Direction)의 내적
    float CosAngle = dot(-L, normalize(LocalLight.Direction));
    float CosInner = cos(LocalLight.InnerConeAngle);
    float CosOuter = cos(LocalLight.OuterConeAngle);
    
    float SpotFalloff = smoothstep(CosOuter, CosInner, CosAngle);
    
    float3 LightColor = LocalLight.Color * LocalLight.Intensity;
    float3 LitColor = BaseColor.rgb * Diffuse * LightColor * DistanceFalloff * SpotFalloff;
    return float4(LocalLight.Color, 1);
    return float4(LitColor, BaseColor.a);
}
#endif