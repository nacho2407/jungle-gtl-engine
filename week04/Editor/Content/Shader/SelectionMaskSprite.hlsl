cbuffer FSpriteConstants : register(b0)
{
    row_major float4x4 VP;
};

Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

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

PSInput VSMain(VSInput In)
{
    PSInput Out;
    Out.Position = mul(float4(In.Position, 1.0f), VP);
    Out.UV = In.UV;
    Out.Color = In.Color;
    return Out;
}

float4 PSMain(PSInput In) : SV_TARGET
{
    float4 Sampled = SpriteTexture.Sample(SpriteSampler, In.UV);
    float Mask = max(max(Sampled.r, Sampled.g), max(Sampled.b, Sampled.a));
    return float4(1.0f, 1.0f, 1.0f, Mask * In.Color.a);
}
