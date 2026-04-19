#include "Render/Passes/Scene/DecalPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Core/PassTypes.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//DecalDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"
#include "Render/D3D11/Frame/ViewModeSurfaceSet.h"

void FDecalPass::PrepareInputs(FRenderPassContext& Context)
{
    if (Context.ActiveViewSurfaceSet)
    {
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

        ID3D11ShaderResourceView* BaseInputs[3] = {
            Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::BaseColor),
            Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1),
            Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface2),
        };
        Context.Context->PSSetShaderResources(1, ARRAYSIZE(BaseInputs), BaseInputs);
    }

    if (Context.Frame && Context.Frame->DepthTexture && Context.Frame->DepthCopyTexture && Context.Frame->DepthTexture != Context.Frame->DepthCopyTexture)
    {
        Context.Context->CopyResource(Context.Frame->DepthCopyTexture, Context.Frame->DepthTexture);
    }

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

void FDecalPass::PrepareTargets(FRenderPassContext& Context)
{
    if (!Context.ActiveViewSurfaceSet || !Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
        return;
    }

    const EShadingModel ShadingModel = Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode);

    if (ShadingModel == EShadingModel::Unlit)
    {
        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
        return;
    }

    Context.ActiveViewSurfaceSet->ClearModifiedTargets(Context.Context, ShadingModel);

    if (Context.ActiveViewSurfaceSet->GetRTV(ESurfaceSlot::ModifiedBaseColor) && Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::BaseColor))
    {
        ID3D11Resource* Src = nullptr;
        ID3D11Resource* Dst = nullptr;
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::BaseColor)->GetResource(&Src);
        Context.ActiveViewSurfaceSet->GetRTV(ESurfaceSlot::ModifiedBaseColor)->GetResource(&Dst);
        if (Src && Dst) Context.Context->CopyResource(Dst, Src);
        if (Dst) Dst->Release();
        if (Src) Src->Release();
    }

    if (ShadingModel == EShadingModel::Lambert || ShadingModel == EShadingModel::BlinnPhong)
    {
        ID3D11Resource* Src = nullptr;
        ID3D11Resource* Dst = nullptr;
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1)->GetResource(&Src);
        Context.ActiveViewSurfaceSet->GetRTV(ESurfaceSlot::ModifiedSurface1)->GetResource(&Dst);
        if (Src && Dst) Context.Context->CopyResource(Dst, Src);
        if (Dst) Dst->Release();
        if (Src) Src->Release();
    }

    if (ShadingModel == EShadingModel::BlinnPhong)
    {
        ID3D11Resource* Src = nullptr;
        ID3D11Resource* Dst = nullptr;
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface2)->GetResource(&Src);
        Context.ActiveViewSurfaceSet->GetRTV(ESurfaceSlot::ModifiedSurface2)->GetResource(&Dst);
        if (Src && Dst) Context.Context->CopyResource(Dst, Src);
        if (Dst) Dst->Release();
        if (Src) Src->Release();
    }

    Context.ActiveViewSurfaceSet->BindDecalTargets(Context.Context, ShadingModel, Context.GetViewportDSV());
}

void FDecalPass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
}

void FDecalPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FDecalDrawCommandBuilder::Build(Proxy, Context, *Context.DrawCommandList);
}

void FDecalPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    const bool bHasViewModeConfig = Context.ActiveViewSurfaceSet && Context.ViewModePassRegistry && Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode);
    const EShadingModel ShadingModel = bHasViewModeConfig ? Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode) : EShadingModel::Unlit;

    uint32 s, e;
    Context.DrawCommandList->GetPassRange(ERenderPass::Decal, s, e);
    if (s < e)
    {
        Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }

    if (!Context.Frame || !Context.Frame->ViewportRenderTexture)
    {
        return;
    }

    if (bHasViewModeConfig && ShadingModel == EShadingModel::Unlit)
    {
        if (s == e && Context.ActiveViewSurfaceSet)
        {
            Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11Resource* Src = nullptr;
            Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::BaseColor)->GetResource(&Src);
            if (Src)
            {
                Context.Context->CopyResource(Context.Frame->ViewportRenderTexture, Src);
                Src->Release();
            }
        }

        if (Context.Frame->SceneColorCopyTexture && Context.Frame->SceneColorCopyTexture != Context.Frame->ViewportRenderTexture)
        {
            Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
            Context.Context->CopyResource(Context.Frame->SceneColorCopyTexture, Context.Frame->ViewportRenderTexture);
        }

        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
    }
}
