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
    result.Color += materialInput.Emissive;

    return result;
}

FUberLightingResult EvaluatePhong(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
    FUberLightingResult result;
    result.Color = 0.0f;

    result.Color += CalculateAmbientTerm(materialInput);
    result.Color += CalculateDiffuseTerm(materialInput, lightingInput);
    result.Color += CalculateSpecularTerm(materialInput, lightingInput);
    result.Color += materialInput.Emissive;

    return result;
}

FUberLightingResult EvaluateUberLighting(FUberMaterialInput materialInput, FUberLightingInput lightingInput)
{
#if LIGHTING_MODEL_PHONG
    return EvaluatePhong(materialInput, lightingInput);
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_GOURAUD
    return EvaluateLambert(materialInput, lightingInput);
#else
    return EvaluateUnlit(materialInput, lightingInput);
#endif
}

#endif
