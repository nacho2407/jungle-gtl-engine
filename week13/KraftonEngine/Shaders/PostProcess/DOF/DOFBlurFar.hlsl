#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFColorCoCTex : register(t0);

#define MAX_RINGS 4
static const int SamplesPerRing[MAX_RINGS] = { 6, 10, 14, 18 };

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

    int targetRings = 1;
    if (centerCoC > 10.0f)
    {
        targetRings = 4;
    }
    else if (centerCoC > 6.0f)
    {
        targetRings = 3;
    }
    else if (centerCoC > 2.0f)
    {
        targetRings = 2;
    }

    float apertureBlades = clamp(round(DOFApertureBladeCount), 3.0f, 16.0f);
    float sampleJitter = InterleavedGradientNoise(input.position.xy);
    float polygonStrength = smoothstep(2.0f, 8.0f, centerCoC);

    float3 accumColor = center.rgb;
    float totalWeight = 1.0f;

    [loop]
    for (int ring = 1; ring <= targetRings; ++ring)
    {
        float radiusFraction = (float)ring / (float)targetRings;
        int sampleCount = SamplesPerRing[ring - 1];

        [loop]
        for (int s = 0; s < sampleCount; ++s)
        {
            float angle = (((float)s + sampleJitter) / (float)sampleCount) * DOFTwoPi;
            float polygonRadius = lerp(1.0f, PolygonBoundaryRadius(angle, apertureBlades), polygonStrength);

            float2 offset;
            sincos(angle, offset.y, offset.x);

            float2 sampleUV = uv + offset * polygonRadius * radiusFraction * centerCoC * DOFInvHalfResolution;
            float4 neighbor = DOFColorCoCTex.SampleLevel(LinearClampSampler, sampleUV, 0);
            float neighborCoC = max(neighbor.a, 0.0f);

            float cocWeight = saturate(neighborCoC / max(centerCoC, 0.001f));
            float ringWeight = lerp(1.0f, radiusFraction, 0.35f);
            float weight = cocWeight * ringWeight;

            accumColor += neighbor.rgb * weight;
            totalWeight += weight;
        }
    }

    return float4(accumColor / max(totalWeight, 0.0001f), saturate(centerCoC / max(DOFMaxCoCRadius, 0.001f)));
}
