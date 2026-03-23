cbuffer Constants : register(b0)
{
    // @@@ 넘기는 인자 보고 결정해야함.
    // 아마도 ViewProj일 거 같음
    row_major float4x4 View;
    row_major float4x4 Projection;
}

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
    
    float4 worldPos = float4(input.Pos, 1.0f);
    float4 viewPos = mul(worldPos, View);
    output.Pos = mul(viewPos, Projection);
    output.Color = input.Color;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}