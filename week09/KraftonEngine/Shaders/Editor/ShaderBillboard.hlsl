// Shader include: Editor/ShaderBillboard.hlsl
// Role: shared shader code or editor/material entry.
// Slots: declared locally or in included common resources.

#include "../Common/Utils/Functions.hlsl"
#include "../Common/Geometry/VertexLayouts.hlsl"
#include "../Common/Resources/SystemSamplers.hlsl"

Texture2D BillboardTex : register(t0);

PS_Input_Tex VS(VS_Input_PNCT input)
{
    PS_Input_Tex output;
    output.position = ApplyMVP(input.position);
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float4 col = BillboardTex.Sample(LinearClampSampler, input.texcoord);

    if (!bIsWireframe && col.a < 0.05f)
        discard;

    return float4(ApplyWireframe(col.rgb), bIsWireframe ? 1.0f : col.a);
}

