#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

static const float PI = 3.14159265359f;
static const float GOLDEN_ANGLE = 2.39996322973f;

cbuffer DepthOfFieldCB : register(b2)
{
    // xy = full-resolution source texel size, zw = DOF target texel size
    float4 TexelSize;
    // x = focus distance, y = CoC scale, z = max foreground radius, w = max background radius
    float4 FocusParams;
    // x = near clip, y = far clip, z = target/source radius scale, w = gather sharpness
    float4 DepthParams;
    uint GatherRingCount;
    uint GatherSamplesPerRing;
    uint bEnableForeground;
    uint bEnableBackground;
    uint DebugView;
    uint bHalfRes;
    float SlightFocusRadius;
    float ForegroundIntensity;
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float ReconstructCameraForwardDepth(float2 uv, float rawDepth)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 viewH = mul(float4(ndc, rawDepth, 1.0f), InvProj);
    float viewDepth = abs(viewH.w) > 0.0001f ? viewH.z / viewH.w : DepthParams.y;
    return clamp(viewDepth, DepthParams.x, DepthParams.y);
}

float LoadLinearDepth(float2 uv)
{
    uint width;
    uint height;
    SceneDepthTexture.GetDimensions(width, height);

    float2 clampedUV = saturate(uv);
    int2 coord = int2(clampedUV * float2(width, height));
    coord = clamp(coord, int2(0, 0), int2(int(width) - 1, int(height) - 1));
    float rawDepth = SceneDepthTexture.Load(int3(coord, 0));
    return ReconstructCameraForwardDepth(clampedUV, rawDepth);
}

float ComputeSignedCocFull(float linearDepth)
{
    float depth = max(linearDepth, 0.001f);
    float coc = ((depth - FocusParams.x) / depth) * FocusParams.y;
    float cocRadius = abs(coc);
    float focusFade = smoothstep(0.0f, max(SlightFocusRadius, 0.001f), cocRadius);
    coc *= focusFade;
    return clamp(coc, -FocusParams.z, FocusParams.w);
}

float ComputeSignedCocTarget(float linearDepth)
{
    return ComputeSignedCocFull(linearDepth) * DepthParams.z;
}

struct FDofSourceSample
{
    float3 Color;
    float SignedCoc;
};

struct FSetupOutput
{
    float4 Background : SV_Target0;
    float4 Foreground : SV_Target1;
};

int2 GetSceneCoord(float2 uv)
{
    uint width;
    uint height;
    SceneDepthTexture.GetDimensions(width, height);

    float2 clampedUV = saturate(uv);
    int2 coord = int2(clampedUV * float2(width, height));
    return clamp(coord, int2(0, 0), int2(int(width) - 1, int(height) - 1));
}

FDofSourceSample LoadDofSourceSample(float2 uv)
{
    float2 clampedUV = saturate(uv);
    int2 coord = GetSceneCoord(clampedUV);
    float rawDepth = SceneDepthTexture.Load(int3(coord, 0));

    FDofSourceSample sample;
    sample.Color = SceneColorTexture.Load(int3(coord, 0)).rgb;
    sample.SignedCoc = ComputeSignedCocTarget(ReconstructCameraForwardDepth(clampedUV, rawDepth));
    return sample;
}

void AccumulateSetupSample(
    FDofSourceSample sample,
    inout float3 backgroundColorSum,
    inout float backgroundWeightSum,
    inout float backgroundRadius,
    inout float backgroundRadiusSum,
    inout float backgroundSampleCount,
    inout float3 foregroundColorSum,
    inout float foregroundWeightSum,
    inout float foregroundRadius,
    inout float foregroundRadiusSum,
    inout float foregroundSampleCount)
{
    if (sample.SignedCoc > 0.0001f)
    {
        float weight = max(sample.SignedCoc, 0.001f);
        backgroundColorSum += sample.Color * weight;
        backgroundWeightSum += weight;
        backgroundRadius = max(backgroundRadius, sample.SignedCoc);
        backgroundRadiusSum += sample.SignedCoc;
        backgroundSampleCount += 1.0f;
    }
    else if (sample.SignedCoc < -0.0001f)
    {
        float radius = -sample.SignedCoc;
        float weight = max(radius, 0.001f);
        foregroundColorSum += sample.Color * weight;
        foregroundWeightSum += weight;
        foregroundRadius = max(foregroundRadius, radius);
        foregroundRadiusSum += radius;
        foregroundSampleCount += 1.0f;
    }
}

