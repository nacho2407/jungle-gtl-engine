#include "../Common.hlsl"

// GBuffer
Texture2D SceneColor : register(t0);
Texture2D SceneNormal : register(t1);
Texture2D SceneDepth : register(t2);
Texture2D SceneLightColor : register(t3);

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
    float depth = SceneDepth.Load(int3(ip, 0)).r;
    float4 lightColor = SceneLightColor.Load(int3(ip, 0));
    float T = exp(-FogDensity * depth);
    
    return lerp(FogColor, lightColor, T);
    
}