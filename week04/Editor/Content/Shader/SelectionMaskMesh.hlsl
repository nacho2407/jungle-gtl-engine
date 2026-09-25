cbuffer FMeshUnlitConstants : register(b0)
{
    row_major float4x4 MVP;
    float4 BaseColor;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD0;
};

struct PSInput
{
    float4 Position : SV_POSITION;
};

PSInput VSMain(VSInput In)
{
    PSInput Out;
    Out.Position = mul(float4(In.Position, 1.0f), MVP);
    return Out;
}

float4 PSMain(PSInput In) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
