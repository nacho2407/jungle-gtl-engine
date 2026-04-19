#include "Render/Passes/Editor/OverlayTextPass.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Builders/Text/TextDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FOverlayTextPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FOverlayTextPass::PrepareTargets(FRenderPassContext& Context)
{
    // simple pass target setup moved into pass-local code.
}

void FOverlayTextPass::BuildDrawCommands(FRenderPassContext& Context)
{
    FTextDrawCommandBuilder::BuildOverlay(Context, *Context.DrawCommandList);
 }

void FOverlayTextPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
        (void)Context; (void)Proxy;
}

void FOverlayTextPass::SubmitDrawCommands(FRenderPassContext& Context)
{
        if (Context.DrawCommandList) { uint32 s,e; Context.DrawCommandList->GetPassRange(ERenderPass::OverlayFont,s,e); if(s<e) Context.DrawCommandList->SubmitRange(s,e,*Context.Device,Context.Context,*Context.StateCache); }
}
