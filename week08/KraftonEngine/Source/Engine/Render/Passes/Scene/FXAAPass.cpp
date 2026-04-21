#include "Render/Passes/Scene/FXAAPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"

void FFXAAPass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Targets || !Targets->SceneColorCopySRV)
    {
        return;
    }

    if (Targets->ViewportRenderTexture && Targets->SceneColorCopyTexture &&
        Targets->ViewportRenderTexture != Targets->SceneColorCopyTexture)
    {
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);
    }

    ID3D11ShaderResourceView* SceneColorSRV = Targets->SceneColorCopySRV;
    Context.Context->PSSetShaderResources(0, 1, &SceneColorSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);
}

void FFXAAPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
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

    DrawCommandBuilder::BuildFullscreenDrawCommand(ERenderPass::FXAA, Context, *Context.DrawCommandList);
}

void FFXAAPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::FXAA);
}
