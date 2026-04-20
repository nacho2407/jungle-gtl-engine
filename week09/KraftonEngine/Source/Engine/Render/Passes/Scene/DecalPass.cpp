#include "Render/Passes/Scene/DecalPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Pipelines/Registry/ViewModePassConfig.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/DecalDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Pipelines/Context/View/ViewModeSurfaceSet.h"
#include "Render/Pipelines/Context/View/ViewportRenderTargets.h"

namespace
{
bool UsesDecalPass(const FRenderPipelineContext& Context)
{
    return !Context.ViewModePassRegistry ||
           !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode) ||
           Context.ViewModePassRegistry->UsesDecal(Context.ActiveViewMode);
}
} // namespace

void FDecalPass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!UsesDecalPass(Context))
    {
        return;
    }

    if (Context.ActiveViewSurfaceSet)
    {
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

        ID3D11ShaderResourceView* BaseInputs[3] = {
            Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::BaseColor),
            Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1),
            Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface2),
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
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->NormalSRV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FDecalPass::PrepareTargets(FRenderPipelineContext& Context)
{
    if (!UsesDecalPass(Context))
    {
        return;
    }

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
        if (Src && Dst)
            Context.Context->CopyResource(Dst, Src);
        if (Dst)
            Dst->Release();
        if (Src)
            Src->Release();
    }

    if (ShadingModel == EShadingModel::Lambert || ShadingModel == EShadingModel::BlinnPhong)
    {
        ID3D11Resource* Src = nullptr;
        ID3D11Resource* Dst = nullptr;
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1)->GetResource(&Src);
        Context.ActiveViewSurfaceSet->GetRTV(ESurfaceSlot::ModifiedSurface1)->GetResource(&Dst);
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
        Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface2)->GetResource(&Src);
        Context.ActiveViewSurfaceSet->GetRTV(ESurfaceSlot::ModifiedSurface2)->GetResource(&Dst);
        if (Src && Dst)
            Context.Context->CopyResource(Dst, Src);
        if (Dst)
            Dst->Release();
        if (Src)
            Src->Release();
    }

    Context.ActiveViewSurfaceSet->BindDecalTargets(Context.Context, ShadingModel, Context.GetViewportDSV());
}

void FDecalPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FDecalDrawCommandBuilder::Build(Proxy, Context, *Context.DrawCommandList);
}

void FDecalPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.DrawCommandList || !UsesDecalPass(Context))
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

    if (!Targets || !Targets->ViewportRenderTexture)
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
