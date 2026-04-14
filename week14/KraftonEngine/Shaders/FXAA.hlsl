// FXAA.hlsl
#include "Common/Functions.hlsl"
#include "Common/SystemResources.hlsl"
#include "Common/SystemSamplers.hlsl"

cbuffer FXAABuffer : register(b2)
{
    float EdgeThreshold;
    float EdgeThresholdMin;
    float2 _Pad;
};

// SceneColor (t11) is declared in Common/SystemResources.hlsl
#define ColorTex SceneColor
#define Sampler LinearClampSampler

float GetLuma(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 color = ColorTex.SampleLevel(Sampler, input.uv, 0);
    float lumaM = GetLuma(color.rgb);
    
    uint Width, Height;
    ColorTex.GetDimensions(Width, Height);
    
    float2 TexelSize = { 1.0f / Width, 1.0f / Height };
    
    float lumaN = GetLuma(ColorTex.SampleLevel(Sampler, input.uv + float2(0, -TexelSize.y), 0).rgb);
    float lumaS = GetLuma(ColorTex.SampleLevel(Sampler, input.uv + float2(0, TexelSize.y), 0).rgb);
    float lumaW = GetLuma(ColorTex.SampleLevel(Sampler, input.uv + float2(-TexelSize.x, 0), 0).rgb);
    float lumaE = GetLuma(ColorTex.SampleLevel(Sampler, input.uv + float2(TexelSize.x, 0), 0).rgb);
    
    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float lumaRange = lumaMax - lumaMin;
    
    if (lumaRange < max(EdgeThresholdMin, lumaMax * EdgeThreshold))
        return color;
    
    float lumaNW = GetLuma(ColorTex.SampleLevel(Sampler, input.uv + float2(-TexelSize.x, -TexelSize.y), 0).rgb);
    float lumaNE = GetLuma(ColorTex.SampleLevel(Sampler, input.uv + float2(TexelSize.x, -TexelSize.y), 0).rgb);
    float lumaSW = GetLuma(ColorTex.SampleLevel(Sampler, input.uv + float2(-TexelSize.x, TexelSize.y), 0).rgb);
    float lumaSE = GetLuma(ColorTex.SampleLevel(Sampler, input.uv + float2(TexelSize.x, TexelSize.y), 0).rgb);
    
    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25f * 0.125f, 1.0f / 128.0f);
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -8.0f, 8.0f) * TexelSize;

    float4 rgbA = 0.5f * (
        ColorTex.SampleLevel(Sampler, input.uv + dir * (1.0f / 3.0f - 0.5f), 0) +
        ColorTex.SampleLevel(Sampler, input.uv + dir * (2.0f / 3.0f - 0.5f), 0));

    float4 rgbB = rgbA * 0.5f + 0.25f * (
        ColorTex.SampleLevel(Sampler, input.uv + dir * -0.5f, 0) +
        ColorTex.SampleLevel(Sampler, input.uv + dir * 0.5f, 0));

    float lumaB = GetLuma(rgbB.rgb);
    if (lumaB < lumaMin || lumaB > lumaMax)
        return rgbA;

    return rgbB;
}