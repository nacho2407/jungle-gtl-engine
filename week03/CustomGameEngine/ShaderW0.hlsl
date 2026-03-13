cbuffer MyConstants : register(b0)
{
    row_major matrix Model;
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
    
    float4 pos = mul(input.Pos, Model);
    pos = mul(pos, View);
    pos = mul(pos, Projection);
    output.Pos = pos;
    
    output.Color = input.Color;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}