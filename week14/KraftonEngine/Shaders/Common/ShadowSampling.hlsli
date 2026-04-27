#ifndef SHADOW_SAMPLING_HLSLI
#define SHADOW_SAMPLING_HLSLI

// =============================================================================
// ShadowSampling.hlsli — PCF, VSM, Shadow Factor 유틸리티
// =============================================================================
// 의존: ConstantBuffers.hlsli (ShadowBuffer b5, ShadowMapCSM t21, samplers s3/s4)
//       ForwardLightData.hlsli (SpotShadowDatas t24, PointShadowDatas t25)
//
// 사용법:
//   #include "Common/ShadowSampling.hlsli"
//   float shadow = CalcDirectionalShadowFactor(worldPos, viewDepth);
// =============================================================================

// ── Hard Shadow (1-tap comparison) ──────────────────────────────
float SampleShadowHard(Texture2DArray shadowMap, float3 uvw, float compareDepth)
{
    // ShadowComparisonSampler (s3): D3D11_COMPARISON_GREATER_EQUAL (Reversed-Z)
    // compareDepth >= texel → lit (1.0), compareDepth < texel → shadow (0.0)
    return shadowMap.SampleCmpLevelZero(ShadowComparisonSampler, uvw, compareDepth);
}

// ── PCF 3x3 (9-tap) ────────────────────────────────────────────
float SampleShadowPCF3x3(Texture2DArray shadowMap, float3 uvw, float compareDepth, float texelSize)
{
    float shadow = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float3 offset = float3(float(x) * texelSize, float(y) * texelSize, 0.0f);
            shadow += shadowMap.SampleCmpLevelZero(ShadowComparisonSampler, uvw + offset, compareDepth);
        }
    }

    return shadow / 9.0f;
}

// ── PCF 5x5 (25-tap, 고품질) ───────────────────────────────────
float SampleShadowPCF5x5(Texture2DArray shadowMap, float3 uvw, float compareDepth, float texelSize)
{
    float shadow = 0.0f;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            float3 offset = float3(float(x) * texelSize, float(y) * texelSize, 0.0f);
            shadow += shadowMap.SampleCmpLevelZero(ShadowComparisonSampler, uvw + offset, compareDepth);
        }
    }

    return shadow / 25.0f;
}

// ── VSM (Variance Shadow Map) ───────────────────────────────────
// moments = (E[z], E[z^2])   — Reversed-Z: near=1, far=0
float ComputeVSMFactor(float2 moments, float fragDepth)
{
    // Reversed-Z: 가까울수록 depth가 크다
    // fragDepth >= moments.x → 빛에 더 가까움 → lit
    if (fragDepth >= moments.x)
        return 1.0f;

    // Chebyshev 부등식
    float variance = moments.y - (moments.x * moments.x);
    variance = max(variance, 0.00002f); // numerical stability

    float d = moments.x - fragDepth; // Reversed-Z: occluder가 더 큰 값
    float pMax = variance / (variance + d * d);

    // Light bleeding 감소 — 낮은 확률값을 0으로 클램프
    float minAmount = 0.3f;
    pMax = saturate((pMax - minAmount) / (1.0f - minAmount));

    return pMax;
}

float SampleShadowVSM(Texture2DArray shadowMap, float3 uvw, float fragDepth)
{
    // ShadowLinearSampler (s4): bilinear filtering
    float2 moments = shadowMap.SampleLevel(ShadowLinearSampler, uvw, 0).rg;
    return ComputeVSMFactor(moments, fragDepth);
}

// ── Cascade 선택 ────────────────────────────────────────────────
// viewDepth = abs(mul(worldPos, View).z)  (카메라 뷰 공간 깊이)
uint SelectCascade(float viewDepth)
{
    // CascadeSplits.xyzw = [split0, split1, split2, split3]
    // 가장 가까운(해상도 높은) cascade 우선
    for (uint i = 0; i < NumCSMCascades; ++i)
    {
        if (viewDepth < CascadeSplits[i])
            return i;
    }
    return NumCSMCascades - 1;
}

