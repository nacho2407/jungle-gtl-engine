#include "../Common/Common.hlsli"

cbuffer SelectionMaskBuffer : register(b12)
{
    uint UseAlphaTest;
    float AlphaCutoff;
    float2 UVOffset;
    float2 UVScale;
}

Texture2D MaskTexture : register(t0);
SamplerState MaskSampler : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
#if defined(VF_MESH)
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
#endif
#ifdef GPU_SKINNING
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
#endif
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

#ifdef GPU_SKINNING
float3 Skinning(float3 Position, uint4 BoneIndices, float4 BoneWeights)
{
    float3 SkinnedPosition = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < 4; ++i)
    {
        SkinnedPosition += BoneWeights[i] * mul(float4(Position, 1.0f), BonePalette[BoneIndices[i]]).xyz;
    }

    return SkinnedPosition;
}
#endif

VSOutput VS(VSInput Input)
{
    VSOutput Output;

#ifdef GPU_SKINNING
    float3 Position = Skinning(Input.Position, Input.BoneIndices, Input.BoneWeights);
#else
    float3 Position = Input.Position;
#endif

    Output.Position = ApplyMVP(Position);

#if defined(VF_BILLBOARD)
    float2 LocalUV = float2(0.5f - Input.Position.y, 0.5f - Input.Position.z);
    Output.UV = UVOffset + LocalUV * UVScale;
#elif defined(VF_MESH)
    Output.UV = UVOffset + Input.UV * UVScale;
#else
    Output.UV = float2(0.0f, 0.0f);
#endif

    return Output;
}

float4 PS(VSOutput Input) : SV_TARGET
{
    if (UseAlphaTest != 0)
    {
        float Alpha = MaskTexture.Sample(MaskSampler, Input.UV).a;
        if (Alpha <= AlphaCutoff)
        {
            discard;
        }
    }

    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
