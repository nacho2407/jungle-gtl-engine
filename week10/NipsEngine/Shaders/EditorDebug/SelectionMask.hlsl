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

struct VSInputPrimitive
{
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct VSInputStaticMesh
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
};

struct VSInputSkeletalMesh
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
    float4 Color : COLOR;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

struct VSOutputPrimitive
{
    float4 Position : SV_POSITION;
};

struct VSOutputTextured
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VSOutputPrimitive VSPrimitive(VSInputPrimitive Input)
{
    VSOutputPrimitive Output;
    Output.Position = ApplyMVP(Input.Position);
    return Output;
}

VSOutputTextured VSStaticMesh(VSInputStaticMesh Input)
{
    VSOutputTextured Output;
    Output.Position = ApplyMVP(Input.Position);
    Output.UV = UVOffset + Input.UV * UVScale;
    return Output;
}

VSOutputTextured VSSkeletalMesh(VSInputSkeletalMesh Input)
{
    VSOutputTextured Output;
    Output.Position = ApplyMVP(Input.Position);
    Output.UV = UVOffset + Input.UV * UVScale;
    return Output;
}

VSOutputTextured VSBillboard(VSInputPrimitive Input)
{
    VSOutputTextured Output;
    Output.Position = ApplyMVP(Input.Position);
    float2 LocalUV = float2(0.5f - Input.Position.y, 0.5f - Input.Position.z);
    Output.UV = UVOffset + LocalUV * UVScale;
    return Output;
}

float4 PSPrimitive(VSOutputPrimitive Input) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}

float4 PSTextured(VSOutputTextured Input) : SV_TARGET
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
