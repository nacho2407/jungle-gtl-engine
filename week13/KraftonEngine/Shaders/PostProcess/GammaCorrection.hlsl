#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

cbuffer GammaCorrectionCB : register(b2)
{
    float Gamma;
    float Exposure;
    float2 _GammaPad;
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

float3 ACESFilm(float3 color)
{
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;
    return saturate((color * (A * color + B)) / (color * (C * color + D) + E));
}

float3 ApplyToneMapping(float3 hdrColor)
{
    hdrColor = max(hdrColor, 0.0f) * max(Exposure, 0.0f);
    return ACESFilm(hdrColor);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    float3 toneMapped = ApplyToneMapping(sceneColor.rgb);
    return float4(LinearToSRGB(toneMapped), sceneColor.a);
}
