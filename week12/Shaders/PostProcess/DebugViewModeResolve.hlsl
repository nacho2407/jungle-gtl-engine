#include "../Common/Common.hlsli"

Texture2D SceneDepth : register(t0);

SamplerState SampleState : register(s0);

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
};

cbuffer DebugViewModeResolveConstants : register(b7)
{
    uint ViewMode;
    float3 Padding;
};

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

float4 mainPS(VSOutput input) : SV_TARGET
{
    int2 ip = int2(input.ClipPos.xy);
    float depth = SceneDepth.Load(int3(ip, 0)).r;

    if (depth >= 1.0f)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    float visual;
    if (Projection[3][3] < 0.5f)
    {
        float A = Projection[0][2];
        float B = Projection[3][2];
        float zView = abs(B / (depth - A));

        float DepthDensity = 0.05f;
        visual = saturate(exp(-zView * DepthDensity));
    }
    else
    {
        visual = 1.0f - depth;
    }

    return float4(visual, visual, visual, 1.0f);
}
