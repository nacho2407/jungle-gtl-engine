#include "Render/Passes/Editor/SelectionMaskPass.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Builders/Mesh/MeshDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FSelectionMaskPass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FSelectionMaskPass::PrepareTargets(FRenderPassContext& Context)
{
    // simple pass target setup moved into pass-local code.
}

void FSelectionMaskPass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
 }

void FSelectionMaskPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
        FMeshDrawCommandBuilder::Build(Proxy, ERenderPass::SelectionMask, Context, *Context.DrawCommandList);
}

void FSelectionMaskPass::SubmitDrawCommands(FRenderPassContext& Context)
{
        if (Context.DrawCommandList) { uint32 s,e; Context.DrawCommandList->GetPassRange(ERenderPass::SelectionMask,s,e); if(s<e) Context.DrawCommandList->SubmitRange(s,e,*Context.Device,Context.Context,*Context.StateCache); }
}
