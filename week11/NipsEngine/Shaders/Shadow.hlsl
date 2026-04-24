#include "Common.hlsl"

cbuffer ShadowBuffer : register(b2)
{
    row_major matrix PSMLightViewProj;
};

struct VSInput
{
    float3 Position : POSITION;
};

float4 ShadowVS(VSInput input) : SV_POSITION
{
    matrix viewProj = mul(View, Projection);
    
    float4 worldPos = mul(float4(input.Position, 1.0f), Model);
    
    float4 camClip = mul(worldPos, viewProj);
    float3 post = camClip.xyz / camClip.w;
    
    float4 shadowPos = mul(float4(post, 1.0f), PSMLightViewProj);
    return shadowPos;
}

void ShadowPS()
{
}
