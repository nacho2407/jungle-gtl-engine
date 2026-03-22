cbuffer FMeshUnlitConstants : register(b0)
{
    row_major float4x4 VP;
    float4 BaseColor;
};

struct VSInput
{
    float3 Position      : POSITION;

    row_major float4x4 World : WORLD;
    float4 InstanceColor : COLOR0;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

PSInput VSMain(VSInput In)
{
    PSInput Out;

    float4 WorldPosition = mul(float4(In.Position, 1.0f), In.World);
    Out.Position = mul(WorldPosition, VP);
    Out.Color = In.InstanceColor * BaseColor;

    return Out;
}

float4 PSMain(PSInput In) : SV_TARGET
{
    return In.Color;
}