// ── Directional Light Shadow Factor (통합) ──────────────────────
// worldPos:  월드 좌표
// viewDepth: 카메라 뷰 공간 깊이 (abs(viewPos.z))
float CalcDirectionalShadowFactor(float3 worldPos, float viewDepth)
{
    if (NumCSMCascades == 0)
        return 1.0f;

    // cascade 선택
    uint cascade = SelectCascade(viewDepth);

    // 라이트 공간 좌표 계산
    float4 lightSpacePos = mul(float4(worldPos, 1.0f), CSMViewProj[cascade]);
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // NDC [-1,1] → UV [0,1]  (Y 반전)
    float2 shadowUV = projCoords.xy * float2(0.5f, -0.5f) + 0.5f;
    float  fragDepth = projCoords.z;

    // UV 범위 밖이면 그림자 없음
    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        fragDepth < 0.0f  || fragDepth > 1.0f)
        return 1.0f;

    // bias 적용 (Reversed-Z: 가까울수록 큰 값 → bias를 더해야 acne 방지)
    fragDepth += ShadowBias;

    float3 uvw = float3(shadowUV, (float)cascade);
    float texelSize = 1.0f / (float)CSMResolution;

    // FilterMode 분기
    if (ShadowFilterMode == 0) // Hard
    {
        float shadowMapDepth = ShadowMapCSM.SampleLevel(PointClampSampler, uvw, 0).r;
        return fragDepth < shadowMapDepth ? 0.0f : 1.0f;
    }
    else if (ShadowFilterMode == 1) // PCF
    {
        return SampleShadowPCF3x3(ShadowMapCSM, uvw, fragDepth, texelSize);
    }
    else // VSM
    {
        return SampleShadowVSM(ShadowMapCSM, uvw, fragDepth);
    }
}

// ── Spot Light Shadow Factor ────────────────────────────────────
float CalcSpotShadowFactor(uint lightIndex, float3 worldPos)
{
    if (NumShadowSpotLights == 0)
        return 1.0f;

    FSpotShadowData sd = SpotShadowDatas[lightIndex];

    float4 lightSpacePos = mul(float4(worldPos, 1.0f), sd.ViewProj);
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    float2 shadowUV = projCoords.xy * float2(0.5f, -0.5f) + 0.5f;
    float  fragDepth = projCoords.z + ShadowBias; // Reversed-Z: bias를 더해야 acne 방지

    // Atlas UV 변환 (scale + bias)
    shadowUV = shadowUV * sd.AtlasScaleBias.xy + sd.AtlasScaleBias.zw;

    if (fragDepth < 0.0f || fragDepth > 1.0f)
        return 1.0f;

    float3 uvw = float3(shadowUV, (float)sd.PageIndex);
    float texelSize = 1.0f / 4096.0f; // atlas resolution

    if (ShadowFilterMode == 0)
        return SampleShadowHard(ShadowMapSpotAtlas, uvw, fragDepth);
    else if (ShadowFilterMode == 1)
        return SampleShadowPCF3x3(ShadowMapSpotAtlas, uvw, fragDepth, texelSize);
    else
        return SampleShadowVSM(ShadowMapSpotAtlas, uvw, fragDepth);
}

// ── Point Light Shadow Factor ───────────────────────────────────
float CalcPointShadowFactor(uint lightIndex, float3 worldPos, float3 lightPos)
{
    if (NumShadowPointLights == 0)
        return 1.0f;

    FPointShadowData pd = PointShadowDatas[lightIndex];

    // 라이트→프래그먼트 방향으로 dominant face 결정
    float3 L = worldPos - lightPos;
    float3 absL = abs(L);

    uint face;
    if (absL.x >= absL.y && absL.x >= absL.z)
        face = (L.x > 0.0f) ? 0 : 1; // +X, -X
    else if (absL.y >= absL.x && absL.y >= absL.z)
        face = (L.y > 0.0f) ? 2 : 3; // +Y, -Y
    else
        face = (L.z > 0.0f) ? 4 : 5; // +Z, -Z

    float4 lightSpacePos = mul(float4(worldPos, 1.0f), pd.FaceViewProj[face]);
    float fragDepth = (lightSpacePos.z / lightSpacePos.w) + ShadowBias; // Reversed-Z

    // TextureCubeArray 샘플링 — 방향 벡터 + array index
    float4 cubeCoord = float4(L, (float)pd.CubeArrayIndex);

    if (ShadowFilterMode == 2) // VSM
    {
        float2 moments = ShadowMapPointCube.SampleLevel(ShadowLinearSampler, cubeCoord, 0).rg;
        return ComputeVSMFactor(moments, fragDepth);
    }
    else // Hard or PCF — comparison sampler로 단일 탭
    {
        return ShadowMapPointCube.SampleCmpLevelZero(ShadowComparisonSampler, cubeCoord, fragDepth);
    }
}

#endif // SHADOW_SAMPLING_HLSLI
