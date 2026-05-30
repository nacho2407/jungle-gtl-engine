#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFColorCoCTex : register(t0);

#define MAX_RINGS 4
static const int SamplesPerRing[MAX_RINGS] = { 8, 12, 16, 20 };

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

    float3 accumColor = 0.0f;
    float totalWeight = 0.0f;
    float coverage = 0.0f;

    [loop]
    for (int ring = 1; ring <= MAX_RINGS; ++ring)
    {
        float radiusFraction = (float)ring / (float)MAX_RINGS;
        int sampleCount = SamplesPerRing[ring - 1];

        [loop]
        for (int s = 0; s < sampleCount; ++s)
        {
            float angle = (((float)s + sampleJitter) / (float)sampleCount) * DOFTwoPi;
            float polygonRadius = PolygonBoundaryRadius(angle, apertureBlades);

            float2 direction;
            sincos(angle, direction.y, direction.x);

            float2 offsetPixels = direction * polygonRadius * radiusFraction * maxRadius;
            float2 sampleUV = uv + offsetPixels * DOFInvHalfResolution;
            float4 neighbor = DOFColorCoCTex.SampleLevel(LinearClampSampler, sampleUV, 0);

            float nearCoC = max(-neighbor.a, 0.0f);
            float sampleDistance = length(offsetPixels);
            float reachWeight = saturate((nearCoC - sampleDistance + 1.0f) * 0.5f);
            float cocWeight = saturate(nearCoC / max(DOFMaxCoCRadius, 0.001f));
            float ringWeight = lerp(1.0f, radiusFraction, 0.35f);
            float weight = reachWeight * cocWeight * ringWeight;

            accumColor += neighbor.rgb * weight;
            totalWeight += weight;
            coverage = max(coverage, reachWeight * cocWeight);
        }
    }

    if (totalWeight <= 0.0001f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    return float4(accumColor / totalWeight, saturate(coverage));
}
