#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D FontAtlas : register(t0);

PS_Input_Tex VS(VS_Input_PT input)
{
    PS_Input_Tex output;
    output.position = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float4 col = FontAtlas.Sample(PointClampSampler, input.texcoord);

    if (col.r < 0.1f)
        discard;

    return float4(0.6f, 1.0f, 1.0f, col.r);
}
