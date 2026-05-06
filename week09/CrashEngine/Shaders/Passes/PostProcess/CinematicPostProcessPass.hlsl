#include "../../Utils/Functions.hlsl"
#include "../../Resources/SystemResources.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

cbuffer CinematicPostProcessParams : register(b2)
{
    float4 FadeColor;
    float FadeAmount;
    float LetterBoxAmount;
    float Gamma;
    float VignetteIntensity;
    float VignetteRadius;
    float VignetteSoftness;
    float bEnableGammaCorrection;
    float bEnableVignette;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float3 ApplyVignette(float3 color, float2 uv)
{
    if (bEnableVignette < 0.5f || VignetteIntensity <= 0.0f)
    {
        return color;
    }

    float2 centeredUV = uv - 0.5f;
    float distanceFromCenter = length(centeredUV) * 2.0f;
    float edge = saturate((distanceFromCenter - VignetteRadius) / max(VignetteSoftness, 0.0001f));
    float factor = lerp(1.0f, 1.0f - saturate(VignetteIntensity), edge);
    return color * factor;
}

float3 ApplyGammaCorrection(float3 color)
{
    if (bEnableGammaCorrection < 0.5f)
    {
        return color;
    }

    return pow(saturate(color), 1.0f / max(Gamma, 0.0001f));
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float2 uv = input.uv;
    float4 color = SceneColor.SampleLevel(LinearClampSampler, uv, 0);

    color.rgb = ApplyVignette(color.rgb, uv);
    color = lerp(color, FadeColor, saturate(FadeAmount));
    color.rgb = ApplyGammaCorrection(color.rgb);

    float letterBox = saturate(LetterBoxAmount);
    if (letterBox > 0.0f && (uv.y < letterBox || uv.y > 1.0f - letterBox))
    {
        color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    return color;
}
