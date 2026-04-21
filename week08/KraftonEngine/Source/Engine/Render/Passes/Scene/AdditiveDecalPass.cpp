#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Passes/Scene/AdditiveDecalPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FAdditiveDecalPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FAdditiveDecalPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FAdditiveDecalPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    DrawCommandBuilder::BuildMeshDrawCommand(Proxy, ERenderPass::AdditiveDecal, Context, *Context.DrawCommandList);
}

void FAdditiveDecalPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::AdditiveDecal);
}
