#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "CommonTypes.hlsli"

// LocalLights StructuredBuffer - t6 slot
StructuredBuffer<FLocalLightInfo> g_LightBuffer : register(t6);

float3 GetAmbientLightColor()
{
    return Ambient.Color * Ambient.Intensity;
}

// 특정 인덱스의 Directional Light 방향 반환
float3 GetDirectionalLightDirection(int Index)
{
    if (Index < NumDirectionalLights)
    {
        return normalize(Directional[Index].Direction);
    }
    return float3(0, 0, 0);
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

float ComputeLambertTerm(float3 Normal, float3 LightDirection)
{
    return saturate(dot(normalize(Normal), -normalize(LightDirection)));
}

float4 ComputeGouraudLighting(float4 BaseColor, float4 GouraudL)
{
    return float4(BaseColor.rgb * GouraudL.rgb, BaseColor.a);
}

float3 ComputeGouraudLightingColor(float3 Normal, float3 WorldPosition)
{
    float3 TotalLight = GetAmbientLightColor();

    // Directional Lights
    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float Diffuse = ComputeLambertTerm(Normal, Directional[i].Direction);
        TotalLight += Diffuse * Directional[i].Color * Directional[i].Intensity;
    }

    // Local Lights (Point / Spot)
    // Note: This requires access to g_LightBuffer, which must be bound to VS for Gouraud
    for (int j = 0; j < NumLocalLights; ++j)
    {
        FLocalLightInfo LocalLight = g_LightBuffer[j];    
        
        float3 LightVector = LocalLight.Position - WorldPosition;
        float Distance = length(LightVector);
        
        if (Distance < LocalLight.AttenuationRadius && LocalLight.AttenuationRadius > 0.001f)
        {
            float3 L = LightVector / Distance;
            float Diffuse = saturate(dot(normalize(Normal), L));
            
            float DistanceFalloff = saturate(1.0f - (Distance / LocalLight.AttenuationRadius));
            DistanceFalloff *= DistanceFalloff;
            
            float SpotFalloff = 1.0f;
            float DirLengthSq = dot(LocalLight.Direction, LocalLight.Direction);
            if (DirLengthSq > 0.0001f)
            {
                float3 SpotDirection = normalize(LocalLight.Direction);
                float CosAngle = dot(-L, SpotDirection);
                float CosInner = cos(radians(LocalLight.InnerConeAngle));
                float CosOuter = cos(radians(LocalLight.OuterConeAngle));
                SpotFalloff = smoothstep(CosOuter, CosInner, CosAngle);
            }
            
            TotalLight += Diffuse * LocalLight.Color * LocalLight.Intensity * DistanceFalloff * SpotFalloff;
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
    float3 TotalLight = GetAmbientLightColor();

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float Diffuse = ComputeLambertTerm(Normal, Directional[i].Direction);
        TotalLight += Diffuse * Directional[i].Color * Directional[i].Intensity;
    }

    float3 LitColor = BaseColor.rgb * saturate(TotalLight);
    return float4(LitColor, BaseColor.a);
}

float4 ComputeBlinnPhongLighting(float4 BaseColor, float3 Normal, float4 MaterialParam, float2 UV)
{
    float3 WorldPosition = ReconstructWorldPositionFromSceneDepth(UV);
    float3 ViewDirection = normalize(CameraWorldPos - WorldPosition);
    
    float3 TotalDiffuse = GetAmbientLightColor();
    float3 TotalSpecular = float3(0, 0, 0);

    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);

    for (int i = 0; i < NumDirectionalLights; ++i)
    {
        float3 LightDirection = normalize(-Directional[i].Direction);
        float3 HalfVector = normalize(ViewDirection + LightDirection);

        float Diffuse = saturate(dot(normalize(Normal), LightDirection));
        float Specular = pow(saturate(dot(normalize(Normal), HalfVector)), Shininess) * SpecularStrength;

        float3 LightColor = Directional[i].Color * Directional[i].Intensity;
        TotalDiffuse += Diffuse * LightColor;
        TotalSpecular += Specular * LightColor;
    }

    float3 FinalColor = BaseColor.rgb * saturate(TotalDiffuse) + TotalSpecular;
    return float4(FinalColor, BaseColor.a);
}

float3 LocalLightBlinnPhong(FLocalLightInfo LocalLight, float3 Normal, float4 BaseColor, float4 MaterialParam, float2 UV)
{
    float3 WorldPosition = ReconstructWorldPositionFromSceneDepth(UV);
    float3 LightVector = LocalLight.Position - WorldPosition;
    float Distance = length(LightVector);

    if (Distance >= LocalLight.AttenuationRadius || LocalLight.AttenuationRadius <= 0.001f)
    {
        return float3(0, 0, 0);
    }

    float3 L = LightVector / Distance;
    float3 V = normalize(CameraWorldPos - WorldPosition);
    float3 H = normalize(V + L);

    float Diffuse = saturate(dot(normalize(Normal), L));
    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);
    float Specular = pow(saturate(dot(normalize(Normal), H)), Shininess) * SpecularStrength;

    float DistanceFalloff = saturate(1.0f - (Distance / LocalLight.AttenuationRadius));
    DistanceFalloff *= DistanceFalloff;

    float SpotFalloff = 1.0f;
    float DirLengthSq = dot(LocalLight.Direction, LocalLight.Direction);
    if (DirLengthSq > 0.0001f)
    {
        float3 SpotDirection = normalize(LocalLight.Direction);
        float CosAngle = dot(-L, SpotDirection);

        float CosInner = cos(radians(LocalLight.InnerConeAngle));
        float CosOuter = cos(radians(LocalLight.OuterConeAngle));
        SpotFalloff = smoothstep(CosOuter, CosInner, CosAngle);
    }

    float3 LightColor = LocalLight.Color * LocalLight.Intensity;
    float Attenuation = DistanceFalloff * SpotFalloff;
    float3 DiffuseColor = BaseColor.rgb * Diffuse * LightColor;
    float3 SpecularColor = LightColor * Specular;
    return (DiffuseColor + SpecularColor) * Attenuation;
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