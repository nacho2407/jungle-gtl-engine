#include "../Common/Utils/Functions.hlsl"
#include "../Common/Geometry/VertexLayouts.hlsl"
#include "../Common/Resources/SystemSamplers.hlsl"

Texture2D FontAtlas : register(t0);

PS_Input_Tex VS(VS_Input_PT input)
{
    PS_Input_Tex output;
    output.position = ApplyVP(input.position);
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float4 col = FontAtlas.Sample(PointClampSampler, input.texcoord);
    float coverage = col.r;
    if (!bIsWireframe && ShouldDiscardFontPixel(coverage))
        discard;

    return float4(ApplyWireframe(float3(1.0f, 1.0f, 1.0f)), bIsWireframe ? 1.0f : coverage);
}
