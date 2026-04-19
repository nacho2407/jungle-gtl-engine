#include "Render/Passes/Scene/DepthPrePass.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Builders/Mesh/MeshDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FDepthPrePass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FDepthPrePass::PrepareTargets(FRenderPassContext& Context)
{
    // simple pass target setup moved into pass-local code.
}

void FDepthPrePass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
}

void FDepthPrePass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FMeshDrawCommandBuilder::Build(Proxy, ERenderPass::DepthPrePass, Context, *Context.DrawCommandList);
}

void FDepthPrePass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::DepthPrePass, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}
