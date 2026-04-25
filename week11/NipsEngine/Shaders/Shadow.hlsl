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
    float4 camClip = mul(worldPos, VirtualViewProj);
    if (abs(camClip.w) > 1e-5f)
    {
        post = float4(camClip.xyz / camClip.w, 1.0f);
    }
#endif

    float4 shadowPos = mul(post, DirLightViewProj);
    return shadowPos;
}

void ShadowPS()
{
}
