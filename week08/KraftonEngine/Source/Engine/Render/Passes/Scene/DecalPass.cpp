#include "Render/Passes/Scene/DecalPass.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Builders/Decal/DecalDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FDecalPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FDecalPass::PrepareTargets(FRenderPassContext& Context)
{
    // decal pass input/target setup moved into pass-local code.
}

void FDecalPass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
 }

void FDecalPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
        FDecalDrawCommandBuilder::Build(Proxy, Context, *Context.DrawCommandList);
}

void FDecalPass::SubmitDrawCommands(FRenderPassContext& Context)
{
        if (Context.DrawCommandList) { uint32 s,e; Context.DrawCommandList->GetPassRange(ERenderPass::Decal,s,e); if(s<e) Context.DrawCommandList->SubmitRange(s,e,*Context.Device,Context.Context,*Context.StateCache); }
}
