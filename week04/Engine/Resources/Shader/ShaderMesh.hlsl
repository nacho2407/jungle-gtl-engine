cbuffer FMeshUnlitConstants : register(b0)
{
    row_major float4x4 MVP;
    float4 BaseColor;
};

struct VSInput
{
    float3 Position : POSITION;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

PSInput VSMain(VSInput In)
{
    PSInput Out;
    Out.Position = mul(float4(In.Position, 1.0f), MVP);
    Out.Color = BaseColor;
    return Out;
}

float4 PSMain(PSInput In) : SV_TARGET
{
    return In.Color;
}