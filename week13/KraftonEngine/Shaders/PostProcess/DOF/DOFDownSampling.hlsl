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

    float depth = SceneDepthTexture.SampleLevel(PointClampSampler, uv, 0).r;
    float coc = CalculateSignedCoC(depth);
    float3 color = SceneColorTexture.SampleLevel(LinearClampSampler, uv, 0).rgb;

    return float4(color, coc);
}
