#include "../Common/Common.hlsli"
#include "../Common/SkeletalSkinning.hlsli"

struct VSInput
{
    float3 Position : POSITION;
};

struct VSSkeletalInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
    float4 Color : COLOR;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

float4 DepthPrepassVS(VSInput input) : SV_POSITION
{
    return ApplyMVP(input.Position);
}

float4 SkeletalDepthPrepassVS(VSSkeletalInput input) : SV_POSITION
{
    FSkinningResult Skinned = ApplyLinearBlendSkinning(
        input.Position,
        input.Normal,
        input.Tangent.xyz,
        input.BoneIndices,
        input.BoneWeights);

    return ApplyMVP(Skinned.Position);
}

void DepthPrepassPS() {}
