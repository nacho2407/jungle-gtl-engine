#ifndef SHADOW_FUNCTION_H
#define SHADOW_FUNCTION_H

#include "Common.hlsl"

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
    uint shadowIndex = LightShadowIndices[lightIndex];
    if (shadowIndex == INVALID_SHADOW_INDEX)
    {
        return 1.0f;
    }

    FAtlasShadowData shadowData = AtlasShadowDatas[shadowIndex];

    float4 camClip = mul(worldPos, VirtualViewProj);
    if (abs(camClip.w) < 1e-5f)
    {
        return 1.0f;
    }

    float3 post = camClip.xyz / camClip.w;
    float4 shadowCoord = mul(float4(post, 1.0f), shadowData.ShadowViewProj);
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

    return ComputeShadowPCF(projCoords, shadowData.ScaleOffset, shadowData.ShadowSoftness, shadowSampler, shadowMap, shadowData.ShadowBias);
}

#endif
