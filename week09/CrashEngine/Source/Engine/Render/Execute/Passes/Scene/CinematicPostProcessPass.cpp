#include "Render/Execute/Passes/Scene/CinematicPostProcessPass.h"

#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"

namespace
{
bool HasActiveCinematicEffect(const FCameraScreenEffectInfo& Effects)
{
    return (Effects.bEnableFade && Effects.FadeAmount > 0.0f) ||
           (Effects.bEnableLetterBox && Effects.LetterBoxAmount > 0.0f) ||
           Effects.bEnableGammaCorrection ||
           (Effects.bEnableVignette && Effects.VignetteIntensity > 0.0f);
}
} // namespace

bool FCinematicPostProcessPass::IsEnabled(const FRenderPipelineContext& Context) const
{
    return Context.SceneView && HasActiveCinematicEffect(Context.SceneView->ScreenEffects);
}

void FCinematicPostProcessPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.Targets || !Context.Targets->SceneColorCopySRV)
    {
        return;
    }

    CopyViewportColorToSceneColor(Context);
    BindSceneColorInput(Context, true);
}

void FCinematicPostProcessPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!IsEnabled(Context))
    {
        return;
    }

    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Targets || !Targets->SceneColorCopySRV || !Targets->SceneColorCopyTexture)
    {
        return;
    }

    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::CinematicPostProcess, Context, *Context.DrawCommandList);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    const FCameraScreenEffectInfo& Effects = Context.SceneView->ScreenEffects;

    FCinematicPostProcessCBData Constants = {};
    Constants.FadeColor = Effects.FadeColor;
    Constants.FadeAmount = Effects.bEnableFade ? Effects.FadeAmount : 0.0f;
    Constants.LetterBoxAmount = Effects.bEnableLetterBox ? Effects.LetterBoxAmount : 0.0f;
    Constants.Gamma = Effects.bEnableGammaCorrection ? Effects.Gamma : 1.0f;
    Constants.VignetteIntensity = Effects.bEnableVignette ? Effects.VignetteIntensity : 0.0f;
    Constants.VignetteRadius = Effects.VignetteRadius;
    Constants.VignetteSoftness = Effects.VignetteSoftness;
    Constants.bEnableGammaCorrection = Effects.bEnableGammaCorrection ? 1.0f : 0.0f;
    Constants.bEnableVignette = Effects.bEnableVignette ? 1.0f : 0.0f;

    FConstantBuffer* CinematicCB =
        FConstantBufferCache::Get().GetBuffer(ERenderCBKey::CinematicPostProcess, sizeof(FCinematicPostProcessCBData));
    if (!CinematicCB)
    {
        return;
    }

    CinematicCB->Update(Context.Context, &Constants, sizeof(Constants));
    Context.DrawCommandList->GetCommands().back().PerShaderCB[0] = CinematicCB;
}

void FCinematicPostProcessPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::CinematicPostProcess);
}
