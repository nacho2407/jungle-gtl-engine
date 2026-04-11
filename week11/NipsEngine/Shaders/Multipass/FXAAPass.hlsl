#include "../Common.hlsl"

// Input
Texture2D FinalSceneColor : register(t0);
SamplerState SampleState : register(s0);

cbuffer FXAAConstants : register(b10)
{
    float2 InvResolution; // (1/Width, 1/Height)
    float Threshold; // 0.05 ~ 0.2 추천
    float Padding;
}

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
};

// Fullscreen Triangle
VSOutput mainVS(uint vertexID : SV_VertexID)
{
    VSOutput output;

    float2 pos;

    if (vertexID == 0)
        pos = float2(-1.0f, -1.0f);
    else if (vertexID == 1)
        pos = float2(-1.0f, 3.0f);
    else
        pos = float2(3.0f, -1.0f);

    output.ClipPos = float4(pos, 0.0f, 1.0f);
    return output;
}

float GetLuma(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

float4 mainPS(VSOutput input) : SV_TARGET
{
    int2 ip = int2(input.ClipPos.xy);

    // 중심
    float3 center = FinalSceneColor.Load(int3(ip, 0)).rgb;

    // 주변 샘플
    float3 left = FinalSceneColor.Load(int3(ip + int2(-1, 0), 0)).rgb;
    float3 right = FinalSceneColor.Load(int3(ip + int2(1, 0), 0)).rgb;
    float3 up = FinalSceneColor.Load(int3(ip + int2(0, -1), 0)).rgb;
    float3 down = FinalSceneColor.Load(int3(ip + int2(0, 1), 0)).rgb;

    // luma 계산
    float lumaCenter = GetLuma(center);
    float lumaMin = min(lumaCenter, min(min(GetLuma(left), GetLuma(right)), min(GetLuma(up), GetLuma(down))));
    float lumaMax = max(lumaCenter, max(max(GetLuma(left), GetLuma(right)), max(GetLuma(up), GetLuma(down))));

    float contrast = lumaMax - lumaMin;
    
    // edge 아닌 경우
    if (contrast < Threshold)
        return float4(center, 1.0f);

    // 방향 계산
    float edgeH = abs(GetLuma(left) - GetLuma(right));
    float edgeV = abs(GetLuma(up) - GetLuma(down));

    // edgeH > edgeV = 수평 edge
    int2 dir = (edgeH > edgeV) ? int2(1, 0) : int2(0, 1);

    // 방향 따라 샘플링
    float3 sample1 = FinalSceneColor.Load(int3(ip + dir, 0)).rgb;
    float3 sample2 = FinalSceneColor.Load(int3(ip - dir, 0)).rgb;

    // 평균
    float3 result = (sample1 + sample2) * 0.5;

    // center 유지 (과도 blur 방지)
    result = lerp(center, result, 0.7);

    // clamp (아티팩트 방지)
    float3 minColor = min(center, min(sample1, sample2));
    float3 maxColor = max(center, max(sample1, sample2));
    result = clamp(result, minColor, maxColor);

    return float4(result, 1.0f);
}