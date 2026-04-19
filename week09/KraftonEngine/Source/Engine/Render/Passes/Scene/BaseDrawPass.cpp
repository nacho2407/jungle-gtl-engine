#include "Render/Passes/Scene/BaseDrawPass.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Builders/Mesh/MeshDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FBaseDrawPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FBaseDrawPass::PrepareTargets(FRenderPassContext& Context)
{
    // base pass input/target setup moved into pass-local code.
}

void FBaseDrawPass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
 }

void FBaseDrawPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
        FMeshDrawCommandBuilder::Build(Proxy, ERenderPass::Opaque, Context, *Context.DrawCommandList);
}

void FBaseDrawPass::SubmitDrawCommands(FRenderPassContext& Context)
{
        if (Context.DrawCommandList) { uint32 s,e; Context.DrawCommandList->GetPassRange(ERenderPass::Opaque,s,e); if(s<e) Context.DrawCommandList->SubmitRange(s,e,*Context.Device,Context.Context,*Context.StateCache); }
}
