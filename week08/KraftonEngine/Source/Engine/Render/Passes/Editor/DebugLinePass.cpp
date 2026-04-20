#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Passes/Editor/DebugLinePass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/LineDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FDebugLinePass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FDebugLinePass::PrepareTargets(FRenderPipelineContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FDebugLinePass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    FLineDrawCommandBuilder::Build(Context, *Context.DrawCommandList);
}

void FDebugLinePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::EditorLines, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}
