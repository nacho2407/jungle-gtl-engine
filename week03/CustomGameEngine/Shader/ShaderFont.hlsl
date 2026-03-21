cbuffer FrameConstant : register(b0)
{
    row_major matrix View;
    row_major matrix Projection;
};

struct VS_INPUT
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);
