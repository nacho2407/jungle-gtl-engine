// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Passes/Scene/AlphaBlendPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
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
    DrawCommandBuild::BuildMeshDrawCommand(Proxy, ERenderPass::AlphaBlend, Context, *Context.DrawCommandList);
}

void FAlphaBlendPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::AlphaBlend);
}
