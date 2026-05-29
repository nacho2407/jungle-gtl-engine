#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFColorCoCTex : register(t0);

static const float2 Disk[16] = {
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870), float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38440307, 0.95628987), float2(0.20334582, -0.66986957),
    float2(0.11558417, 0.82333505), float2(0.18510671, 0.47451193),
    float2(-0.71677703, 0.13222270), float2(-0.23241232, -0.01105474),
    float2(0.47545300, 0.79539304), float2(0.51189273, -0.24621427),
    float2(0.67251712, 0.42839893), float2(0.70830405, -0.82433983)
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{
    float2 uv = input.uv;
    float4 center = DOFColorCoCTex.SampleLevel(LinearClampSampler, uv, 0);
    float centerCoC = abs(center.a);

    if (centerCoC < 0.01f)
        return center;

    float3 accumColor = center.rgb;
    float totalWeight = 1.0f;

    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        float2 sampleUV = uv + Disk[i] * centerCoC * DOFInvHalfResolution;
        float4 neighbor = DOFColorCoCTex.SampleLevel(LinearClampSampler, sampleUV, 0);
        float neighborCoC = abs(neighbor.a);

        float weight = saturate(neighborCoC / max(centerCoC, 0.001f));
        accumColor += neighbor.rgb * weight;
        totalWeight += weight;
    }

    return float4(accumColor / max(totalWeight, 0.0001f), center.a);
}
