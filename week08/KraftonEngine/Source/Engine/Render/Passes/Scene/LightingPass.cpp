#include "Render/Passes/Scene/LightingPass.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Builders/Fullscreen/FullscreenDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FLightingPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FLightingPass::PrepareTargets(FRenderPassContext& Context)
{
    // lighting pass target setup moved into pass-local code.
}

void FLightingPass::BuildDrawCommands(FRenderPassContext& Context)
{
    FFullscreenDrawCommandBuilder::Build(ERenderPass::Lighting, Context, *Context.DrawCommandList);
 }

void FLightingPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
        (void)Context; (void)Proxy;
}

void FLightingPass::SubmitDrawCommands(FRenderPassContext& Context)
{
        if (Context.DrawCommandList) { uint32 s,e; Context.DrawCommandList->GetPassRange(ERenderPass::Lighting,s,e); if(s<e) Context.DrawCommandList->SubmitRange(s,e,*Context.Device,Context.Context,*Context.StateCache); }
}
