#include "Render/Passes/Editor/GizmoPass.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Builders/Mesh/MeshDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FGizmoPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FGizmoPass::PrepareTargets(FRenderPassContext& Context)
{
    // simple pass target setup moved into pass-local code.
}

void FGizmoPass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
 }

void FGizmoPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
        FMeshDrawCommandBuilder::Build(Proxy, Proxy.Pass, Context, *Context.DrawCommandList);
}

void FGizmoPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (!Context.DrawCommandList) return;
    uint32 s = 0, e = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::GizmoOuter, s, e);
    if (s < e) Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    Context.DrawCommandList->GetPassRange(ERenderPass::GizmoInner, s, e);
    if (s < e) Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
}
