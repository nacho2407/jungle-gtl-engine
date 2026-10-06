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
    float4 center = DOFColorCoCTex.SampleLevel(LinearClampSampler, uv, 0);
    float centerCoC = max(center.a, 0.0f);

    if (centerCoC < 0.01f)
    {
        return float4(center.rgb, 0.0f);
    }

    float apertureBlades = clamp(round(DOFApertureBladeCount), 3.0f, 16.0f);
    float sampleJitter = InterleavedGradientNoise(input.position.xy);
    float polygonStrength = smoothstep(2.0f, 8.0f, centerCoC);
    int sampleCount = 10;
    if (centerCoC > 10.0f)
    {
        sampleCount = 32;
    }
    else if (centerCoC > 6.0f)
    {
        sampleCount = 24;
    }
    else if (centerCoC > 2.0f)
    {
        sampleCount = 16;
    }

    float3 accumColor = center.rgb;
    float totalWeight = 1.0f;

    [loop]
    for (int i = 0; i < sampleCount; ++i)
    {
        int sampleIndex = (i + (int)(sampleJitter * 31.0f)) & 31;
        float2 apertureSample = MapDiskSampleToPolygonAperture(DOFPoissonSamples[sampleIndex], apertureBlades, polygonStrength);
        float2 sampleUV = uv + apertureSample * centerCoC * DOFInvHalfResolution;
        float4 neighbor = DOFColorCoCTex.SampleLevel(LinearClampSampler, sampleUV, 0);
        float neighborCoC = max(neighbor.a, 0.0f);

        float sampleRadius = length(apertureSample);
        float cocWeight = saturate(neighborCoC / max(centerCoC, 0.001f));
        float radiusWeight = lerp(1.0f, sampleRadius, 0.35f);
        float weight = cocWeight * radiusWeight;

        accumColor += neighbor.rgb * weight;
        totalWeight += weight;
    }

    return float4(accumColor / max(totalWeight, 0.0001f), saturate(centerCoC / max(DOFMaxCoCRadius, 0.001f)));
}
