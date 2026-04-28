#ifndef SHADOW_SAMPLING_HLSLI
#define SHADOW_SAMPLING_HLSLI

// =============================================================================
// ShadowSampling.hlsli — PCF, VSM, Shadow Factor 유틸리티
// =============================================================================
// 의존: ConstantBuffers.hlsli (ShadowBuffer b5, ShadowMapCSM t21, samplers s3/s4)
//       ForwardLightData.hlsli (SpotShadowDatas t24, PointShadowDatas t25)
//
// ShadowSharpen 매핑:
//   PCF: halfSize = round((1 - sharpen) * 3)  →  0=7×7, 0.33=5×5, 0.67=3×3, 1.0=1×1
//   VSM: light bleeding cutoff = lerp(0.1, 0.6, sharpen)
// =============================================================================

// ── PCF 커널 반경 계산 ─────────────────────────────────────────
// sharpen 0.0 → halfSize 3 (7×7, 49 taps)
// sharpen 1.0 → halfSize 0 (1×1, hard shadow)
int ComputePCFHalfSize(float sharpen)
{
    return (int)round((1.0f - saturate(sharpen)) * 3.0f);
}

// ── Dynamic PCF (Texture2D — Point Light atlas) ────────────────
float SampleShadowPCF2D(Texture2D shadowMap, float2 uv, float compareDepth, float texelSize, int halfSize)
{
    float shadow = 0.0f;
    float count  = 0.0f;

    for (int y = -halfSize; y <= halfSize; ++y)
    {
        for (int x = -halfSize; x <= halfSize; ++x)
        {
            float2 offset = float2(float(x) * texelSize, float(y) * texelSize);
            float depth = shadowMap.SampleLevel(PointClampSampler, uv + offset, 0).r;
            shadow += (compareDepth >= depth) ? 1.0f : 0.0f;
            count += 1.0f;
        }
    }

    return shadow / count;
}

// ── Dynamic PCF (Texture2DArray) ───────────────────────────────
float SampleShadowPCF(Texture2DArray shadowMap, float3 uvw, float compareDepth, float texelSize, int halfSize)
{
    float shadow = 0.0f;
    float count  = 0.0f;

    for (int y = -halfSize; y <= halfSize; ++y)
    {
        for (int x = -halfSize; x <= halfSize; ++x)
        {
            float3 offset = float3(float(x) * texelSize, float(y) * texelSize, 0.0f);
            float depth = shadowMap.SampleLevel(PointClampSampler, uvw + offset, 0).r;
            shadow += (compareDepth >= depth) ? 1.0f : 0.0f;
            count += 1.0f;
        }
    }

    return shadow / count;
}

// ── VSM (Variance Shadow Map) ───────────────────────────────────
// moments = (E[z], E[z^2])   — Reversed-Z: near=1, far=0
// sharpen: light bleeding cutoff 강도 (0=soft, 1=sharp)
float ComputeVSMFactor(float2 moments, float fragDepth, float sharpen)
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

    // Light bleeding 감소 — sharpen이 클수록 공격적으로 cutoff
    float minAmount = lerp(0.1f, 0.6f, saturate(sharpen));
    pMax = saturate((pMax - minAmount) / (1.0f - minAmount));

    return pMax;
}

float SampleShadowVSM(Texture2DArray shadowMap, float3 uvw, float fragDepth, float sharpen)
{
    // ShadowLinearSampler (s4): bilinear filtering
    float2 moments = shadowMap.SampleLevel(ShadowLinearSampler, uvw, 0).rg;
    return ComputeVSMFactor(moments, fragDepth, sharpen);
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

bool SampleDirectionalCascade(float3 biasedPos, uint cascade, out float shadowFactor)
{
    float4 lightSpacePos = mul(float4(biasedPos, 1.0f), CSMViewProj[cascade]);
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    float2 shadowUV = projCoords.xy * float2(0.5f, -0.5f) + 0.5f;
    float  fragDepth = projCoords.z;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        fragDepth < 0.0f  || fragDepth > 1.0f)
    {
        shadowFactor = 1.0f;
        return false;
    }

    // constant depth bias only — slope bias is already handled by normal offset above
    fragDepth += ShadowBias;

    float3 uvw = float3(shadowUV, (float)cascade);
    float texelSize = 1.0f / (float)CSMResolution;

    // FilterMode 분기 — ShadowSharpen은 b5 (Directional per-light)
    if (ShadowFilterMode == 0) // Hard
    {
        float shadowMapDepth = ShadowMapCSM.SampleLevel(PointClampSampler, uvw, 0).r;
        shadowFactor = fragDepth < shadowMapDepth ? 0.0f : 1.0f;
    }
    else if (ShadowFilterMode == 1) // PCF
    {
        int halfSize = ComputePCFHalfSize(ShadowSharpen);
        shadowFactor = SampleShadowPCF(ShadowMapCSM, uvw, fragDepth, texelSize, halfSize);
    }
    else // VSM
    {
        shadowFactor = SampleShadowVSM(ShadowMapCSM, uvw, fragDepth, ShadowSharpen);
    }

    return true;
}

