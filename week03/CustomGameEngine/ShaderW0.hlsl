cbuffer MyConstants : register(b0)
{
    row_major matrix WorldMatrix;
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
    
    output.Pos = mul(input.Pos, WorldMatrix);
    
    output.Color = input.Color;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}