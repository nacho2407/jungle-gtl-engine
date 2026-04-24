// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Passes/Editor/SelectionMaskPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FSelectionMaskPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FSelectionMaskPass::PrepareTargets(FRenderPipelineContext& Context)
{
    Context.Context->OMSetRenderTargets(0, nullptr, Context.GetViewportDSV());
}

void FSelectionMaskPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    DrawCommandBuild::BuildMeshDrawCommand(Proxy, ERenderPass::SelectionMask, Context, *Context.DrawCommandList);
}

void FSelectionMaskPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::SelectionMask);
}
