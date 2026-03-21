cbuffer FLineConstants : register(b0)
{
    float4x4 VP;
};

struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
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