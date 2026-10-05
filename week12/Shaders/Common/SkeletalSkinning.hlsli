#ifndef SKELETAL_SKINNING_H
#define SKELETAL_SKINNING_H

#define MAX_GPU_SKIN_BONE_COUNT 512

cbuffer BoneMatrixBuffer : register(b5)
{
    row_major float4x4 BoneMatrices[MAX_GPU_SKIN_BONE_COUNT];
    uint BoneCount;
    float3 BoneMatrixBufferPadding;
};

struct FSkinningResult
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
};

float3 NormalizeOrFallback(float3 Value, float3 Fallback)
{
    float LenSq = dot(Value, Value);
    if (LenSq <= 1.0e-8f)
    {
        return Fallback;
    }

    return Value * rsqrt(LenSq);
}

FSkinningResult ApplyLinearBlendSkinning(
    float3 Position,
    float3 Normal,
    float3 Tangent,
    uint4 BoneIndices,
    float4 BoneWeights)
{
    FSkinningResult Result;

    if (BoneCount == 0)
    {
        Result.Position = Position;
        Result.Normal = Normal;
        Result.Tangent = Tangent;
        return Result;
    }

    float ValidWeightSum = 0.0f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if (BoneWeights[i] > 0.0f && BoneIndices[i] < BoneCount)
        {
            ValidWeightSum += BoneWeights[i];
        }
    }

    if (ValidWeightSum <= 1.0e-6f)
    {
        Result.Position = Position;
        Result.Normal = Normal;
        Result.Tangent = Tangent;
        return Result;
    }

    float3 SkinnedPosition = float3(0.0f, 0.0f, 0.0f);
    float3 SkinnedNormal = float3(0.0f, 0.0f, 0.0f);
    float3 SkinnedTangent = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int j = 0; j < 4; ++j)
    {
        uint BoneIndex = BoneIndices[j];
        float RawWeight = BoneWeights[j];

        if (RawWeight <= 0.0f || BoneIndex >= BoneCount)
        {
            continue;
        }

        float Weight = RawWeight / ValidWeightSum;
        float4x4 SkinMatrix = BoneMatrices[BoneIndex];

        SkinnedPosition += mul(float4(Position, 1.0f), SkinMatrix).xyz * Weight;
        SkinnedNormal += mul(float4(Normal, 0.0f), SkinMatrix).xyz * Weight;
        SkinnedTangent += mul(float4(Tangent, 0.0f), SkinMatrix).xyz * Weight;
    }

    Result.Position = SkinnedPosition;
    Result.Normal = NormalizeOrFallback(SkinnedNormal, Normal);
    Result.Tangent = NormalizeOrFallback(SkinnedTangent, Tangent);
    return Result;
}

#endif
