#ifndef SHADOW_FUNCTION_H
#define SHADOW_FUNCTION_H
#define SHADOW_LIGHT_DIRECTIONAL 0u
#define SHADOW_LIGHT_POINT       1u
#define SHADOW_LIGHT_SPOT        2u

#define SHADOW_MAP_CSM_TYPE 0u
#define SHADOW_MAP_PSM_TYPE 1u

#include "Common.hlsl"
#include "Lighting.hlsl"

uint SelectPointFace(float3 dir)
{
    float3 a = abs(dir);
    
    if (a.x >= a.y && a.x >= a.z)
    {
        return dir.x < 0 ? 0u : 1u; // -X, +X
    }
    else if (a.y >= a.x && a.y >= a.z)
    {
        return dir.y < 0 ? 2u : 3u; // -Y, +Y
    }
    else
    {
        return dir.z < 0 ? 4u : 5u; // -Z, +Z
    }
}

float ComputeShadowPCF(
    float3 lightSpacePos,
    float4 ScaleOffset,
    int kernelHalfSize,
    SamplerComparisonState shadowSampler,
    Texture2D shadowMap,
    float bias
)
{
    float2 uv = lightSpacePos.xy * float2(0.5, -0.5) + 0.5;
    uv = ScaleOffset.xy * uv + ScaleOffset.zw;
    
    float compareDepth = lightSpacePos.z;
    float2 shadowMapResolution;
    shadowMap.GetDimensions(shadowMapResolution.x, shadowMapResolution.y);

    float2 texelSize = 1.0 / shadowMapResolution; // 하드코딩 제거

    float shadow = 0.0;
    int count = 0;
    
    for (int x = -kernelHalfSize; x <= kernelHalfSize; x++)
    {
        for (int y = -kernelHalfSize; y <= kernelHalfSize; y++)
        {
            shadow += shadowMap.SampleCmpLevelZero(
            shadowSampler,
            uv + float2(x, y) * texelSize,
            compareDepth - bias
            );
            count++;
        }
    }
        

    return shadow / count;
}

float ComputeShadowAtlas(
    uint lightIndex,
    float4 worldPos,
    SamplerComparisonState shadowSampler,
    Texture2D shadowMap
)
{
    FLightShadowIndices shadowIndices = LightShadowIndices[lightIndex];

    if (shadowIndices.ShadowIndex == INVALID_SHADOW_INDEX ||
        shadowIndices.IndexCount == 0)
    {
        return 1.0f;
    }

    FAtlasShadowData shadowData = AtlasShadowDatas[shadowIndices.ShadowIndex];
    
    if (shadowData.ShadowType == SHADOW_LIGHT_POINT)
    {
        LightInfo light = Lights[lightIndex];
        float3 dir = worldPos.xyz - light.Position;
        
        uint faceIndex = SelectPointFace(dir);
        shadowData = AtlasShadowDatas[shadowIndices.ShadowIndex + faceIndex];
    }

    float4 shadowCoord;

    if (shadowData.ShadowMapType == SHADOW_MAP_PSM_TYPE)
    {
        float4 camClip = mul(worldPos, shadowData.VirtualViewProj);
        if (abs(camClip.w) < 1e-5f)
        {
            return 1.0f;
        }

        float3 post = camClip.xyz / camClip.w;
        shadowCoord = mul(float4(post, 1.0f), shadowData.ShadowViewProj);
    }
    else
    {
        shadowCoord = mul(worldPos, shadowData.ShadowViewProj);
    }

    if (abs(shadowCoord.w) < 1e-5f)
    {
        return 1.0f;
    }

    float3 projCoords = shadowCoord.xyz / shadowCoord.w;

    float2 shadowUV = float2(
        projCoords.x * 0.5f + 0.5f,
       -projCoords.y * 0.5f + 0.5f
    );

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f;
    }

    int kernel = clamp((int) shadowData.ShadowSoftness, 0, 4);

    return ComputeShadowPCF(
        projCoords,
        shadowData.ScaleOffset,
        kernel,
        shadowSampler,
        shadowMap,
        shadowData.ConstantBias
    );
}
#endif
