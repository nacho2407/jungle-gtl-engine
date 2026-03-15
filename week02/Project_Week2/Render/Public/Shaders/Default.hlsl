cbuffer Constants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4 BaseColor;
};

struct VS_INPUT
{
    float3 Pos : POSITION;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.Pos, 1.0f), World);
    float4 viewPos = mul(worldPos, View);
    output.Pos = mul(viewPos, Projection);
    output.Color = input.Color * BaseColor;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}