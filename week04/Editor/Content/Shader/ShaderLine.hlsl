cbuffer FLineConstants : register(b0)
{
    row_major float4x4 VP;
};

struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR0;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
};

PSInput VSMain(VSInput In)
{
    PSInput Out;
    Out.Position = mul(float4(In.Position, 1.0f), VP);
    Out.Color = In.Color;
    return Out;
}

float4 PSMain(PSInput In) : SV_TARGET
{
    return In.Color;
}