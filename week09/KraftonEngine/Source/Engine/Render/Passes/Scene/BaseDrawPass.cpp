#include "Render/Passes/Scene/BaseDrawPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//MeshDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"
#include "Render/D3D11/Frame/ViewModeSurfaceSet.h"
#include "Render/Core/PassTypes.h"

void FBaseDrawPass::PrepareInputs(FRenderPassContext& Context)
{
    ID3D11ShaderResourceView* NullSRVs[6] = {};
    Context.Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);
    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FBaseDrawPass::PrepareTargets(FRenderPassContext& Context)
{
    if (Context.ActiveViewSurfaceSet && Context.ViewModePassRegistry && Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        const EShadingModel ShadingModel = Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode);
        Context.ActiveViewSurfaceSet->ClearBaseTargets(Context.Context, ShadingModel);
        Context.ActiveViewSurfaceSet->BindBaseDrawTargets(Context.Context, ShadingModel, Context.GetViewportDSV());
    }
    else
    {
        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
    }
}

void FBaseDrawPass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
}

void FBaseDrawPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FMeshDrawCommandBuilder::Build(Proxy, ERenderPass::Opaque, Context, *Context.DrawCommandList);
}

void FBaseDrawPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::Opaque, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}