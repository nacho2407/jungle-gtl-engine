#include "Render/Passes/Scene/LightingPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//FullscreenDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"
#include "Render/D3D11/Frame/ViewModeSurfaceSet.h"
#include "Render/Core/PassTypes.h"
#include "Render/Core/FrameContext.h"
#include "Render/Core/RenderConstants.h"

void FLightingPass::PrepareInputs(FRenderPassContext& Context)
{
    if (!Context.ActiveViewSurfaceSet || !Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        return;
    }

    if (Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode) == EShadingModel::Unlit)
    {
        return;
    }

    const bool bNeedsReadableDepth = Context.Frame && Context.Frame->DepthTexture && Context.Frame->DepthCopyTexture &&
        Context.Frame->DepthTexture != Context.Frame->DepthCopyTexture;

    Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

    if (bNeedsReadableDepth)
    {
        Context.Context->CopyResource(Context.Frame->DepthCopyTexture, Context.Frame->DepthTexture);
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

    if (Context.Frame && Context.Frame->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Context.Frame->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV = nullptr;
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

void FLightingPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FLightingPass::SubmitDrawCommands(FRenderPassContext& Context)
{
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

    if (Context.Frame && Context.Frame->ViewportRenderTexture && Context.Frame->SceneColorCopyTexture &&
        Context.Frame->ViewportRenderTexture != Context.Frame->SceneColorCopyTexture)
    {
        ID3D11ShaderResourceView* NullSRV = nullptr;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Context.Frame->SceneColorCopyTexture, Context.Frame->ViewportRenderTexture);

        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
    }
}
