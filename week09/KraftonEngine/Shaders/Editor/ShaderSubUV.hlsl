#include "../Common/Utils/Functions.hlsl"
#include "../Common/Geometry/VertexLayouts.hlsl"
#include "../Common/Resources/SystemSamplers.hlsl"

Texture2D SubUVAtlas : register(t0);

// b2 (PerShader0): SubUV UV region (atlas frame offset + size)
cbuffer SubUVRegionBuffer : register(b2)
{
    float4 UVRegion; // xy = offset, zw = size
}

PS_Input_Tex VS(VS_Input_PNCT input)
{
    PS_Input_Tex output;
    output.position = ApplyMVP(input.position);
    output.texcoord = UVRegion.xy + input.texcoord * UVRegion.zw;
    return output;
}

float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float4 col = SubUVAtlas.Sample(LinearClampSampler, input.texcoord);
    if (!bIsWireframe && ShouldDiscardFontPixel(col.r))
        discard;

    return float4(ApplyWireframe(col.rgb), bIsWireframe ? 1.0f : col.a);
}
