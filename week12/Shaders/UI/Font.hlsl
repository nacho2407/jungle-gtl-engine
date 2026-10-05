#include "../Common/Common.hlsli"

Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

struct VSInput
{
    float3 position : POSITION; // CPU Billboard ???붾뱶 醫뚰몴
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
    output.position = mul(mul(float4(input.position, 1.0f), View), Projection);
    output.texCoord = input.texCoord;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 col = FontAtlas.Sample(FontSampler, input.texCoord);
    if (bIsWireframe < 0.5f)
    {
        if (col.r < 0.1f)
        {
            discard; // 寃? 諛곌꼍 ?쒓굅
        }
        return col;
    }

    return float4(WireframeRGB, 1.0f);

}
