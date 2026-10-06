#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{
    float2 uv = input.uv;

    float2 offsets[4] =
    {
        float2(-0.5f, -0.5f),
        float2( 0.5f, -0.5f),
        float2(-0.5f,  0.5f),
        float2( 0.5f,  0.5f)
    };

    float nearCoC = 0.0f;
    float farCoC = 0.0f;
    float3 weightedColor = 0.0f;
    float totalWeight = 0.0f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float2 sampleUV = uv + offsets[i] * DOFInvFullResolution;
        float depth = SceneDepthTexture.SampleLevel(PointClampSampler, sampleUV, 0).r;
        float coc = CalculateSignedCoC(depth);
        float3 color = SceneColorTexture.SampleLevel(LinearClampSampler, sampleUV, 0).rgb;

        nearCoC = min(nearCoC, min(coc, 0.0f));
        farCoC = max(farCoC, max(coc, 0.0f));

        float cocWeight = saturate(abs(coc) / max(DOFMaxCoCRadius, 0.001f));
        float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
        float highlightWeight = saturate(luminance / max(DOFBokehThreshold, 1.0f));
        float weight = 1.0f + cocWeight * 3.0f + highlightWeight;

        weightedColor += color * weight;
        totalWeight += weight;
    }

    float coc = DominantSignedCoC(nearCoC, farCoC);
    float3 color = weightedColor / max(totalWeight, 0.0001f);

    return float4(color, coc);
}
