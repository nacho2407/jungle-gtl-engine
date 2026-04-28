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

// ---------------------------------------------------------------------------
// VSM (Variance Shadow Map) - Chebyshev 상한 기반 그림자 계산
//
// VSM RTV에는 float2(depth, depth*depth) 가 기록되어 있음
//   M1 = moments.r  = E[x]   (평균 깊이)
//   M2 = moments.g  = E[x²]  (평균 제곱 깊이)
//
// 분산: Var = M2 - M1²
//   - Var가 클수록 해당 텍셀 안에 깊이 변화가 많다 (경계부)
//   - Var가 작을수록 균일한 표면
//
// Chebyshev 상한: P(occluder >= t) <= Var / (Var + (t - M1)²)
//   - t  : 현재 픽셀의 light-space depth
//   - 반환값이 클수록 "lit일 가능성이 높다"
//
// t <= M1 이면 현재 픽셀이 평균 occluder보다 가까움 → 완전 lit (1.0)
// t >  M1 이면 Chebyshev 값으로 부드러운 그림자 계산
// ---------------------------------------------------------------------------
float ComputeShadowVSM(
    float3      lightSpacePos,   // projCoords (NDC xyz, z는 현재 픽셀 depth)
    float4      ScaleOffset,     // 아틀라스 ScaleOffset (단일 조명이면 (1,1,0,0))
    Texture2D   vsmMap,          // VSM RTV 결과 텍스처 (float2: moment1, moment2)
    SamplerState vsmSampler,     // 비교 없는 일반 sampler (Linear 권장)
    float       minVariance      // light bleeding 방지용 최소 분산값 (예: 0.00001)
)
{
    // 1. UV 변환 (NDC → [0,1] → 아틀라스 적용)
    float2 uv = lightSpacePos.xy * float2(0.5f, -0.5f) + 0.5f;
    uv = ScaleOffset.xy * uv + ScaleOffset.zw;

    // 2. Moment 샘플링
    float2 moments = vsmMap.Sample(vsmSampler, uv).rg;
    float M1 = moments.r; // E[x]
    float M2 = moments.g; // E[x²]

    float t = lightSpacePos.z; // 현재 픽셀의 light-space depth

    // 3. 완전 lit 판정: 수신점이 평균 occluder보다 가깝거나 같으면 그림자 없음
    if (t <= M1)
        return 1.0f;

    // 4. 분산 계산: Var = E[x²] - E[x]²
    //    minVariance로 clamp해서 수치 불안정 및 light bleeding 완화
    float variance = max(M2 - M1 * M1, minVariance);

    // 5. Chebyshev 상한: p_max = Var / (Var + (t - M1)²)
    float delta    = t - M1;
    float pMax     = variance / (variance + delta * delta);

    return pMax;
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
