#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFBlurTex : register(t0);
Texture2D<float4> DOFBokehTex : register(t1);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{ 
    float2 uv = input.uv;

    float4 sharpColor = SceneColorTexture.SampleLevel(LinearClampSampler, uv, 0);
    float4 blurColor = DOFBlurTex.SampleLevel(LinearClampSampler, uv, 0);
    float3 bokehColor = DOFBokehTex.SampleLevel(LinearClampSampler, uv, 0).rgb;
    float depth = SceneDepthTexture.SampleLevel(PointClampSampler, uv, 0).r;
    float coc = abs(CalculateSignedCoC(depth));
    float blendFactor = saturate(coc / max(DOFMaxCoCRadius, 0.001f));

    float3 color = lerp(sharpColor.rgb, blurColor.rgb, blendFactor) + bokehColor;
    return float4(color, sharpColor.a);
}
