// Shader: OutlinePostProcessPass
// Role: fullscreen editor selection outline.
// Entries: VS, PS. Slots: b2 OutlinePostProcessCB, t11 SceneColor, t13 StencilTex, s0 LinearClamp.

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
    const int radius = min(max((int)(OutlineThickness + 0.5f), 1), 6);

    const uint center = StencilTex.Load(int3(coord, 0)).g;

    // Draw the outline on background pixels adjacent to selected geometry.
    if (center != 0)
        discard;

    uint neighborMask = 0;
    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            if (x == 0 && y == 0)
                continue;

            neighborMask |= StencilTex.Load(int3(coord + int2(x, y), 0)).g;
        }
    }

    if (neighborMask == 0)
        discard;

    return OutlineColor;
}

