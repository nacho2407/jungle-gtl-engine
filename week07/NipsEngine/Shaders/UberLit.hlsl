#ifndef UBER_LIT_HLSL
#define UBER_LIT_HLSL

#include "Common.hlsl"

#if !defined(LIGHTING_MODEL_UNLIT) && !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_GOURAUD) && !defined(LIGHTING_MODEL_PHONG)
    #define LIGHTING_MODEL_LAMBERT 1
#endif

#ifndef USE_NORMAL_MAP
    #define USE_NORMAL_MAP 0
#endif

#ifndef USE_DECAL_LIGHTING
    #define USE_DECAL_LIGHTING 0
#endif

struct FUberMaterialInput
{
    float3 Albedo;
    float3 Emissive;
    float3 SpecularColor;
    float  Shininess;
};

struct FUberLightingInput
{
    float3 WorldPos;
    float3 WorldNormal;
    float3 ViewDir;
};

struct FUberLightingResult
{
    float3 Color;
};

float3 GetSafeNormal(float3 v)
{
    float lenSq = dot(v, v);
    return (lenSq > 1e-8f) ? normalize(v) : float3(0.0f, 0.0f, 1.0f);
}

float3 CalculateAmbientTerm(FUberMaterialInput materialInput)
{
    return materialInput.Albedo * AmbientLight.Color.rgb * AmbientLight.Intensity;
}

float3 CalculateDiffuseTerm(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    float3 N = GetSafeNormal(lightingInput.WorldNormal);
    float3 L = GetSafeNormal(-DirectionalLight.Direction);
    float NdotL = saturate(dot(N, L));
    
    if (NdotL <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);

    return materialInput.Albedo * DirectionalLight.Color.rgb * DirectionalLight.Intensity * NdotL;
}

float3 CalculateSpecularTerm(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    // Blinn-Phong Model
    float3 N = GetSafeNormal(lightingInput.WorldNormal);
    float3 L = GetSafeNormal(-DirectionalLight.Direction);
    float3 V = GetSafeNormal(lightingInput.ViewDir);
    float3 H = GetSafeNormal(L + V);

    float NdotH = saturate(dot(N, H));
    float specularPower = max(materialInput.Shininess, 1.0f);
    float specular = pow(NdotH, specularPower);

    return materialInput.SpecularColor * DirectionalLight.Color.rgb * DirectionalLight.Intensity * specular;
}

float CalculateAttenuation(float3 lightPosition, float radius, float falloffExponent, float3 worldPos)
{
    float safeRadius = max(radius, 1e-4f);
    float distanceToLight = distance(lightPosition, worldPos);
    float normalizedFalloff = saturate(1.0f - (distanceToLight / safeRadius));
    return pow(normalizedFalloff, max(falloffExponent, 1e-4f));
}

float CalculateConeAttenuation(FSpotLightInfo light, float3 worldPos)
{
    float3 lightToSurface = GetSafeNormal(worldPos - light.Position);
    float coneCos = dot(GetSafeNormal(light.Direction), lightToSurface);
    float coneRange = max(light.InnerConeCos - light.OuterConeCos, 1e-4f);
    return saturate((coneCos - light.OuterConeCos) / coneRange);
}

float3 CalculatePointDiffuseTerm(FPointLightInfo light, FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    float3 N = GetSafeNormal(lightingInput.WorldNormal);
    float3 L = GetSafeNormal(light.Position - lightingInput.WorldPos);
    float attenuation = CalculateAttenuation(light.Position, light.Radius, light.FalloffExponent, lightingInput.WorldPos);
    float NdotL = saturate(dot(N, L));

    return materialInput.Albedo * light.Color.rgb * light.Intensity * attenuation * NdotL;
}

float3 CalculatePointSpecularTerm(FPointLightInfo light, FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    float3 N = GetSafeNormal(lightingInput.WorldNormal);
    float3 L = GetSafeNormal(light.Position - lightingInput.WorldPos);
    float3 V = GetSafeNormal(lightingInput.ViewDir);
    float3 H = GetSafeNormal(L + V);
    float attenuation = CalculateAttenuation(light.Position, light.Radius, light.FalloffExponent, lightingInput.WorldPos);

    float NdotH = saturate(dot(N, H));
    float specular = pow(NdotH, max(materialInput.Shininess, 1.0f));

    return materialInput.SpecularColor * light.Color.rgb * light.Intensity * attenuation * specular;
}

