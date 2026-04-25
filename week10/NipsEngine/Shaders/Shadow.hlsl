#include "Common.hlsl"

struct VSInput
{
    float3 Position : POSITION;
};

float4 ShadowVS(VSInput input) : SV_POSITION
{
    float4 worldPos = mul(float4(input.Position, 1.0f), Model);
    
    float4 camClip = mul(worldPos, VirtualViewProj);
    float3 post = camClip.xyz / camClip.w;
    
    float4 shadowPos = mul(float4(post, 1.0f), DirLightViewProj);
    return shadowPos;
}

void ShadowPS()
{
}
