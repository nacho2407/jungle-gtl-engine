#ifndef SHADOW_FUNCTION_H
#define SHADOW_FUNCTION_H
#define SHADOW_LIGHT_DIRECTIONAL 0u
#define SHADOW_LIGHT_POINT       1u
#define SHADOW_LIGHT_SPOT        2u

#define SHADOW_MAP_CSM_TYPE 0u
#define SHADOW_MAP_PSM_TYPE 1u

#include "../Common/Common.hlsli"
#include "../Common/Lighting.hlsli"

uint SelectPointFace(float3 dir)
{
    float3 a = abs(dir);
    
    if (a.x >= a.y && a.x >= a.z)
    {
        return dir.x >= 0 ? 0u : 1u; // +X, -X
    }
    else if (a.y >= a.x && a.y >= a.z)
    {
        return dir.y >= 0 ? 2u : 3u; // +Y, -Y
    }
    else
    {
        return dir.z >= 0 ? 4u : 5u; // +Z, -Z
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

    float2 texelSize = 1.0 / shadowMapResolution; // ?섎뱶肄붾뵫 ?쒓굅

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
// VSM (Variance Shadow Map) - Chebyshev ?곹븳 湲곕컲 洹몃┝??怨꾩궛
//
// VSM RTV?먮뒗 float2(depth, depth*depth) 媛 湲곕줉?섏뼱 ?덉쓬
//   M1 = moments.r  = E[x]   (?됯퇏 源딆씠)
//   M2 = moments.g  = E[x짼]  (?됯퇏 ?쒓낢 源딆씠)
//
// 遺꾩궛: Var = M2 - M1짼
//   - Var媛 ?댁닔濡??대떦 ?띿? ?덉뿉 源딆씠 蹂?붽? 留롫떎 (寃쎄퀎遺)
//   - Var媛 ?묒쓣?섎줉 洹좎씪???쒕㈃
//
// Chebyshev ?곹븳: P(occluder >= t) <= Var / (Var + (t - M1)짼)
//   - t  : ?꾩옱 ?쎌???light-space depth
//   - 諛섑솚媛믪씠 ?댁닔濡?"lit??媛?μ꽦???믩떎"
//
// t <= M1 ?대㈃ ?꾩옱 ?쎌????됯퇏 occluder蹂대떎 媛源뚯? ???꾩쟾 lit (1.0)
// t >  M1 ?대㈃ Chebyshev 媛믪쑝濡?遺?쒕윭??洹몃┝??怨꾩궛
// ---------------------------------------------------------------------------
float ComputeShadowVSM(
    float3      lightSpacePos,   // projCoords (NDC xyz, z???꾩옱 ?쎌? depth)
    float4      ScaleOffset,     // ?꾪??쇱뒪 ScaleOffset (?⑥씪 議곕챸?대㈃ (1,1,0,0))
    Texture2D   vsmMap,          // VSM RTV 寃곌낵 ?띿뒪泥?(float2: moment1, moment2)
    SamplerState vsmSampler,     // 鍮꾧탳 ?녿뒗 ?쇰컲 sampler (Linear 沅뚯옣)
    float       minVariance      // light bleeding 諛⑹???理쒖냼 遺꾩궛媛?(?? 0.00001)
)
{
    // 1. UV 蹂??(NDC ??[0,1] ???꾪??쇱뒪 ?곸슜)
    float2 uv = lightSpacePos.xy * float2(0.5f, -0.5f) + 0.5f;
    uv = ScaleOffset.xy * uv + ScaleOffset.zw;

    // 2. Moment ?섑뵆留?
    float2 moments = vsmMap.Sample(vsmSampler, uv).rg;
    float M1 = moments.r; // E[x]
    float M2 = moments.g; // E[x짼]

    float t = lightSpacePos.z; // ?꾩옱 ?쎌???light-space depth

    // 3. ?꾩쟾 lit ?먯젙: ?섏떊?먯씠 ?됯퇏 occluder蹂대떎 媛源앷굅??媛숈쑝硫?洹몃┝???놁쓬
    if (t <= M1)
        return 1.0f;

    // 4. 遺꾩궛 怨꾩궛: Var = E[x짼] - E[x]짼
    //    minVariance濡?clamp?댁꽌 ?섏튂 遺덉븞??諛?light bleeding ?꾪솕
    float variance = max(M2 - M1 * M1, minVariance);

    // 5. Chebyshev ?곹븳: p_max = Var / (Var + (t - M1)짼)
    float delta    = t - M1;
    float pMax     = variance / (variance + delta * delta);

    return pMax;
}


float ComputeShadowAtlas(
    uint lightIndex,
    float4 worldPos,
    SamplerComparisonState shadowSampler,
    Texture2D shadowMap,
    SamplerState linearSampler,
    TextureCubeArray<float> pointShadowCube
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

        if (light.ShadowTextureIndex == INVALID_SHADOW_INDEX)
        {
            return 1.0f;
        }
        
        uint faceIndex = SelectPointFace(dir);
        if (faceIndex >= shadowIndices.IndexCount)
        {
            return 1.0f;
        }

        shadowData = AtlasShadowDatas[shadowIndices.ShadowIndex + faceIndex];

        float4 shadowCoord = mul(worldPos, shadowData.ShadowViewProj);
        if (abs(shadowCoord.w) < 1e-5f)
        {
            return 1.0f;
        }

        float3 projCoords = shadowCoord.xyz / shadowCoord.w;
        if (projCoords.z < 0.0f || projCoords.z > 1.0f)
        {
            return 1.0f;
        }

        float3 sampleDir = normalize(dir);
        float storedDepth = pointShadowCube.SampleLevel(
            linearSampler,
            float4(sampleDir, (float)light.ShadowTextureIndex),
            0).r;

        return (projCoords.z - shadowData.ConstantBias) <= storedDepth ? 1.0f : 0.0f;
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
