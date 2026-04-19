#include "Render/Passes/Scene/HeightFogPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//FullscreenDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FHeightFogPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FHeightFogPass::PrepareTargets(FRenderPassContext& Context)
{
    // post process target setup moved into pass-local code.
}

void FHeightFogPass::BuildDrawCommands(FRenderPassContext& Context)
{
    if (!Context.Frame || !Context.Frame->ShowFlags.bFog)
    {
        return;
    }

    if (!Context.Scene || !Context.Scene->HasFog())
    {
        return;
    }

    FFullscreenDrawCommandBuilder::Build(ERenderPass::PostProcess, Context, *Context.DrawCommandList, 0);
}

void FHeightFogPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FHeightFogPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, s, e);
        for (uint32 i = s; i < e; ++i)
        {
            const auto& c = Context.DrawCommandList->GetCommands()[i];
            if ((c.SortKey & 0xFFFu) == 0)
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
