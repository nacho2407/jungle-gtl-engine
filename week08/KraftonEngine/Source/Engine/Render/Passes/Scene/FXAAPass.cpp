#include "Render/Passes/Scene/FXAAPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//FullscreenDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FFXAAPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FFXAAPass::PrepareTargets(FRenderPassContext& Context)
{
    // FXAA target setup moved into pass-local code.
}

void FFXAAPass::BuildDrawCommands(FRenderPassContext& Context)
{
    if (!Context.Frame || !Context.Frame->ShowFlags.bFXAA)
    {
        return;
    }

    if (!Context.Frame->SceneColorCopySRV || !Context.Frame->SceneColorCopyTexture)
    {
        return;
    }

    FFullscreenDrawCommandBuilder::Build(ERenderPass::FXAA, Context, *Context.DrawCommandList);
}

void FFXAAPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FFXAAPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::FXAA, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}
