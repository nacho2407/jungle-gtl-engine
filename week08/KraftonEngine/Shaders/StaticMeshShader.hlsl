#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/SystemSamplers.hlsl"

Texture2D g_txColor : register(t0);

cbuffer PerShader1 : register(b2)
{
    float4 SectionColor;
};

PS_Input_Full VS(VS_Input_PNCT_T input)
{
    PS_Input_Full output;
    output.position = ApplyMVP(input.position);
    output.normal = normalize(mul(input.normal, (float3x3)NormalMatrix));
    output.color = input.color * SectionColor;
    output.texcoord = input.texcoord;

    return output;
}

float4 PS(PS_Input_Full input) : SV_TARGET
{
    float4 texColor = g_txColor.Sample(LinearWrapSampler, input.texcoord);

    if (texColor.a < 0.001f)
        texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

    float4 finalColor = texColor * input.color;
    finalColor.a = texColor.a * input.color.a;

    return float4(ApplyWireframe(finalColor.rgb), finalColor.a);
}
