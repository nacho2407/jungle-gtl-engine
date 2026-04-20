#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Passes/Editor/OverlayTextPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/TextDrawCommandBuilder.h"
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
    FTextDrawCommandBuilder::BuildOverlay(Context, *Context.DrawCommandList);
}

void FOverlayTextPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::OverlayFont, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}
