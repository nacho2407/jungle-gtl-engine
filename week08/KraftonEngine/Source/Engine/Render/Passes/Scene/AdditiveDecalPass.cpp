#include "Render/Passes/Scene/AdditiveDecalPass.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders/MeshDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FAdditiveDecalPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FAdditiveDecalPass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FAdditiveDecalPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FMeshDrawCommandBuilder::Build(Proxy, ERenderPass::AdditiveDecal, Context, *Context.DrawCommandList);
}

void FAdditiveDecalPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::AdditiveDecal, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}