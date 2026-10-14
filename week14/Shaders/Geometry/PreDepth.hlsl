// =============================================================================
// PreDepth.hlsl - Camera depth pre-pass VS / PS
// =============================================================================

#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/Skinning.hlsli"

PS_Input_PosOnly VS_StaticMesh(VS_Input_PNCTT input)
{
    PS_Input_PosOnly output;
    float4 worldPos = mul(float4(input.position, 1.0f), Model);
    output.position = mul(mul(worldPos, View), Projection);
    return output;
}

PS_Input_PosOnly VS_InstancedStaticMesh(VS_Input_PNCTT_Instanced input)
{
    PS_Input_PosOnly output;
    float4x4 world = float4x4(
        input.world0,
        input.world1,
        input.world2,
        input.world3);

    float4 worldPos = mul(float4(input.position, 1.0f), world);
    output.position = mul(mul(worldPos, View), Projection);
    return output;
}

PS_Input_PosOnly VS_SkeletalMesh(VS_Input_PNCTTBB input)
{
    PS_Input_PosOnly output;

    FSkinningResult skinned = ApplyLinearBlendSkinning(
        input.position,
        input.normal,
        input.tangent.xyz,
        input.boneIndices,
        input.boneWeights);

    float4 worldPos = mul(skinned.position, Model);
    output.position = mul(mul(worldPos, View), Projection);
    return output;
}

float4 PS(PS_Input_PosOnly input) : SV_TARGET
{
    return 0.0f;
}
