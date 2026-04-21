#include "Render/Passes/Editor/GridPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"

void FGridPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FGridPass::PrepareTargets(FRenderPipelineContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FGridPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    DrawCommandBuilder::BuildLineDrawCommand(Context, *Context.DrawCommandList);
}

void FGridPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::Grid, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}
