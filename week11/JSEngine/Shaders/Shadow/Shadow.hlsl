#include "../Common/Common.hlsli"

struct VSInput
{
    float3 Position : POSITION;
#ifdef GPU_SKINNING
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
#endif
};

#ifdef GPU_SKINNING
float3 Skinning(float3 Position, uint4 BoneIndices, float4 BoneWeights)
{
    float3 SkinnedPosition = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < 4; i++)
    {
        SkinnedPosition += BoneWeights[i] * mul(float4(Position, 1.0f), BoneMatrices[BoneIndices[i]]).xyz;
    }

    return SkinnedPosition;
}
#endif

float4 VS(VSInput input) : SV_POSITION
{
#ifdef GPU_SKINNING
    float4 worldPos = mul(float4(Skinning(input.Position, input.BoneIndices, input.BoneWeights), 1.0f), Model);
#else
    float4 worldPos = mul(float4(input.Position, 1.0f), Model);
#endif
    float4 post = worldPos;

#ifdef SHADOW_MAP_PSM
    float4 camClip = mul(post, VirtualViewProj);
    if (abs(camClip.w) > 1e-5f)
    {
        post = float4(camClip.xyz / camClip.w, 1.0f);
    }
#endif

    float4 shadowPos = mul(post, ShadowViewProj);
    return shadowPos;
}

void PS()
{
}
