#include "Common.hlsl"

Texture2D SceneTexture : register(t0);
Texture2D DepthTexture : register(t1);
SamplerState DefaultSampler : register(s0);

struct PS_Input
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float LinearizeDepth(float depth)
{
    float A = Projection._43;
    float B = Projection._33;
    
    return A / (B - depth);
}

PS_Input VS(uint vertexId : SV_VertexID)
{
    PS_Input output;
    
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };
    
    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    return output;
}

float4 PS(PS_Input input) : SV_TARGET
{   
    float2 uv = input.position.xy / ViewportSize;
    
    float depth = DepthTexture.Sample(DefaultSampler, uv).r;
    
    float dist = LinearizeDepth(depth);
    
    float fogFactor = saturate((dist - FogStartDistance) / (FogEndDistance - FogStartDistance));

    float3 originalColor = SceneTexture.Sample(DefaultSampler, uv).rgb;
    float3 finalColor = lerp(originalColor, FogColor, fogFactor);
    
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}