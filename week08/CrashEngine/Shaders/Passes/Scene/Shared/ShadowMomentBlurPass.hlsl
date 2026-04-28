/*
    ShadowMomentBlurPass.hlsl
    Separable Gaussian blur for VSM moment textures (mip0 prefilter).
*/

// Slice 하나만 보더라도 SRV는 Texture2DArray view로 만들어져 있으므로 타입을 맞춰줍니다.
Texture2DArray<float2> g_InputMoment : register(t0);
SamplerState LinearClampSampler : register(s0);

cbuffer MomentBlurParams : register(b2)
{
    float2 BlurTexelSize;
    float2 Padding;
}

struct FVSOut
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

FVSOut VS(uint VertexID : SV_VertexID)
{
    FVSOut Out;

    float2 Pos;
    Pos.x = (VertexID == 2) ? 3.0f : -1.0f;
    Pos.y = (VertexID == 1) ? 3.0f : -1.0f;

    Out.Position = float4(Pos, 0.0f, 1.0f);
    Out.UV = float2(0.5f * (Pos.x + 1.0f), 1.0f - 0.5f * (Pos.y + 1.0f));
    return Out;
}

float2 SampleMoment(float2 UV)
{
    return g_InputMoment.SampleLevel(LinearClampSampler, float3(UV, 0.0f), 0.0f).rg;
}

float2 PS_Horizontal(FVSOut Input) : SV_TARGET0
{
    const float2 Offset1 = float2(1.3846153846f * BlurTexelSize.x, 0.0f);
    const float2 Offset2 = float2(3.2307692308f * BlurTexelSize.x, 0.0f);

    float2 Sum = SampleMoment(Input.UV) * 0.2270270270f;
    Sum += SampleMoment(Input.UV + Offset1) * 0.3162162162f;
    Sum += SampleMoment(Input.UV - Offset1) * 0.3162162162f;
    Sum += SampleMoment(Input.UV + Offset2) * 0.0702702703f;
    Sum += SampleMoment(Input.UV - Offset2) * 0.0702702703f;
    return Sum;
}

float2 PS_Vertical(FVSOut Input) : SV_TARGET0
{
    const float2 Offset1 = float2(0.0f, 1.3846153846f * BlurTexelSize.y);
    const float2 Offset2 = float2(0.0f, 3.2307692308f * BlurTexelSize.y);

    float2 Sum = SampleMoment(Input.UV) * 0.2270270270f;
    Sum += SampleMoment(Input.UV + Offset1) * 0.3162162162f;
    Sum += SampleMoment(Input.UV - Offset1) * 0.3162162162f;
    Sum += SampleMoment(Input.UV + Offset2) * 0.0702702703f;
    Sum += SampleMoment(Input.UV - Offset2) * 0.0702702703f;
    return Sum;
}
