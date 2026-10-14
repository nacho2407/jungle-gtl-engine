#ifndef SKINNING_HLSLI
#define SKINNING_HLSLI

struct FSkinMatrix
{
    float4 Row0;
    float4 Row1;
    float4 Row2;
    float4 Row3;
};

struct FSkinningResult
{
    float4 position;
    float3 normal;
    float3 tangent;
    float accumWeight;
};


// StructedBuffer가 RowMajor로 해석해서 나중에 Transfopose하면 추후 헷갈리니까 Mul을 새로 정의했음
StructuredBuffer<FSkinMatrix> SkinMatrices : register(t13);

float4 MulSkinMatrix(float4 v, FSkinMatrix m)
{
    return v.x * m.Row0 + v.y * m.Row1 + v.z * m.Row2 + v.w * m.Row3;
}

FSkinningResult ApplyLinearBlendSkinning(
    float3 position,
    float3 normal,
    float3 tangent,
    int4 boneIndices,
    float4 boneWeights)
{
    FSkinningResult result;
    result.position = float4(0.0f, 0.0f, 0.0f, 0.0f);
    result.normal = float3(0.0f, 0.0f, 0.0f);
    result.tangent = float3(0.0f, 0.0f, 0.0f);
    result.accumWeight = 0.0f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        int boneIndex = boneIndices[i];
        float weight = boneWeights[i];

        if (boneIndex >= 0 && weight > 0.0f)
        {
            FSkinMatrix skinMatrix = SkinMatrices[boneIndex];

            result.position += weight * MulSkinMatrix(float4(position, 1.0f), skinMatrix);
            result.normal += weight * MulSkinMatrix(float4(normal, 0.0f), skinMatrix).xyz;
            result.tangent += weight * MulSkinMatrix(float4(tangent, 0.0f), skinMatrix).xyz;
            result.accumWeight += weight;
        }
    }

    if (result.accumWeight <= 0.0f)
    {
        result.position = float4(position, 1.0f);
        result.normal = normal;
        result.tangent = tangent;
    }

    return result;
}

float GetBoneInfluenceWeight(int4 boneIndices, float4 boneWeights, int selectedBoneIndex)
{
    float selectedWeight = 0.0f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if (boneIndices[i] == selectedBoneIndex)
        {
            selectedWeight += boneWeights[i];
        }
    }

    return selectedWeight;
}

#endif