float3 CalculateSpotDiffuseTerm(FSpotLightInfo light, FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    float3 N = GetSafeNormal(lightingInput.WorldNormal);
    float3 L = GetSafeNormal(light.Position - lightingInput.WorldPos);
    float distanceAttenuation = CalculateAttenuation(
        light.Position, light.Radius, light.FalloffExponent, lightingInput.WorldPos);
    float coneAttenuation = CalculateConeAttenuation(light, lightingInput.WorldPos);
    float NdotL = saturate(dot(N, L));

    return materialInput.Albedo * light.Color.rgb * light.Intensity * distanceAttenuation * coneAttenuation * NdotL;
}

float3 CalculateSpotSpecularTerm(FSpotLightInfo light, FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    float3 N = GetSafeNormal(lightingInput.WorldNormal);
    float3 L = GetSafeNormal(light.Position - lightingInput.WorldPos);
    float3 V = GetSafeNormal(lightingInput.ViewDir);
    float3 H = GetSafeNormal(L + V);
    float distanceAttenuation = CalculateAttenuation(
        light.Position, light.Radius, light.FalloffExponent, lightingInput.WorldPos);
    float coneAttenuation = CalculateConeAttenuation(light, lightingInput.WorldPos);

    float NdotH = saturate(dot(N, H));
    float specular = pow(NdotH, max(materialInput.Shininess, 1.0f));

    return materialInput.SpecularColor * light.Color.rgb * light.Intensity * distanceAttenuation * coneAttenuation * specular;
}

float3 AccumulatePointDiffuseTerms(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    float3 accumulated = float3(0.0f, 0.0f, 0.0f);
    [loop]
    for (uint i = 0; i < PointLightCount; ++i)
    {
        accumulated += CalculatePointDiffuseTerm(PointLights[i], materialInput, lightingInput);
    }
    return accumulated;
}

float3 AccumulatePointSpecularTerms(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    float3 accumulated = float3(0.0f, 0.0f, 0.0f);
    [loop]
    for (uint i = 0; i < PointLightCount; ++i)
    {
        accumulated += CalculatePointSpecularTerm(PointLights[i], materialInput, lightingInput);
    }
    return accumulated;
}

float3 AccumulateSpotDiffuseTerms(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    float3 accumulated = float3(0.0f, 0.0f, 0.0f);
    [loop]
    for (uint i = 0; i < SpotLightCount; ++i)
    {
        accumulated += CalculateSpotDiffuseTerm(SpotLights[i], materialInput, lightingInput);
    }
    return accumulated;
}

float3 AccumulateSpotSpecularTerms(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    float3 accumulated = float3(0.0f, 0.0f, 0.0f);
    [loop]
    for (uint i = 0; i < SpotLightCount; ++i)
    {
        accumulated += CalculateSpotSpecularTerm(SpotLights[i], materialInput, lightingInput);
    }
    return accumulated;
}

FUberLightingResult EvaluateUnlit(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    FUberLightingResult result;
    result.Color = materialInput.Albedo + materialInput.Emissive;
    return result;
}

FUberLightingResult EvaluateLambert(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    FUberLightingResult result;
    result.Color = 0.0f;

    // 순수 Lambertian 조명 모델은 Diffuse Term만 계산하기 때문에, 우선 Ambient Term은 포함시키지 않음
    result.Color += CalculateDiffuseTerm(materialInput, lightingInput);
    result.Color += AccumulatePointDiffuseTerms(materialInput, lightingInput);
    result.Color += AccumulateSpotDiffuseTerms(materialInput, lightingInput);
    result.Color += materialInput.Emissive;

    return result;
}

FUberLightingResult EvaluatePhong(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    FUberLightingResult result;
    result.Color = 0.0f;

    result.Color += CalculateAmbientTerm(materialInput);
    result.Color += CalculateDiffuseTerm(materialInput, lightingInput);
    result.Color += AccumulatePointDiffuseTerms(materialInput, lightingInput);
    result.Color += AccumulateSpotDiffuseTerms(materialInput, lightingInput);
    result.Color += CalculateSpecularTerm(materialInput, lightingInput);
    result.Color += AccumulatePointSpecularTerms(materialInput, lightingInput);
    result.Color += AccumulateSpotSpecularTerms(materialInput, lightingInput);
    result.Color += materialInput.Emissive;

    return result;
}

FUberLightingResult EvaluateUberLighting(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
#if LIGHTING_MODEL_PHONG || LIGHTING_MODEL_GOURAUD
    return EvaluatePhong(materialInput, lightingInput);
#elif LIGHTING_MODEL_LAMBERT
    return EvaluateLambert(materialInput, lightingInput);
#else
    return EvaluateUnlit(materialInput, lightingInput);
#endif
}

#endif
