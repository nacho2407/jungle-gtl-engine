#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFColorCoCTex : register(t0);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{
    float2 uv = input.uv;
    float maxRadius = max(DOFMaxCoCRadius, 0.0f);

    if (maxRadius < 0.01f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float apertureBlades = clamp(round(DOFApertureBladeCount), 3.0f, 16.0f);
    float sampleJitter = InterleavedGradientNoise(input.position.xy);
    const int sampleCount = 32;

    float3 accumColor = 0.0f;
    float totalWeight = 0.0f;
    float coverageAccum = 0.0f;
    float coverageWeight = 0.0f;
    float maxCoverage = 0.0f;

    [loop]
    for (int i = 0; i < sampleCount; ++i)
    {
        int sampleIndex = (i + (int)(sampleJitter * 31.0f)) & 31;
        float2 apertureSample = MapDiskSampleToPolygonAperture(DOFPoissonSamples[sampleIndex], apertureBlades, 1.0f);
        float2 offsetPixels = apertureSample * maxRadius;
        float2 sampleUV = uv + offsetPixels * DOFInvHalfResolution;
        float4 neighbor = DOFColorCoCTex.SampleLevel(LinearClampSampler, sampleUV, 0);

        float nearCoC = max(-neighbor.a, 0.0f);
        float sampleDistance = length(offsetPixels);
        float reachWeight = saturate((nearCoC - sampleDistance + 1.0f) * 0.5f);
        float cocWeight = saturate(nearCoC / max(DOFMaxCoCRadius, 0.001f));
        float radiusWeight = lerp(1.0f, length(apertureSample), 0.35f);
        float weight = reachWeight * cocWeight * radiusWeight;

        accumColor += neighbor.rgb * weight;
        totalWeight += weight;
        coverageAccum += reachWeight * cocWeight * radiusWeight;
        coverageWeight += radiusWeight;
        maxCoverage = max(maxCoverage, reachWeight * cocWeight);
    }

    if (totalWeight <= 0.0001f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float averageCoverage = coverageAccum / max(coverageWeight, 0.0001f);
    float coverage = lerp(averageCoverage, maxCoverage, 0.35f);
    return float4(accumColor / totalWeight, smoothstep(0.0f, 1.0f, saturate(coverage)));
}
