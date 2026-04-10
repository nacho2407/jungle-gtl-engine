#include "../Common.hlsl"

// GBuffer
Texture2D SceneColor : register(t0);
Texture2D SceneNormal : register(t1);
Texture2D SceneDepth : register(t2);

SamplerState SampleState : register(s0);

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
};

// Fullscreen Triangle VS
VSOutput mainVS(uint vertexID : SV_VertexID)
{
    VSOutput output;

    float2 pos;

    if (vertexID == 0)
    {
        pos = float2(-1.0f, -1.0f);
    }
    else if (vertexID == 1)
    {
        pos = float2(-1.0f, 3.0f);
    }
    else // 2
    {
        pos = float2(3.0f, -1.0f);
    }

    output.ClipPos = float4(pos, 0.0f, 1.0f);
    return output;
}

// PS
float4 mainPS(VSOutput input) : SV_TARGET
{
    int2 ip = int2(input.ClipPos.xy);
    float3 albedo = SceneColor.Load(int3(ip, 0)).rgb;
    float3 normal = SceneNormal.Load(int3(ip, 0)).rgb;
    float depth = SceneDepth.Load(int3(ip, 0)).r;

    return float4(albedo, 1.0f);
    //return float4(normal, 1.0f);
    
    // float visual = 1.0f - depth;
    // return float4(visual, visual, visual, 1.0f);
}