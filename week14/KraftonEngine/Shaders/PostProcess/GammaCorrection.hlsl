#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

cbuffer GammaCorrectionCB : register(b2)
{
    float Gamma;
    float3 _GammaPad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float3 LinearToSRGB(float3 color)
{
    color = max(color, 0.0f);
    float3 low = color * 12.92f;
    float safeGamma = max(Gamma, 0.01f);
    float3 high = 1.055f * pow(color, 1.0f / safeGamma) - 0.055f;
    return lerp(low, high, step(0.0031308f, color));
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    return float4(LinearToSRGB(sceneColor.rgb), sceneColor.a);
}
