cbuffer FrameConstant : register(b0)
{
    row_major matrix View;
    row_major matrix Projection;
};

struct VS_INPUT
{
    float4 Pos : POSITION;
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
    
    matrix worldViewProj = mul(View, Projection);
    
    float4 pos = mul(input.Pos, worldViewProj);
    output.Pos = pos;
    
    output.Color = input.Color;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 finalColor = input.Color;
    return finalColor;
}