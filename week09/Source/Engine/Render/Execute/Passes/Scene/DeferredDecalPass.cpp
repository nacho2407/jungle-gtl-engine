#include "Render/Execute/Passes/Scene/DeferredDecalPass.h"

#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"

namespace
{
bool UsesDeferredDecalPass(const FRenderPipelineContext& Context)
{
    return !Context.ViewMode.Registry ||
           !Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode) ||
           Context.ViewMode.Registry->UsesDecal(Context.ViewMode.ActiveViewMode);
}
} // namespace

void FDeferredDecalPass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!UsesDeferredDecalPass(Context))
    {
        return;
    }

    if (Context.ViewMode.Surfaces)
    {
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

        ID3D11ShaderResourceView* BaseInputs[3] = {
            Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::BaseColor),
            Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::Surface1),
            Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::Surface2),
        };
        Context.Context->PSSetShaderResources(1, ARRAY_SIZE(BaseInputs), BaseInputs);
    }

    if (Targets && Targets->DepthTexture && Targets->DepthCopyTexture && Targets->DepthTexture != Targets->DepthCopyTexture)
    {
        Context.Context->CopyResource(Targets->DepthCopyTexture, Targets->DepthTexture);
    }

    if (Targets && Targets->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV  = nullptr;
        Context.StateCache->NormalSRV   = nullptr;
        Context.StateCache->SpecularSRV = nullptr;
        Context.StateCache->bForceAll   = true;
    }
}

void FDeferredDecalPass::PrepareTargets(FRenderPipelineContext& Context)
{
    if (!UsesDeferredDecalPass(Context))
    {
        return;
    }

    if (!Context.ViewMode.Surfaces || !Context.ViewMode.Registry || !Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode))
    {
        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
        return;
    }

    const EShadingModel ShadingModel = Context.ViewMode.Registry->GetShadingModel(Context.ViewMode.ActiveViewMode);

    if (ShadingModel == EShadingModel::Unlit)
    {
        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
        return;
    }

    Context.ViewMode.Surfaces->ClearModifiedTargets(Context.Context, ShadingModel);

    if (Context.ViewMode.Surfaces->GetRTV(EViewModeSurfaceslot::ModifiedBaseColor) && Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::BaseColor))
    {
        ID3D11Resource* Src = nullptr;
        ID3D11Resource* Dst = nullptr;
        Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::BaseColor)->GetResource(&Src);
        Context.ViewMode.Surfaces->GetRTV(EViewModeSurfaceslot::ModifiedBaseColor)->GetResource(&Dst);
        if (Src && Dst)
            Context.Context->CopyResource(Dst, Src);
        if (Dst)
            Dst->Release();
        if (Src)
            Src->Release();
    }

    if (ShadingModel == EShadingModel::Lambert || ShadingModel == EShadingModel::BlinnPhong || ShadingModel == EShadingModel::WorldNormal)
    {
        ID3D11Resource* Src = nullptr;
        ID3D11Resource* Dst = nullptr;
        Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::Surface1)->GetResource(&Src);
        Context.ViewMode.Surfaces->GetRTV(EViewModeSurfaceslot::ModifiedSurface1)->GetResource(&Dst);
        if (Src && Dst)
            Context.Context->CopyResource(Dst, Src);
        if (Dst)
            Dst->Release();
        if (Src)
            Src->Release();
    }

    if (ShadingModel == EShadingModel::BlinnPhong)
    {
        ID3D11Resource* Src = nullptr;
        ID3D11Resource* Dst = nullptr;
        Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::Surface2)->GetResource(&Src);
        Context.ViewMode.Surfaces->GetRTV(EViewModeSurfaceslot::ModifiedSurface2)->GetResource(&Dst);
        if (Src && Dst)
            Context.Context->CopyResource(Dst, Src);
        if (Dst)
            Dst->Release();
        if (Src)
            Src->Release();
    }

    Context.ViewMode.Surfaces->BindDecalTargets(Context.Context, ShadingModel, Context.GetViewportDSV());
}

void FDeferredDecalPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
{
    DrawCommandBuild::BuildDecalDrawCommand(Proxy, Context, *Context.DrawCommandList);
}

void FDeferredDecalPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.DrawCommandList || !UsesDeferredDecalPass(Context))
    {
        return;
    }

    const bool          bHasViewModeConfig = Context.ViewMode.Surfaces && Context.ViewMode.Registry && Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode);
    const EShadingModel ShadingModel       = bHasViewModeConfig ? Context.ViewMode.Registry->GetShadingModel(Context.ViewMode.ActiveViewMode) : EShadingModel::Unlit;

    uint32 Start = 0;
    uint32 End   = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::Decal, Start, End);
    if (Start < End)
    {
        Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
    }

    if (!Targets || !Targets->ViewportRenderTexture)
    {
        return;
    }

    if (bHasViewModeConfig && ShadingModel == EShadingModel::Unlit)
    {
        if (Start == End && Context.ViewMode.Surfaces)
        {
            Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11Resource* Src = nullptr;
            Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::BaseColor)->GetResource(&Src);
            if (Src)
            {
                Context.Context->CopyResource(Targets->ViewportRenderTexture, Src);
                Src->Release();
            }
        }

        if (Targets->SceneColorCopyTexture && Targets->SceneColorCopyTexture != Targets->ViewportRenderTexture)
        {
            Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
            Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);
        }

        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
    }
}
