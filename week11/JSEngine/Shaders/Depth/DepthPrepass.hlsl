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
        SkinnedPosition += BoneWeights[i] * mul(float4(Position, 1.0f), BonePalette[BoneIndices[i]]).xyz;
    }

    return SkinnedPosition;
}
#endif

float4 VS(VSInput input) : SV_POSITION
{
#ifdef GPU_SKINNING
    return ApplyMVP(Skinning(input.Position, input.BoneIndices, input.BoneWeights));
#else
    return ApplyMVP(input.Position);
#endif
}

void PS() {}
