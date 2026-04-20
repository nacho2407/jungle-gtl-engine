#include "Render/Passes/Editor/OverlayTextPass.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders/TextDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FOverlayTextPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FOverlayTextPass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FOverlayTextPass::BuildDrawCommands(FRenderPassContext& Context)
{
    FTextDrawCommandBuilder::BuildOverlay(Context, *Context.DrawCommandList);
}

void FOverlayTextPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::OverlayFont, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}