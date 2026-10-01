// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Passes/Editor/OverlayBillboardPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"

void FOverlayBillboardPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FOverlayBillboardPass::PrepareTargets(FRenderPipelineContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FOverlayBillboardPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    DrawCommandBuild::BuildOverlayBillboardDrawCommand(Context, *Context.DrawCommandList);
}

void FOverlayBillboardPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    uint32 Start = 0;
    uint32 End   = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::OverlayBillboard, Start, End);
    if (Start < End)
    {
        Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
    }
}
