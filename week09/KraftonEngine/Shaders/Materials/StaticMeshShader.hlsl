#include "../Common/Utils/Functions.hlsl"
#include "../Common/Geometry/VertexLayouts.hlsl"
#include "../Common/Resources/SystemSamplers.hlsl"
#include "../Common/Types/StaticMeshMaterialCommon.hlsli"

Texture2D g_txColor : register(t0);

PS_Input_Full VS(VS_Input_PNCT_T input)
{
    PS_Input_Full output;
    output.position = ApplyMVP(input.position);
    output.normal = normalize(mul(input.normal, (float3x3)NormalMatrix));
    output.color = GetStaticMeshSectionColorOrWhite();
    output.texcoord = input.texcoord;

    return output;
}

float4 PS(PS_Input_Full input) : SV_TARGET
{
    float4 texColor = SampleStaticMeshBaseColor(g_txColor, input.texcoord);

    float4 finalColor = texColor * input.color;
    finalColor.a = texColor.a * input.color.a;

    return float4(ApplyWireframe(finalColor.rgb), finalColor.a);
}
