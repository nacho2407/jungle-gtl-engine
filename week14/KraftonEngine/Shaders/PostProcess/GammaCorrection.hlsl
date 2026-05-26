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

float3 ACESFilm(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0);

    float exposure = 1.0f; // 나중에 CB로 빼면 좋음
    float3 mapped = ACESFilm(sceneColor.rgb * exposure);

    return float4(LinearToSRGB(mapped), sceneColor.a);
}