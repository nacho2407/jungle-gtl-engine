#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFColorCoCTex : register(t0);

#define MAX_RINGS 2
static const int SamplesPerRing[MAX_RINGS] = { 4, 8 };
static const float TWO_PI = 6.28318530f;

float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{
    float2 uv = input.uv;
    float4 center = DOFColorCoCTex.SampleLevel(LinearClampSampler, uv, 0);
    float signedCenterCoC = center.a;
    float centerCoC = abs(signedCenterCoC);

    if (centerCoC < 0.01f)
        return center;

    float3 accumColor = center.rgb;
    float totalWeight = 1.0f;

    int targetRings = (centerCoC > 4.0f) ? 2 : 1;
    float rotation = InterleavedGradientNoise(input.position.xy) * TWO_PI;

    [loop]
    for (int ring = 1; ring <= targetRings; ++ring)
    {
        float radiusFraction = (float)ring / (float)targetRings;
        int sampleCount = SamplesPerRing[ring - 1];

        [loop]
        for (int s = 0; s < sampleCount; ++s)
        {
            float angle = ((float)s / (float)sampleCount) * TWO_PI + rotation;

            float2 offset;
            sincos(angle, offset.y, offset.x);

            float2 sampleUV = uv + offset * radiusFraction * centerCoC * DOFInvHalfResolution;
            float4 neighbor = DOFColorCoCTex.SampleLevel(LinearClampSampler, sampleUV, 0);
            float neighborCoC = abs(neighbor.a);

            float cocWeight = saturate(neighborCoC / max(centerCoC, 0.001f));
            float sideWeight = (signedCenterCoC * neighbor.a >= 0.0f) ? 1.0f : 0.15f;
            float ringWeight = radiusFraction;
            float weight = cocWeight * sideWeight * ringWeight;

            accumColor += neighbor.rgb * weight;
            totalWeight += weight;
        }
    }

    return float4(accumColor / max(totalWeight, 0.0001f), signedCenterCoC);
}
