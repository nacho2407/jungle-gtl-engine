#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float3 color = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0).rgb;

    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float threshold = 1.2f;
    float softKnee = 0.6f;

    float knee = threshold * softKnee;
    float soft = luminance - threshold + knee;
    soft = clamp(soft, 0.0f, 2.0f * knee);
    soft = soft * soft / max(4.0f * knee, 0.0001f);

    float contribution = max(soft, luminance - threshold);
    contribution /= max(luminance, 0.0001f);

    float3 desaturated = lerp(color, luminance.xxx, 0.75f);
    float3 bloom = desaturated * contribution;

    return float4(bloom, 1.0f);
}
