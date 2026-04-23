#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Passes/Editor/OverlayTextPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FOverlayTextPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FOverlayTextPass::PrepareTargets(FRenderPipelineContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FOverlayTextPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    DrawCommandBuild::BuildOverlayTextDrawCommand(Context, *Context.DrawCommandList);
}

void FOverlayTextPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    uint32 Start = 0;
    uint32 End   = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::OverlayTextWorld, Start, End);
    if (Start < End)
    {
        Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
    }

    Context.DrawCommandList->GetPassRange(ERenderPass::OverlayFont, Start, End);
    if (Start < End)
    {
        Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
    }
}
