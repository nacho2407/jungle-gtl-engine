#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Passes/Scene/AlphaBlendPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FAlphaBlendPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FAlphaBlendPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FAlphaBlendPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    DrawCommandBuilder::BuildMeshDrawCommand(Proxy, ERenderPass::AlphaBlend, Context, *Context.DrawCommandList);
}

void FAlphaBlendPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::AlphaBlend);
}
