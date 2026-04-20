#include "Render/Passes/Editor/DebugLinePass.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders/LineDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FDebugLinePass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FDebugLinePass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FDebugLinePass::BuildDrawCommands(FRenderPassContext& Context)
{
    FLineDrawCommandBuilder::Build(Context, *Context.DrawCommandList);
}

void FDebugLinePass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::EditorLines, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}