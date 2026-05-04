/*
    subUV_sample.hlsl

    SubUV effect shader.
    - t0: SubUV atlas texture
    - b2: UVRegion, packed by SubUVSceneProxy from cell count and offsets
      UVRegion.xy = frame offset, UVRegion.zw = frame size
*/

#include "../../Utils/Functions.hlsl"
#include "../../Common/VertexLayouts.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

Texture2D SubUVAtlas : register(t0);

cbuffer SubUVRegionParams : register(b2)
{
    float4 UVRegion;
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

    float2 UV = UVRegion.xy + input.texcoord * UVRegion.zw;
    float4 color = SubUVAtlas.Sample(LinearWrapSampler, UV);
    if (!bIsWireframe && color.a <= 0.001f)
    {
        discard;
    }

    return float4(ApplyWireframe(color.rgb), bIsWireframe ? 1.0f : color.a);
}