float ResolveSetupRadius(float radiusSum, float activeSampleCount, float footprintSampleCount, float maxRadius)
{
    if (activeSampleCount <= 0.0001f)
    {
        return 0.0f;
    }

    float averageRadius = radiusSum / max(footprintSampleCount, 1.0f);
    float maxBiasStart = max(SlightFocusRadius * DepthParams.z, 0.5f);
    float maxBiasEnd = max(maxBiasStart + 3.0f, 2.0f);
    float maxBias = smoothstep(maxBiasStart, maxBiasEnd, maxRadius);
    return lerp(averageRadius, maxRadius, maxBias);
}

FSetupOutput PS_Setup(PS_Input_UV input)
{
    float2 uv = input.uv;
    FDofSourceSample center = LoadDofSourceSample(uv);

    float3 backgroundColorSum = 0.0f;
    float backgroundWeightSum = 0.0f;
    float backgroundRadius = 0.0f;
    float backgroundRadiusSum = 0.0f;
    float backgroundSampleCount = 0.0f;
    float3 foregroundColorSum = 0.0f;
    float foregroundWeightSum = 0.0f;
    float foregroundRadius = 0.0f;
    float foregroundRadiusSum = 0.0f;
    float foregroundSampleCount = 0.0f;
    float setupFootprintSampleCount = bHalfRes != 0u ? 4.0f : 1.0f;

    if (bHalfRes != 0u)
    {
        [unroll]
        for (uint y = 0u; y < 2u; ++y)
        {
            [unroll]
            for (uint x = 0u; x < 2u; ++x)
            {
                float2 offset = float2(x == 0u ? -0.5f : 0.5f, y == 0u ? -0.5f : 0.5f);
                AccumulateSetupSample(
                    LoadDofSourceSample(uv + offset * TexelSize.xy),
                    backgroundColorSum,
                    backgroundWeightSum,
                    backgroundRadius,
                    backgroundRadiusSum,
                    backgroundSampleCount,
                    foregroundColorSum,
                    foregroundWeightSum,
                    foregroundRadius,
                    foregroundRadiusSum,
                    foregroundSampleCount);
            }
        }
    }
    else
    {
        AccumulateSetupSample(
            center,
            backgroundColorSum,
            backgroundWeightSum,
            backgroundRadius,
            backgroundRadiusSum,
            backgroundSampleCount,
            foregroundColorSum,
            foregroundWeightSum,
            foregroundRadius,
            foregroundRadiusSum,
            foregroundSampleCount);
    }

    backgroundRadius = ResolveSetupRadius(backgroundRadiusSum, backgroundSampleCount, setupFootprintSampleCount, backgroundRadius);
    foregroundRadius = ResolveSetupRadius(foregroundRadiusSum, foregroundSampleCount, setupFootprintSampleCount, foregroundRadius);

    float3 foregroundColor = foregroundWeightSum > 0.0001f ? foregroundColorSum / foregroundWeightSum : 0.0f;

    FSetupOutput output;
    float3 backgroundColor = backgroundWeightSum > 0.0001f ? backgroundColorSum / backgroundWeightSum : 0.0f;
    output.Background = float4(
        backgroundColor * backgroundRadius,
        backgroundRadius);
    output.Foreground = float4(foregroundColor * foregroundRadius, foregroundRadius);
    return output;
}

float4 LoadDofBackgroundSetup(float2 uv)
{
    float4 sampleValue = DofBackgroundSetupTexture.SampleLevel(LinearClampSampler, uv, 0);
    float radius = max(sampleValue.a, 0.0f);
    float3 color = radius > 0.0001f ? sampleValue.rgb / radius : 0.0f;
    return float4(color, radius);
}

float4 LoadDofForegroundSetup(float2 uv)
{
    float4 sampleValue = DofForegroundSetupTexture.SampleLevel(LinearClampSampler, uv, 0);
    float radius = max(sampleValue.a, 0.0f);
    float3 color = radius > 0.0001f ? sampleValue.rgb / radius : 0.0f;
    return float4(color, radius);
}

struct FGatherLayer
{
    float3 ColorSum;
    float WeightSum;
    float Coverage;
    float CoverageSum;
    float TotalSampleWeight;
};

