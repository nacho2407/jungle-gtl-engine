#include "Common.hlsl"

struct VSInput
{
    float3 Position : POSITION;
};

float4 ShadowVS(VSInput input) : SV_POSITION
{
    float4 worldPos = mul(float4(input.Position, 1.0f), Model);
    
    float4 post = worldPos;
#ifdef SHADOW_MAP_PSM
    matrix viewProj = mul(View, Projection);
    float4 camClip = mul(worldPos, viewProj);
    post = camClip
#endif
    
    float4 shadowPos = mul(post, DirLightViewProj);
    return shadowPos;
}

void ShadowPS()
{
}
