cbuffer FShaderConstants : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    float4 Color;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 VertexColor : COLOR;
};

float4 mainHPPS(PSInput Input) : SV_TARGET
{
    return Color;
}
