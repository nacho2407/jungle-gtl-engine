#include "../Common.hlsl"

// GBuffer
Texture2D SceneColor : register(t0);
Texture2D SceneNormal : register(t1);
Texture2D SceneDepth : register(t2);
Texture2D SceneLightColor : register(t3);
Texture2D SceneWorldPos : register(t4);

SamplerState SampleState : register(s0);

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
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
    float rawDepth = SceneDepth.Load(int3(ip, 0)).r;
    float4 lightColor = SceneLightColor.Load(int3(ip, 0));
    float FogStart = 0;

    if (FogDensity <= 0.0f)
        return lightColor;

    float3 worldPos = SceneWorldPos.Load(int3(ip, 0)).xyz;

    float dist = length(worldPos - CameraPosition.xyz);
        
    float scaledDensity = FogDensity * 0.1; // 스케일 조정
    float effectiveDist = max(dist - FogStart, 0.0f); // 시작 거리
    float heightFactor = exp(-HeightFalloff * max(worldPos.z - CameraPosition.z, 0.0f));
    float fogAmount = 1.0f - exp(-scaledDensity * heightFactor * effectiveDist);
    fogAmount = saturate(fogAmount);

    return lerp(lightColor, FogColor, fogAmount);
}