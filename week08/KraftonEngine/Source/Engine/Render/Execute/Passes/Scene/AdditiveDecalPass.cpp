#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Passes/Scene/AdditiveDecalPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
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
    DrawCommandBuild::BuildMeshDrawCommand(Proxy, ERenderPass::AdditiveDecal, Context, *Context.DrawCommandList);
}

void FAdditiveDecalPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::AdditiveDecal);
}
