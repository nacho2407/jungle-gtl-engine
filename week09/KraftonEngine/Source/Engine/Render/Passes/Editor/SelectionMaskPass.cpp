#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Passes/Editor/SelectionMaskPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/MeshDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FSelectionMaskPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FSelectionMaskPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FSelectionMaskPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FMeshDrawCommandBuilder::Build(Proxy, ERenderPass::SelectionMask, Context, *Context.DrawCommandList);
}

void FSelectionMaskPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::SelectionMask);
}