void InitGatherLayer(out FGatherLayer layer)
{
    layer.ColorSum = 0.0f;
    layer.WeightSum = 0.0f;
    layer.Coverage = 0.0f;
    layer.CoverageSum = 0.0f;
    layer.TotalSampleWeight = 0.0f;
}

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

float2 GetDofTargetPixel(float2 uv)
{
    return floor(uv / max(TexelSize.zw, float2(0.000001f, 0.000001f)));
}

float ComputeGatherSampleT(uint sampleIndex, uint sampleCount, float radialJitter)
{
    float sampleT = (float(sampleIndex) + 0.5f + (radialJitter - 0.5f) * 0.65f) / max(float(sampleCount), 1.0f);
    return saturate(sampleT);
}

float ComputeGatherRadiusT(uint sampleIndex, uint sampleCount, float radialJitter)
{
    return sqrt(ComputeGatherSampleT(sampleIndex, sampleCount, radialJitter));
}

float ComputeGatherAreaWeight(float sampleT)
{
    return 1.0f;
}

float ComputeGatherAngle(uint sampleIndex, float gatherRotation)
{
    return gatherRotation + float(sampleIndex) * GOLDEN_ANGLE;
}

float ComputeLayerIntersection(float coc, float sampleDistance)
{
    float radiusMask = smoothstep(0.0f, 0.75f, coc);
    float transitionWidth = max(1.0f / max(DepthParams.w, 0.0001f), 0.75f);
    float edgeCoverage = smoothstep(sampleDistance - transitionWidth, sampleDistance + transitionWidth, coc);
    return edgeCoverage * radiusMask;
}

void AccumulateBackgroundSample(float4 sampleValue, float sampleDistance, float areaWeight, uint bEnabled, inout FGatherLayer layer)
{
    float coc = sampleValue.a;
    if (bEnabled == 0u)
    {
        return;
    }

    areaWeight = max(areaWeight, 0.0f);
    if (areaWeight <= 0.0001f)
    {
        return;
    }

    layer.TotalSampleWeight += areaWeight;
    if (coc <= 0.0001f)
    {
        return;
    }

    float intersection = ComputeLayerIntersection(coc, sampleDistance);
    if (intersection <= 0.0001f)
    {
        return;
    }

    float weight = intersection * areaWeight;
    layer.ColorSum += sampleValue.rgb * weight;
    layer.WeightSum += weight;
    layer.Coverage = max(layer.Coverage, intersection);
    layer.CoverageSum += intersection * areaWeight;
}

float ComputeForegroundFading(float radius)
{
    float focusBoundary = max(SlightFocusRadius * DepthParams.z, 0.5f);
    float fadeWidth = max(2.0f * DepthParams.z, 1.5f);
    return smoothstep(focusBoundary, focusBoundary + fadeWidth, radius) * ForegroundIntensity;
}

float ComputeForegroundSampleWeight(float radius)
{
    const float pixelRadius = 0.25f;
    float safeRadius = max(radius, pixelRadius);
    return min(1.0f / (PI * safeRadius * safeRadius), 1.0f / (PI * pixelRadius * pixelRadius));
}

struct FForegroundGatherSample
{
    float3 Color;
    float Radius;
    float Distance;
    float Intersection;
    float AreaWeight;
    float Weight;
};

struct FForegroundGatherLayer
{
    float3 Layer0ColorSum;
    float Layer0Weight;
    float3 Layer1ColorSum;
    float Layer1Weight;
    float ForegroundSampleCount;
    float TotalSampleCount;
    float OcclusionSum;
    float MaxOcclusion;
    float ClosestForegroundRadius;
    float RingCount;
};

struct FForegroundHoleFillLayer
{
    float3 ColorSum;
    float WeightSum;
    float ClosestDistance;
    float KernelRadius;
};

void InitForegroundGatherLayer(float closestForegroundRadius, uint ringCount, out FForegroundGatherLayer layer)
{
    layer.Layer0ColorSum = 0.0f;
    layer.Layer0Weight = 0.0f;
    layer.Layer1ColorSum = 0.0f;
    layer.Layer1Weight = 0.0f;
    layer.ForegroundSampleCount = 0.0f;
    layer.TotalSampleCount = 0.0f;
    layer.OcclusionSum = 0.0f;
    layer.MaxOcclusion = 0.0f;
    layer.ClosestForegroundRadius = closestForegroundRadius;
    layer.RingCount = max(float(ringCount), 1.0f);
}

