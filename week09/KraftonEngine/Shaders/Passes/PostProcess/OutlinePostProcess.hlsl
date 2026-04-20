// OutlinePostProcess.hlsl
// Fullscreen Triangle VS + stencil-based outline PS.

#include "../../Common/Utils/Functions.hlsl"
#include "../../Common/Resources/SystemResources.hlsl"

cbuffer OutlinePostProcessCB : register(b2)
{
    float4 OutlineColor;
    float OutlineThickness;
    float3 _Pad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    const int2 coord = int2(input.position.xy);
    const int offset = max((int)OutlineThickness, 1);

    const uint center = StencilTex.Load(int3(coord, 0)).g;
    const uint up = StencilTex.Load(int3(coord + int2(0, -offset), 0)).g;
    const uint down = StencilTex.Load(int3(coord + int2(0, offset), 0)).g;
    const uint left = StencilTex.Load(int3(coord + int2(-offset, 0), 0)).g;
    const uint right = StencilTex.Load(int3(coord + int2(offset, 0), 0)).g;
    const uint upLeft = StencilTex.Load(int3(coord + int2(-offset, -offset), 0)).g;
    const uint upRight = StencilTex.Load(int3(coord + int2(offset, -offset), 0)).g;
    const uint downLeft = StencilTex.Load(int3(coord + int2(-offset, offset), 0)).g;
    const uint downRight = StencilTex.Load(int3(coord + int2(offset, offset), 0)).g;

    // Draw the outline on background pixels adjacent to selected geometry.
    if (center != 0)
        discard;

    const uint neighborMask = up | down | left | right | upLeft | upRight | downLeft | downRight;
    if (neighborMask == 0)
        discard;

    return OutlineColor;
}
