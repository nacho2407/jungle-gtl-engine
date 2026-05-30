#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFColorCoCTex : register(t0);

#define MAX_RINGS 4
static const int SamplesPerRing[MAX_RINGS] = { 8, 12, 16, 20 };
static const float TWO_PI = 6.28318530f;

float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float PolygonBoundaryRadius(float angle, float bladeCount)
{
    bladeCount = clamp(bladeCount, 3.0f, 16.0f);
    float sectorAngle = TWO_PI / bladeCount;
    float localAngle = frac((angle + sectorAngle * 0.5f) / sectorAngle) * sectorAngle - sectorAngle * 0.5f;
    return cos(sectorAngle * 0.5f) / max(cos(localAngle), 0.001f);
}

float3 ExtractHighlight(float3 color)
{
    float threshold = max(DOFBokehThreshold, 0.0f);
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float highlightRatio = saturate((luminance - threshold) / max(luminance, 0.001f));
    return color * highlightRatio;
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{
    float2 uv = input.uv;
    float apertureBlades = clamp(round(DOFApertureBladeCount), 3.0f, 16.0f);
    float radiusScale = max(DOFBokehRadiusScale, 0.0f);
    float maxSampleRadius = max(DOFMaxCoCRadius * radiusScale, 0.0f);

    if (maxSampleRadius < 0.01f || DOFBokehIntensity <= 0.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float sampleJitter = InterleavedGradientNoise(input.position.xy);
    float3 accumColor = 0.0f;
    float totalWeight = 0.0f;

    [loop]
    for (int ring = 1; ring <= MAX_RINGS; ++ring)
    {
        float radiusFraction = (float)ring / (float)MAX_RINGS;
        int sampleCount = SamplesPerRing[ring - 1];

        [loop]
        for (int s = 0; s < sampleCount; ++s)
        {
            float angle = (((float)s + sampleJitter) / (float)sampleCount) * TWO_PI;
            float polygonRadius = PolygonBoundaryRadius(angle, apertureBlades);

            float2 direction;
            sincos(angle, direction.y, direction.x);

            float2 offsetPixels = direction * polygonRadius * radiusFraction * maxSampleRadius;
            float2 sampleUV = uv + offsetPixels * DOFInvHalfResolution;
            float4 neighbor = DOFColorCoCTex.SampleLevel(LinearClampSampler, sampleUV, 0);

            float neighborCoC = abs(neighbor.a);
            float neighborRadius = neighborCoC * radiusScale;
            float sampleDistance = length(offsetPixels);
            float reachWeight = saturate((neighborRadius - sampleDistance + 1.0f) * 0.5f);
            float cocWeight = saturate(neighborCoC / max(DOFMaxCoCRadius, 0.001f));
            float ringWeight = lerp(1.0f, radiusFraction, 0.35f);
            float weight = reachWeight * cocWeight * ringWeight;

            float3 highlight = ExtractHighlight(neighbor.rgb);
            accumColor += highlight * weight;
            totalWeight += weight;
        }
    }

    float3 bokeh = accumColor / max(totalWeight, 0.0001f);
    return float4(bokeh * max(DOFBokehIntensity, 0.0f), 0.0f);
}
