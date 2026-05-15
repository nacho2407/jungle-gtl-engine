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

float4 ProjectShadowPosition(float3 position)
{
    float4 worldPos = mul(float4(position, 1.0f), Model);
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

float4 ShadowVS(VSInput input) : SV_POSITION
{
    return ProjectShadowPosition(input.Position);
}

float4 SkeletalShadowVS(VSSkeletalInput input) : SV_POSITION
{
    FSkinningResult Skinned = ApplyLinearBlendSkinning(
        input.Position,
        input.Normal,
        input.Tangent.xyz,
        input.BoneIndices,
        input.BoneWeights);

    return ProjectShadowPosition(Skinned.Position);
}

void ShadowPS()
{
}