void InitForegroundHoleFillLayer(float closestForegroundRadius, out FForegroundHoleFillLayer layer)
{
    layer.ColorSum = 0.0f;
    layer.WeightSum = 0.0f;
    layer.ClosestDistance = 1000000.0f;
    layer.KernelRadius = max(closestForegroundRadius, 0.001f);
}

FForegroundGatherSample MakeForegroundGatherSample(float4 sampleValue, float sampleDistance, float areaWeight)
{
    FForegroundGatherSample sample;
    sample.Color = sampleValue.rgb;
    sample.Radius = sampleValue.a;
    sample.Distance = sampleDistance;
    sample.Intersection = ComputeLayerIntersection(sample.Radius, sampleDistance);
    sample.AreaWeight = areaWeight;
    sample.Weight = ComputeForegroundSampleWeight(sample.Radius);
    return sample;
}

void AccumulateForegroundLayerSample(FForegroundGatherSample sample, uint bEnabled, inout FForegroundGatherLayer layer)
{
    if (bEnabled == 0u)
    {
        return;
    }

    float areaWeight = max(sample.AreaWeight, 0.0f);
    if (areaWeight <= 0.0001f)
    {
        return;
    }

    layer.TotalSampleCount += areaWeight;

    float foregroundFading = ComputeForegroundFading(sample.Radius);
    if (sample.Radius > 0.0001f)
    {
        float support = ComputeLayerIntersection(sample.Radius + 1.0f, sample.Distance) * foregroundFading;
        layer.ForegroundSampleCount += support * areaWeight;
    }

    sample.Intersection *= foregroundFading;
    layer.OcclusionSum += sample.Intersection * areaWeight;
    layer.MaxOcclusion = max(layer.MaxOcclusion, sample.Intersection);
    if (sample.Intersection <= 0.0001f)
    {
        return;
    }

    float layer1 = saturate((layer.ClosestForegroundRadius - sample.Radius) * 0.5f);
    float layer0 = 1.0f - layer1;
    float weightedIntersection = sample.Intersection * sample.Weight * areaWeight;

    layer.Layer0ColorSum += sample.Color * (layer0 * weightedIntersection);
    layer.Layer0Weight += layer0 * weightedIntersection;
    layer.Layer1ColorSum += sample.Color * (layer1 * weightedIntersection);
    layer.Layer1Weight += layer1 * weightedIntersection;
}

void AccumulateForegroundPair(FForegroundGatherSample a, FForegroundGatherSample b, uint bEnabled, inout FForegroundGatherLayer layer)
{
    AccumulateForegroundLayerSample(a, bEnabled, layer);
    AccumulateForegroundLayerSample(b, bEnabled, layer);
}

void AccumulateForegroundHoleFillSample(float4 sampleValue, float sampleDistance, float areaWeight, uint bEnabled, inout FForegroundHoleFillLayer layer)
{
    float coc = sampleValue.a;
    if (bEnabled == 0u || coc <= 0.0001f)
    {
        return;
    }

    float holeRadius = coc * 1.15f;
    float holeIntersection = ComputeLayerIntersection(holeRadius, sampleDistance);
    if (holeIntersection <= 0.0001f)
    {
        return;
    }

    layer.ClosestDistance = min(layer.ClosestDistance, sampleDistance);
    float weight = holeIntersection * areaWeight;
    layer.ColorSum += sampleValue.rgb * weight;
    layer.WeightSum += weight;
}

float4 ResolveBackgroundLayer(FGatherLayer layer, float3 fallbackColor)
{
    float3 color = layer.WeightSum > 0.0001f ? layer.ColorSum / layer.WeightSum : fallbackColor;
    float averageCoverage = layer.TotalSampleWeight > 0.0001f ? layer.CoverageSum / layer.TotalSampleWeight : 0.0f;
    float densityCoverage = smoothstep(0.0f, 0.18f, averageCoverage);
    float peakCoverage = layer.Coverage * 0.85f;
    float coverage = saturate(max(densityCoverage, peakCoverage));
    return float4(color * coverage, coverage);
}

