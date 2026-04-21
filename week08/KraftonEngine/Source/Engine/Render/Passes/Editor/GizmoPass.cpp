#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Passes/Editor/GizmoPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FGizmoPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FGizmoPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FGizmoPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    DrawCommandBuilder::BuildMeshDrawCommand(Proxy, Proxy.Pass, Context, *Context.DrawCommandList);
}

void FGizmoPass::SubmitDrawCommands(FRenderPipelineContext& Context)
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
