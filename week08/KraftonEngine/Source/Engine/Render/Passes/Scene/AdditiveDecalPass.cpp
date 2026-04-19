#include "Render/Passes/Scene/AdditiveDecalPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//MeshDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FAdditiveDecalPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FAdditiveDecalPass::PrepareTargets(FRenderPassContext& Context)
{
    // simple pass target setup moved into pass-local code.
}

void FAdditiveDecalPass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
}

void FAdditiveDecalPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FMeshDrawCommandBuilder::Build(Proxy, ERenderPass::AdditiveDecal, Context, *Context.DrawCommandList);
}

void FAdditiveDecalPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::AdditiveDecal, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}