#include "Render/Passes/Scene/LightingPass.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders/FullscreenDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Frame/ViewModeSurfaceSet.h"
#include "Render/Core/PassTypes.h"
#include "Render/Frame/FrameContext.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Frame/ViewportRenderTargets.h"

void FLightingPass::PrepareInputs(FRenderPassContext& Context)
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
    Context.Context->PSSetShaderResources(0, ARRAYSIZE(SurfaceSRVs), SurfaceSRVs);

    if (Targets && Targets->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->NormalSRV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FLightingPass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FLightingPass::BuildDrawCommands(FRenderPassContext& Context)
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

void FLightingPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.DrawCommandList)
    {
        return;
    }

    uint32 s, e;
    Context.DrawCommandList->GetPassRange(ERenderPass::Lighting, s, e);
    if (s < e)
    {
        Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }

    if (Targets && Targets->ViewportRenderTexture && Targets->SceneColorCopyTexture &&
        Targets->ViewportRenderTexture != Targets->SceneColorCopyTexture)
    {
        ID3D11ShaderResourceView* NullSRV = nullptr;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);

        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
    }
}
