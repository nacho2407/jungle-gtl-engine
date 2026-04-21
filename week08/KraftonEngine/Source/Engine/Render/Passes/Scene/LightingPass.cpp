#include "Render/Passes/Scene/LightingPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/FullscreenDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Pipelines/Context/View/ViewModeSurfaceSet.h"
#include "Render/Pipelines/Registry/ViewModePassConfig.h"
#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Pipelines/Context/View/ViewportRenderTargets.h"
#include "Render/Pipelines/Context/FrameSharedResources.h"
#include "Render/Visibility/TileBasedLightCulling.h"

void FLightingPass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.ActiveViewSurfaceSet || !Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        return;
    }

    if (Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode) == EShadingModel::Unlit)
    {
        return;
    }

    const bool bNeedsReadableDepth = Targets && Targets->DepthTexture && Targets->DepthCopyTexture &&
                                     Targets->DepthTexture != Targets->DepthCopyTexture;

    Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

    if (bNeedsReadableDepth)
    {
        Context.Context->CopyResource(Targets->DepthCopyTexture, Targets->DepthTexture);
    }

    ID3D11ShaderResourceView* SurfaceSRVs[6] = {
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::BaseColor),
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1),
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface2),
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::ModifiedBaseColor),
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::ModifiedSurface1),
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::ModifiedSurface2),
    };
    Context.Context->PSSetShaderResources(0, ARRAY_SIZE(SurfaceSRVs), SurfaceSRVs);

	// LightCulling 관련 데이터 바이딩
    if (Context.LightCulling)
    {
        // TileMask SRV 추가
        ID3D11ShaderResourceView* TileMaskSRV = Context.LightCulling->GetPerTileMaskSRV();
        Context.Context->PSSetShaderResources(7, 1, &TileMaskSRV);

		//b2 LightCullingParams
		ID3D11Buffer* LightCullingParamsCB = Context.LightCulling->GetLightCullingParamsCB();
        Context.Context->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &LightCullingParamsCB);
    }

    if (Targets && Targets->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }

    if (Context.Resources)
    {
        ID3D11Buffer* GlobalLightBuffer = Context.Resources->GlobalLightBuffer.GetBuffer();
        Context.Context->PSSetConstantBuffers(ECBSlot::Light, 1, &GlobalLightBuffer);

        ID3D11ShaderResourceView* LocalLightsSRV = Context.Resources->LocalLightSRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &LocalLightsSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->LightCB = Context.Resources ? &Context.Resources->GlobalLightBuffer : nullptr;
        Context.StateCache->LocalLightSRV = Context.Resources ? Context.Resources->LocalLightSRV : nullptr;
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->NormalSRV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FLightingPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FLightingPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.ActiveViewSurfaceSet || !Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        return;
    }

    if (Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode) == EShadingModel::Unlit)
    {
        return;
    }

    FFullscreenDrawCommandBuilder::Build(ERenderPass::Lighting, Context, *Context.DrawCommandList);
}

void FLightingPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.DrawCommandList)
    {
        return;
    }

    SubmitPassRange(Context, ERenderPass::Lighting);

    if (Targets && Targets->ViewportRenderTexture && Targets->SceneColorCopyTexture &&
        Targets->ViewportRenderTexture != Targets->SceneColorCopyTexture)
    {
        ID3D11ShaderResourceView* NullSRV = nullptr;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);

        BindViewportTarget(Context);
    }
}
