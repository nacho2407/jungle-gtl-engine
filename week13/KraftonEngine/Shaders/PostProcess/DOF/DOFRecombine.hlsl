#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFFarBlurTex : register(t0);
Texture2D<float4> DOFNearBlurTex : register(t1);
Texture2D<float4> DOFBokehTex : register(t2);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{
    float2 uv = input.uv;

    float4 sharpColor = SceneColorTexture.SampleLevel(LinearClampSampler, uv, 0);
    float4 farBlur = DOFFarBlurTex.SampleLevel(LinearClampSampler, uv, 0);
    float4 nearBlur = DOFNearBlurTex.SampleLevel(LinearClampSampler, uv, 0);
    float3 bokeh = DOFBokehTex.SampleLevel(LinearClampSampler, uv, 0).rgb;

    float depth = SceneDepthTexture.SampleLevel(PointClampSampler, uv, 0).r;
    float signedCoC = CalculateSignedCoC(depth);

    float farFactor = signedCoC > 0.0f ? saturate(signedCoC / max(DOFMaxCoCRadius, 0.001f)) : 0.0f;
    farFactor = max(farFactor, farBlur.a);

    float3 color = lerp(sharpColor.rgb, farBlur.rgb, farFactor);
    color = lerp(color, nearBlur.rgb, nearBlur.a);
    color += bokeh;

    return float4(color, sharpColor.a);
}
