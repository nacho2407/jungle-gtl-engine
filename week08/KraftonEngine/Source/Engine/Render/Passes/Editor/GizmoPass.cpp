#include "Render/Passes/Editor/GizmoPass.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders/MeshDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FGizmoPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FGizmoPass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FGizmoPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FMeshDrawCommandBuilder::Build(Proxy, Proxy.Pass, Context, *Context.DrawCommandList);
}

void FGizmoPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    uint32 Start = 0, End = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::GizmoOuter, Start, End);
    if (Start < End)
    {
        Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
    }

    Context.DrawCommandList->GetPassRange(ERenderPass::GizmoInner, Start, End);
    if (Start < End)
    {
        Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
    }
}
