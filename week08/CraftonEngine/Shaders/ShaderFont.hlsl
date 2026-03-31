#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

PS_Input_Tex VS(VS_Input_PT input)
{
    PS_Input_Tex output;
    output.position = ApplyVP(input.position);
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float4 col = FontAtlas.Sample(FontSampler, input.texcoord);
    if (!bIsWireframe && ShouldDiscardFontPixel(col.r))
        discard;

    return float4(ApplyWireframe(col.rgb), bIsWireframe ? 1.0f : col.a);
}
