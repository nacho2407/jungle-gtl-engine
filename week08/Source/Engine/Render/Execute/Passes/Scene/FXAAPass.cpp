// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Passes/Scene/FXAAPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"

void FFXAAPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.Targets || !Context.Targets->SceneColorCopySRV)
    {
        return;
    }

    CopyViewportColorToSceneColor(Context);
    BindSceneColorInput(Context, true);
}


void FFXAAPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.SceneView || !Context.SceneView->ShowFlags.bFXAA)
    {
        return;
    }

    if (!Targets || !Targets->SceneColorCopySRV || !Targets->SceneColorCopyTexture)
    {
        return;
    }

    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::FXAA, Context, *Context.DrawCommandList);
}

void FFXAAPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::FXAA);
}

