cbuffer FFontConstants : register(b0)
{
    row_major float4x4 VP;
    float4 TintColor;
};

struct VSInput
{
    float3 Position : POSITION;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

Texture2D FontTexture : register(t0);
SamplerState FontSampler : register(s0);

PSInput VSMain(VSInput In)
{
    PSInput Out;
    Out.Position = mul(float4(In.Position, 1.0f), VP);
    Out.UV = In.UV;
    Out.Color = In.Color * TintColor;
    return Out;
}

float4 PSMain(PSInput In) : SV_TARGET
{
    float4 Sampled = FontTexture.Sample(FontSampler, In.UV);
    return Sampled * In.Color;
}