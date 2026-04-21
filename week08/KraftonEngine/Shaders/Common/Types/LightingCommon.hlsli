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

// 반환 타입을 float3로 유지하여 메인 루프에서 rgb에만 더할 수 있도록 합니다.
float3 LocalLightLambert(FLocalLightInfo LocalLight, float3 Normal, float4 BaseColor, float2 UV)
{
    // 1. 픽셀의 월드 좌표 복원 및 거리 계산
    float3 WorldPosition = ReconstructWorldPositionFromSceneDepth(UV);
    float3 LightVector = LocalLight.Position - WorldPosition;
    float Distance = length(LightVector);
    
    // [최적화] 거리가 감쇠 반경 밖이거나 비정상적인 반경이면 계산 종료
    if (Distance >= LocalLight.AttenuationRadius || LocalLight.AttenuationRadius <= 0.001f)
    {
        return float3(0, 0, 0);
    }
    
    // 2. 빛의 방향(L) 및 Lambert (Diffuse) 계산
    float3 L = LightVector / Distance;
    float Diffuse = saturate(dot(normalize(Normal), L));
    
    // 3. 거리 감쇠 (Distance Attenuation) - Point, Spot 공통 적용
    float DistanceFalloff = saturate(1.0f - (Distance / LocalLight.AttenuationRadius));
    DistanceFalloff *= DistanceFalloff;
    
    // 4. 스팟 라이트 원뿔 감쇠 (Spotlight Cone Falloff)
    float SpotFalloff = 1.0f; // 기본값 1.0 (감쇠 없음 = 포인트 라이트로 동작)
    
    // Direction 벡터의 길이 제곱을 구합니다. (루트 연산을 피하기 위한 최적화)
    float DirLengthSq = dot(LocalLight.Direction, LocalLight.Direction);
    
    // 방향 벡터의 길이가 0보다 크다면 스팟 라이트로 간주하고 계산 수행
    if (DirLengthSq > 0.0001f)
    {
        // 이때는 Direction이 0이 아니므로 안심하고 normalize 할 수 있습니다.
        float3 SpotDirection = normalize(LocalLight.Direction);
        float CosAngle = dot(-L, SpotDirection);
        
        float CosInner = cos(radians(LocalLight.InnerConeAngle));
        float CosOuter = cos(radians(LocalLight.OuterConeAngle));
        
        // 원뿔의 내부 각도와 외부 각도 사이를 부드럽게 보간
        SpotFalloff = smoothstep(CosOuter, CosInner, CosAngle);
    }
    
    // 5. 최종 컬러 출력 (강도, 거리 감쇠, 스팟 감쇠 모두 곱함)
    float3 LightColor = LocalLight.Color * LocalLight.Intensity;
    return BaseColor.rgb * Diffuse * LightColor * DistanceFalloff * SpotFalloff;
}
#endif