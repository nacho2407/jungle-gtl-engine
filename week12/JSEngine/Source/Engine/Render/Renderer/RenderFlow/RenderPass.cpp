#include "RenderPass.h"
#include "Core/ResourceManager.h"
#include "Component/PostProcess/Light/LightComponent.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Scene/RenderBus.h"

bool FBaseRenderPass::Render(const FRenderPassContext* Context)
{
    bool bResult;
    
	bResult = Begin(Context);
    if (!bResult)
        return false;

    bResult = DrawCommand(Context);
    if (!bResult)
        return false;

    bResult = End(Context);
    if (!bResult)
        return false;

	return true;
}

void FBaseRenderPass::CheckOverrideViewMode(const FRenderPassContext* Context)
{
    if (bSkipWireframe && Context->RenderBus->GetViewMode() == EViewMode::Wireframe)
        return;
    /*
        RasterizerState 를 Material 에서 들고 있는 상태라, 전역 설정인 ViewMode 간의 override 가 필요한데
        현재는 우선 Bind 이후 Override 하는 방식으로 임시 처리
        향후 수정 필요
    */
    if (Context->RenderBus->GetViewMode() == EViewMode::Wireframe)
    {
        ID3D11RasterizerState* WireRS = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::WireFrame);
        Context->DeviceContext->RSSetState(WireRS);
    }
}

bool FBaseRenderPass::IsDebugViewMode(EViewMode ViewMode) const
{
    return ViewMode == EViewMode::Normal ||
        ViewMode == EViewMode::Heatmap ||
        ViewMode == EViewMode::BoneWeightHeatmap ||
        ViewMode == EViewMode::Depth;
}

uint32 FBaseRenderPass::GetLightingModelPermutationKey(EViewMode ViewMode) const
{
    switch (ViewMode)
    {
    case EViewMode::Lit_Gouraud:
        return (uint32)ELightingModel::Gouraud;
    case EViewMode::Lit_Lambert:
        return (uint32)ELightingModel::Lambert;
    case EViewMode::Lit_BlinnPhong:
        return (uint32)ELightingModel::BlinnPhong;
    case EViewMode::Heatmap:
        return (uint32)ELightingModel::Heatmap;
    case EViewMode::BoneWeightHeatmap:
        return (uint32)ELightingModel::BoneWeightHeatmap;
    default:
        return (uint32)ELightingModel::Unlit;
    }
}

uint32 FBaseRenderPass::GetLightCullingPermutationKey(const FRenderPassContext* Context) const
{
    if (Context->RenderBus->GetLightCullMode() == ELightCullMode::Clustered)
    {
        return (uint32)EShaderFeature::ClusterCull;
    }
    else if (Context->RenderBus->GetLightCullMode() == ELightCullMode::Tiled)
    {
        return (uint32)EShaderFeature::TileCull;
    }

    return 0;
}

uint32 FBaseRenderPass::GetShadowMapPermutationKey(const FRenderPassContext* Context, bool bIncludeCascadeVis) const
{
    uint32 PermutationKey = 0;
    bool bShadowApplied = false;

    // ShadowMap Permutation Key 조합
    for (const FShadowLightRequest& Request : Context->RenderBus->ShadowLightRequests)
    {
        if (!Request.bCastShadows || !Request.LightComponent) continue;
        if (Request.Type != EShadowLightType::SLT_Directional) continue;

        ULightComponent* LightComp = Cast<ULightComponent>(Request.LightComponent);
        if (!LightComp) continue;
        switch (LightComp->GetShadowMapType())
        {
        case EShadowMap::CSM:
        {
            PermutationKey |= (uint32)EShaderFeature::ShadowCSM;
            if (bIncludeCascadeVis && Context->RenderBus->GetCascadeVis())
                PermutationKey |= (uint32)EShaderFeature::CascadeVis;
            bShadowApplied = true;
            break;
        }

        case EShadowMap::PSM:
        {
            PermutationKey |= (uint32)EShaderFeature::ShadowPSM;
            bShadowApplied = true;

            break;
        }
        default: break;
        }
        break;
    }
    // VSM 모드 + 그림자 활성일 때만 OR
    if (bShadowApplied && Context->RenderBus->GetShadowFilterMode() == EShadowFilter::VSM)
    {
        PermutationKey |= (uint32)EShaderFeature::ShadowVSM;
    }

    return PermutationKey;
}

uint32 FBaseRenderPass::GetTexturePermutationKey(
    const UMaterialInterface* Material,
    bool bIncludeNormalMap,
    bool bIncludeSpecularMap,
    bool bIncludeEmissiveMap,
    bool bIncludeAlphaMask) const
{
    uint32 PermutationKey = 0;
    if (Material)
    {
        if (Material->HasDiffuseMap()) PermutationKey |= (uint32)EShaderFeature::HasDiffuseMap;
        if (bIncludeNormalMap && Material->HasNormalMap()) PermutationKey |= (uint32)EShaderFeature::HasNormalMap;
        if (bIncludeSpecularMap && Material->HasSpecularMap()) PermutationKey |= (uint32)EShaderFeature::HasSpecularMap;
        if (bIncludeEmissiveMap && Material->HasEmissiveMap()) PermutationKey |= (uint32)EShaderFeature::HasEmissiveMap;
        if (bIncludeAlphaMask && Material->HasAlphaMask()) PermutationKey |= (uint32)EShaderFeature::HasAlphaMask;
    }

    return PermutationKey;
}

