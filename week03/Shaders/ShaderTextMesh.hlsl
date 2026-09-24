cbuffer FrameConstant : register(b0)
{
    row_major matrix View;
    row_major matrix Projection;
};

cbuffer ObjectConstant : register(b1)
{
    row_major matrix Model;
    float4 Color;
    int bIsSelected;
    float3 Padding;
};

Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

struct VS_INPUT
{
    float4 Pos : POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    matrix worldViewProj = mul(mul(Model, View), Projection);
    output.Pos = mul(input.Pos, worldViewProj);
    output.Color = input.Color * Color;
    output.UV = input.UV;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float mask = FontAtlas.Sample(FontSampler, input.UV).r;
    if (mask <= 0.01f)
    {
        discard;
    }

    float4 finalColor = float4(input.Color.rgb, input.Color.a * mask);
    if (bIsSelected > 0)
    {
        finalColor.rgb = saturate(finalColor.rgb + float3(0.2f, 0.2f, 0.2f));
    }

    return finalColor;
}