float ComputeBackgroundBlend(float cocFull)
{
    float fadeWidth = max(SlightFocusRadius + 2.0f, 1.0f);
    return smoothstep(0.0f, fadeWidth, max(cocFull, 0.0f));
}

float4 ResolveForegroundLayer(FForegroundGatherLayer layer)
{
    float totalWeight = layer.Layer0Weight + layer.Layer1Weight;
    if (totalWeight <= 0.0001f || layer.TotalSampleCount <= 0.0001f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float3 layer0Color = layer.Layer0Weight > 0.0001f ? layer.Layer0ColorSum / layer.Layer0Weight : float3(0.0f, 0.0f, 0.0f);
    float3 layer1Color = layer.Layer1Weight > 0.0001f ? layer.Layer1ColorSum / layer.Layer1Weight : layer0Color;
    layer0Color = layer.Layer0Weight > 0.0001f ? layer0Color : layer1Color;

    float kernelRadius = max(layer.ClosestForegroundRadius * ((layer.RingCount + 0.5f) / layer.RingCount), 0.25f);
    float kernelArea = 1.0f / ComputeForegroundSampleWeight(kernelRadius);
    float layer0Opacity = saturate((layer.Layer1Weight <= 0.0001f ? 1.0f : 0.0f) + (kernelArea * layer.Layer0Weight / layer.TotalSampleCount));

    float3 color = lerp(layer1Color, layer0Color, layer0Opacity);
    float supportOpacity = saturate(layer.ForegroundSampleCount / max(layer.TotalSampleCount * 0.45f, 1.0f));
    float intersectionOpacity = saturate(layer.OcclusionSum / max(layer.TotalSampleCount * 0.5f, 1.0f));
    float peakOpacity = layer.MaxOcclusion * 0.75f;
    float opacity = min(saturate(max(intersectionOpacity, max(peakOpacity, supportOpacity * 0.35f))), ForegroundIntensity);
    return float4(color * opacity, opacity);
}

float4 ResolveForegroundHoleFillLayer(FForegroundHoleFillLayer layer)
{
    if (layer.WeightSum <= 0.0001f || layer.ClosestDistance >= 999999.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float3 color = layer.ColorSum / layer.WeightSum;
    float opacity = saturate(1.0f - (layer.ClosestDistance / layer.KernelRadius)) * (0.5f * ForegroundIntensity);
    return float4(color * opacity, opacity);
}

float FindClosestForegroundRadius(
    float2 uv,
    float4 centerForeground,
    uint ringCount,
    uint foregroundPairCount,
    float maxRadius,
    float gatherRotation,
    float radialJitter)
{
    float closestRadius = max(centerForeground.a, 0.0f);
    uint pairSampleCount = max(ringCount * foregroundPairCount, 1u);

    [loop]
    for (uint pairSampleIndex = 0u; pairSampleIndex < pairSampleCount; ++pairSampleIndex)
    {
        float radiusT = ComputeGatherRadiusT(pairSampleIndex, pairSampleCount, radialJitter);
        float radius = maxRadius * radiusT;
        float angle = ComputeGatherAngle(pairSampleIndex, gatherRotation);
        float2 offsetPixels = float2(cos(angle), sin(angle)) * radius;
        float2 sampleUV = offsetPixels * TexelSize.zw;

        closestRadius = max(closestRadius, LoadDofForegroundSetup(uv + sampleUV).a);
        closestRadius = max(closestRadius, LoadDofForegroundSetup(uv - sampleUV).a);
    }

    return closestRadius;
}

float3 UnpremultiplyForDebug(float4 premultipliedColor)
{
    float opacity = saturate(premultipliedColor.a);
    if (opacity <= 0.0001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    return premultipliedColor.rgb / opacity;
}

float4 CompositePremultipliedLayer(float4 under, float4 over)
{
    float underOpacity = saturate(under.a);
    float overOpacity = saturate(over.a);
    float3 color = under.rgb * (1.0f - overOpacity) + over.rgb;
    float opacity = 1.0f - (1.0f - underOpacity) * (1.0f - overOpacity);
    return float4(color, saturate(opacity));
}

float4 ClampPremultipliedOpacity(float4 layer, float maxOpacity)
{
    float opacity = saturate(layer.a);
    float clampedOpacity = min(opacity, saturate(maxOpacity));
    if (opacity <= 0.0001f || clampedOpacity >= opacity)
    {
        return float4(layer.rgb, opacity);
    }

    float scale = clampedOpacity / opacity;
    return float4(layer.rgb * scale, clampedOpacity);
}

float4 LoadForegroundLayerForRecombine(float2 uv)
{
    float4 foreground = DofForegroundTexture.SampleLevel(LinearClampSampler, uv, 0);
    float4 foregroundHoleFill = DofForegroundHoleFillTexture.SampleLevel(LinearClampSampler, uv, 0);
    float4 combined = CompositePremultipliedLayer(foregroundHoleFill, foreground);
    return ClampPremultipliedOpacity(combined, ForegroundIntensity + 0.1f);
}

void AccumulateBackgroundUpsampleSample(
    float2 sampleUV,
    float currentRadius,
    float spatialWeight,
    inout float4 valueSum,
    inout float weightSum)
{
    float4 sampleValue = DofBackgroundTexture.SampleLevel(LinearClampSampler, sampleUV, 0);
    if (sampleValue.a <= 0.0001f)
    {
        return;
    }

    float sampleRadius = LoadDofBackgroundSetup(sampleUV).a;
    float cocWeight = exp2(-abs(sampleRadius - currentRadius) * 0.45f);
    float coverageWeight = saturate(sampleValue.a * 3.0f);
    float weight = spatialWeight * cocWeight * coverageWeight;
    valueSum += sampleValue * weight;
    weightSum += weight;
}

void AccumulateForegroundUpsampleSample(
    float2 sampleUV,
    float currentRadius,
    float spatialWeight,
    inout float4 valueSum,
    inout float weightSum)
{
    float4 sampleValue = LoadForegroundLayerForRecombine(sampleUV);
    if (sampleValue.a <= 0.0001f)
    {
        return;
    }

    float sampleRadius = LoadDofForegroundSetup(sampleUV).a;
    float cocWeight = currentRadius > 0.25f ? exp2(-abs(sampleRadius - currentRadius) * 0.25f) : 1.0f;
    float opacityWeight = currentRadius > 0.25f
        ? lerp(0.25f, 1.0f, saturate(sampleValue.a * 2.0f))
        : saturate(sampleValue.a * 3.0f);
    float weight = spatialWeight * cocWeight * opacityWeight;
    valueSum += sampleValue * weight;
    weightSum += weight;
}

float4 SampleBackgroundForRecombine(float2 uv, float currentRadius)
{
    float4 result = DofBackgroundTexture.SampleLevel(LinearClampSampler, uv, 0);
    if (bHalfRes == 0u)
    {
        return result;
    }

    float2 texel = TexelSize.zw;
    float4 valueSum = 0.0f;
    float weightSum = 0.0f;

    AccumulateBackgroundUpsampleSample(uv, currentRadius, 1.0f, valueSum, weightSum);
    AccumulateBackgroundUpsampleSample(uv + float2(texel.x, 0.0f), currentRadius, 0.55f, valueSum, weightSum);
    AccumulateBackgroundUpsampleSample(uv - float2(texel.x, 0.0f), currentRadius, 0.55f, valueSum, weightSum);
    AccumulateBackgroundUpsampleSample(uv + float2(0.0f, texel.y), currentRadius, 0.55f, valueSum, weightSum);
    AccumulateBackgroundUpsampleSample(uv - float2(0.0f, texel.y), currentRadius, 0.55f, valueSum, weightSum);
    AccumulateBackgroundUpsampleSample(uv + texel, currentRadius, 0.3f, valueSum, weightSum);
    AccumulateBackgroundUpsampleSample(uv - texel, currentRadius, 0.3f, valueSum, weightSum);
    AccumulateBackgroundUpsampleSample(uv + float2(texel.x, -texel.y), currentRadius, 0.3f, valueSum, weightSum);
    AccumulateBackgroundUpsampleSample(uv + float2(-texel.x, texel.y), currentRadius, 0.3f, valueSum, weightSum);

    result = weightSum > 0.0001f ? valueSum / weightSum : float4(0.0f, 0.0f, 0.0f, 0.0f);
    return result;
}

float4 SampleForegroundForRecombine(float2 uv, float currentRadius)
{
    float4 result = LoadForegroundLayerForRecombine(uv);
    if (bHalfRes == 0u)
    {
        return result;
    }

    float2 texel = TexelSize.zw;
    float4 valueSum = 0.0f;
    float weightSum = 0.0f;

    AccumulateForegroundUpsampleSample(uv, currentRadius, 1.0f, valueSum, weightSum);
    AccumulateForegroundUpsampleSample(uv + float2(texel.x, 0.0f), currentRadius, 0.55f, valueSum, weightSum);
    AccumulateForegroundUpsampleSample(uv - float2(texel.x, 0.0f), currentRadius, 0.55f, valueSum, weightSum);
    AccumulateForegroundUpsampleSample(uv + float2(0.0f, texel.y), currentRadius, 0.55f, valueSum, weightSum);
    AccumulateForegroundUpsampleSample(uv - float2(0.0f, texel.y), currentRadius, 0.55f, valueSum, weightSum);
    AccumulateForegroundUpsampleSample(uv + texel, currentRadius, 0.3f, valueSum, weightSum);
    AccumulateForegroundUpsampleSample(uv - texel, currentRadius, 0.3f, valueSum, weightSum);
    AccumulateForegroundUpsampleSample(uv + float2(texel.x, -texel.y), currentRadius, 0.3f, valueSum, weightSum);
    AccumulateForegroundUpsampleSample(uv + float2(-texel.x, texel.y), currentRadius, 0.3f, valueSum, weightSum);

    result = weightSum > 0.0001f ? valueSum / weightSum : float4(0.0f, 0.0f, 0.0f, 0.0f);
    return result;
}

struct FGatherOutput
{
    float4 Background : SV_Target0;
    float4 Foreground : SV_Target1;
    float4 ForegroundHoleFill : SV_Target2;
};

FGatherOutput PS_Gather(PS_Input_UV input)
{
    float2 uv = input.uv;
    float4 centerBackground = LoadDofBackgroundSetup(uv);
    float4 centerForeground = LoadDofForegroundSetup(uv);

    uint ringCount = clamp(GatherRingCount, 1u, 10u);
    uint samplesPerRing = clamp(GatherSamplesPerRing, 4u, 32u);
    uint foregroundPairCount = max((samplesPerRing + 1u) / 2u, 2u);
    float backgroundMaxRadius = FocusParams.w * DepthParams.z;
    float foregroundMaxRadius = FocusParams.z * DepthParams.z;
    float2 targetPixel = GetDofTargetPixel(uv);
    float gatherRotation = Hash12(targetPixel) * (2.0f * PI);
    float radialJitter = Hash12(targetPixel + 19.19f);
    float closestForegroundRadius = FindClosestForegroundRadius(
        uv,
        centerForeground,
        ringCount,
        foregroundPairCount,
        foregroundMaxRadius,
        gatherRotation,
        radialJitter);

    FGatherLayer background;
    InitGatherLayer(background);

    float centerAreaWeight = ComputeGatherAreaWeight(0.0f);
    AccumulateBackgroundSample(centerBackground, 0.0f, centerAreaWeight, bEnableBackground, background);

    FForegroundGatherLayer foreground;
    FForegroundHoleFillLayer foregroundHoleFill;
    InitForegroundGatherLayer(closestForegroundRadius, ringCount, foreground);
    InitForegroundHoleFillLayer(closestForegroundRadius, foregroundHoleFill);

    AccumulateForegroundLayerSample(MakeForegroundGatherSample(centerForeground, 0.0f, centerAreaWeight), bEnableForeground, foreground);
    AccumulateForegroundHoleFillSample(centerForeground, 0.0f, centerAreaWeight, bEnableForeground, foregroundHoleFill);

    uint backgroundSampleCount = max(ringCount * samplesPerRing, 1u);
    [loop]
    for (uint sampleIndex = 0u; sampleIndex < backgroundSampleCount; ++sampleIndex)
    {
        float radiusT = ComputeGatherRadiusT(sampleIndex, backgroundSampleCount, radialJitter);
        float radius = backgroundMaxRadius * radiusT;
        float areaWeight = ComputeGatherAreaWeight(radiusT);
        float angle = ComputeGatherAngle(sampleIndex, gatherRotation);
        float2 offsetPixels = float2(cos(angle), sin(angle)) * radius;
        float2 sampleUV = uv + offsetPixels * TexelSize.zw;
        float4 backgroundSample = LoadDofBackgroundSetup(sampleUV);
        float sampleDistance = length(offsetPixels);

        AccumulateBackgroundSample(backgroundSample, sampleDistance, areaWeight, bEnableBackground, background);
    }

    uint foregroundPairSampleCount = max(ringCount * foregroundPairCount, 1u);
    [loop]
    for (uint pairSampleIndex = 0u; pairSampleIndex < foregroundPairSampleCount; ++pairSampleIndex)
    {
        float radiusT = ComputeGatherRadiusT(pairSampleIndex, foregroundPairSampleCount, radialJitter);
        float radius = foregroundMaxRadius * radiusT;
        float areaWeight = ComputeGatherAreaWeight(radiusT);
        float angle = ComputeGatherAngle(pairSampleIndex, gatherRotation);
        float2 offsetPixels = float2(cos(angle), sin(angle)) * radius;
        float2 offsetUV = offsetPixels * TexelSize.zw;
        float sampleDistance = length(offsetPixels);

        float4 foregroundSampleA = LoadDofForegroundSetup(uv + offsetUV);
        float4 foregroundSampleB = LoadDofForegroundSetup(uv - offsetUV);
        AccumulateForegroundPair(
            MakeForegroundGatherSample(foregroundSampleA, sampleDistance, areaWeight),
            MakeForegroundGatherSample(foregroundSampleB, sampleDistance, areaWeight),
            bEnableForeground,
            foreground);
        AccumulateForegroundHoleFillSample(foregroundSampleA, sampleDistance, areaWeight, bEnableForeground, foregroundHoleFill);
        AccumulateForegroundHoleFillSample(foregroundSampleB, sampleDistance, areaWeight, bEnableForeground, foregroundHoleFill);
    }

    FGatherOutput output;
    output.Background = ResolveBackgroundLayer(background, centerBackground.rgb);
    output.Foreground = ResolveForegroundLayer(foreground);
    output.ForegroundHoleFill = ResolveForegroundHoleFillLayer(foregroundHoleFill);
    return output;
}

float3 VisualizeCoc(float coc)
{
    float focusBand = 0.05f;
    if (abs(coc) <= focusBand)
    {
        return float3(0.03f, 0.03f, 0.03f);
    }

    if (coc < 0.0f)
    {
        float amount = saturate((-coc) / max(FocusParams.z, 0.001f));
        return lerp(float3(0.03f, 0.03f, 0.03f), float3(1.0f, 0.28f, 0.08f), amount);
    }
    else
    {
        float amount = saturate(coc / max(FocusParams.w, 0.001f));
        return lerp(float3(0.03f, 0.03f, 0.03f), float3(0.08f, 0.45f, 1.0f), amount);
    }
}

float4 PS_Recombine(PS_Input_UV input) : SV_TARGET
{
    float2 uv = input.uv;
    float4 scene = SceneColorTexture.SampleLevel(LinearClampSampler, uv, 0);
    float linearDepth = LoadLinearDepth(uv);
    float cocFull = ComputeSignedCocFull(linearDepth);
    float currentBackgroundRadius = max(cocFull, 0.0f) * DepthParams.z;
    float currentForegroundRadius = max(-cocFull, 0.0f) * DepthParams.z;

    float4 background = SampleBackgroundForRecombine(uv, currentBackgroundRadius);
    float4 foregroundLayer = SampleForegroundForRecombine(uv, currentForegroundRadius);

    if (DebugView == 1u)
    {
        return float4(VisualizeCoc(cocFull), 1.0f);
    }
    if (DebugView == 2u)
    {
        return float4(UnpremultiplyForDebug(foregroundLayer) * max(foregroundLayer.a, 0.15f), 1.0f);
    }
    if (DebugView == 3u)
    {
        return float4(UnpremultiplyForDebug(background) * max(background.a, 0.15f), 1.0f);
    }

    float3 result = scene.rgb;

    float backgroundBlend = 0.0f;
    float3 backgroundColor = background.a > 0.0001f ? background.rgb / background.a : scene.rgb;
    if (bEnableBackground != 0u)
    {
        backgroundBlend = ComputeBackgroundBlend(cocFull);
        backgroundBlend *= background.a;
    }

    result = lerp(result, backgroundColor, saturate(backgroundBlend));
    if (bEnableForeground != 0u)
    {
        result = result * (1.0f - foregroundLayer.a) + foregroundLayer.rgb;
    }

    return float4(result, scene.a);
}
