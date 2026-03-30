#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

#ifdef USE_NORMAL_EXPANSION
// StaticMesh: PNCT InputLayout, 노멀 방향으로 확장
PS_Input_PosOnly VS(VS_Input_PNCT input)
{
    PS_Input_PosOnly output;

    float3 offset = normalize(input.normal) * (OutlineOffset * OutlineInvScale);
    float3 expandedPos = input.position + offset;

    output.position = ApplyMVP(expandedPos);
    return output;
}
#else
// Primitive: Position-only InputLayout, sign 방향으로 확장
PS_Input_PosOnly VS(VS_Input_P input)
{
    PS_Input_PosOnly output;

    float3 signDir = sign(input.position);
    float3 offset = signDir * (OutlineOffset * OutlineInvScale);
    float3 scaledPos = input.position + offset;

    output.position = ApplyMVP(scaledPos);
    return output;
}
#endif

float4 PS(PS_Input_PosOnly input) : SV_TARGET
{
    return OutlineColor;
}
