Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PSInput VS(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 col = FontAtlas.Sample(FontSampler, input.texCoord);

    if (col.r < 0.1f)
    {
        discard;
    }

    return float4(1.0f, 1.0f, 1.0f, col.r);
}