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
    
    matrix worldViewProj = mul(mul(Model, View), Projection);
    
    float4 pos = mul(input.Pos, worldViewProj);
    output.Pos = pos;
    
    output.Color = input.Color;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 finalColor = input.Color * Color;
    
    if (bIsSelected > 0)
    {
        return float4(finalColor.xyz, finalColor.w * 0.5f); // 선택 시 투명화.
    }
    return finalColor;
}