#include "Render/Passes/Scene/AlphaBlendPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//MeshDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FAlphaBlendPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FAlphaBlendPass::PrepareTargets(FRenderPassContext& Context)
{
    // simple pass target setup moved into pass-local code.
}

void FAlphaBlendPass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
}

void FAlphaBlendPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FMeshDrawCommandBuilder::Build(Proxy, ERenderPass::AlphaBlend, Context, *Context.DrawCommandList);
}

void FAlphaBlendPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::AlphaBlend, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}