// ── Directional Light Shadow Factor (통합) ──────────────────────
// worldPos:  월드 좌표
// viewDepth: 카메라 뷰 공간 깊이 (abs(viewPos.z))
float CalcDirectionalShadowFactor(float3 worldPos, float viewDepth, float3 N)
{
    if (NumCSMCascades == 0)
        return 1.0f;

    // cascade 선택
    uint cascade = SelectCascade(viewDepth);
    
    // Normal bias
    float3 biasedPos = worldPos + N * ShadowNormalBias;

    float shadowFactor = 1.0f;
    SampleDirectionalCascade(biasedPos, cascade, shadowFactor);

    if (CSMBlendEnabled != 0 && CSMBlendRange > 0.0f && cascade + 1 < NumCSMCascades)
    {
        float splitDepth = CascadeSplits[cascade];
        float blendStart = splitDepth - CSMBlendRange;

        if (viewDepth >= blendStart && viewDepth < splitDepth)
        {
            float nextShadowFactor = 1.0f;
            if (SampleDirectionalCascade(biasedPos, cascade + 1, nextShadowFactor))
            {
                float blendT = saturate((viewDepth - blendStart) / CSMBlendRange);
                shadowFactor = lerp(shadowFactor, nextShadowFactor, blendT);
            }
        }
    }
    
    //마지막 cascade의 경우 fade out 효과 적용
    if (NumCSMCascades > 1 && cascade == NumCSMCascades - 1)
    {
        float lastSplit = CascadeSplits[NumCSMCascades - 1];
        float prevSplit = CascadeSplits[NumCSMCascades - 2];
        float fadeStart = lerp(prevSplit, lastSplit, 0.5f);

        if (viewDepth > fadeStart)
        {
            float fadeT = saturate((viewDepth - fadeStart) / (lastSplit - fadeStart));
            shadowFactor = lerp(shadowFactor, 1.0f, fadeT);
        }
    }

    return shadowFactor;
}

// ── Spot Light Shadow Factor ────────────────────────────────────
float CalcSpotShadowFactor(uint lightIndex, float3 worldPos, float3 N, float3 lightDir)
{
    if (NumShadowSpotLights == 0)
        return 1.0f;

    FSpotShadowData sd = SpotShadowDatas[lightIndex];

    float3 biasedPos = worldPos + N * sd.ShadowNormalBias;
    float4 lightSpacePos = mul(float4(biasedPos, 1.0f), sd.ViewProj);
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    float2 shadowUV = projCoords.xy * float2(0.5f, -0.5f) + 0.5f;
    float  slope = 1.0f - saturate(dot(N, lightDir));
    float  fragDepth = projCoords.z + sd.ShadowBias + sd.ShadowSlopeBias * slope;

    // Atlas UV 변환 (scale + bias)
    shadowUV = shadowUV * sd.AtlasScaleBias.xy + sd.AtlasScaleBias.zw;

    if (fragDepth < 0.0f || fragDepth > 1.0f)
        return 1.0f;

    float3 uvw = float3(shadowUV, (float)sd.PageIndex);
    float texelSize = 1.0f / 4096.0f; // atlas resolution

    if (ShadowFilterMode == 0)
    {
        float shadowMapDepth = ShadowMapSpotAtlas.SampleLevel(PointClampSampler, uvw, 0).r;
        return (fragDepth >= shadowMapDepth) ? 1.0f : 0.0f;
    }
    else if (ShadowFilterMode == 1) // PCF
    {
        int halfSize = ComputePCFHalfSize(sd.ShadowSharpen);
        return SampleShadowPCF(ShadowMapSpotAtlas, uvw, fragDepth, texelSize, halfSize);
    }
    else // VSM
    {
        return SampleShadowVSM(ShadowMapSpotAtlas, uvw, fragDepth, sd.ShadowSharpen);
    }
}

// ── Point Light Shadow Factor ───────────────────────────────────
float CalcPointShadowFactor(uint lightIndex, float3 worldPos, float3 lightPos, float3 N)
{
    if (NumShadowPointLights == 0)
        return 1.0f;

    FPointShadowData pointLightData = PointShadowDatas[lightIndex];

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

    float3 lightDir = normalize(L);
    float slope = 1.0f - saturate(dot(N, -lightDir));

    float3 biasedPos = worldPos + N * pointLightData.ShadowNormalBias;
    float4 lightSpacePos = mul(float4(biasedPos, 1.0f), pointLightData.FaceViewProj[face]);
    float3 ndc = lightSpacePos.xyz / lightSpacePos.w;

    float2 projUV = ndc.xy * float2(0.5f, -0.5f) + 0.5f;
    float  fragDepth = ndc.z + pointLightData.ShadowBias + pointLightData.ShadowSlopeBias * slope;

    // Atlas UV 변환: face별 scale/bias로 아틀라스 내 위치로 리맵
    float4 sb = pointLightData.FaceAtlasScaleBias[face];
    float2 atlasUV = projUV * sb.xy + sb.zw;

    if (ShadowFilterMode == 2) // VSM
    {
        float2 moments = ShadowMapPointLightAtlas.SampleLevel(ShadowLinearSampler, atlasUV, 0).rg;
        return ComputeVSMFactor(moments, fragDepth, pointLightData.ShadowSharpen);
    }
    else if (ShadowFilterMode == 1) // PCF
    {
        float texelSize = 1.0f / 4096.0f;
        int halfSize = ComputePCFHalfSize(pointLightData.ShadowSharpen);
        return SampleShadowPCF2D(ShadowMapPointLightAtlas, atlasUV, fragDepth, texelSize, halfSize);
    }
    else // Hard
    {
        float depth = ShadowMapPointLightAtlas.SampleLevel(PointClampSampler, atlasUV, 0).r;
        return (fragDepth >= depth) ? 1.0f : 0.0f;
    }
}

#endif // SHADOW_SAMPLING_HLSLI
