#include "Render/Passes/Scene/HeightFogPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/FullscreenDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Pipelines/Context/View/ViewportRenderTargets.h"
#include "Render/Pipelines/Registry/ViewModePassConfig.h"
#include "Render/Scene/Scene.h"

void FHeightFogPass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.SceneView)
    {
        return;
    }

    if (Targets && Targets->ViewportRenderTexture && Targets->SceneColorCopyTexture &&
        Targets->ViewportRenderTexture != Targets->SceneColorCopyTexture)
    {
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);
    }

    if (Targets && Targets->DepthTexture && Targets->DepthCopyTexture && Targets->DepthTexture != Targets->DepthCopyTexture)
    {
        Context.Context->CopyResource(Targets->DepthCopyTexture, Targets->DepthTexture);
    }

    if (Targets && Targets->SceneColorCopySRV)
    {
        ID3D11ShaderResourceView* SceneColorSRV = Targets->SceneColorCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);
    }

    if (Targets && Targets->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }
}

void FHeightFogPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FHeightFogPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.SceneView || !Context.SceneView->ShowFlags.bFog)
    {
        return;
    }

    if (Context.ViewModePassRegistry &&
        Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode) &&
        Context.ViewModePassRegistry->SuppressesSceneExtras(Context.ActiveViewMode))
    {
        return;
    }

    if (!Context.Scene || !Context.Scene->HasFog())
    {
        return;
    }

    FFullscreenDrawCommandBuilder::Build(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::None);
}

void FHeightFogPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, s, e);
        for (uint32 i = s; i < e; ++i)
        {
            const auto& c = Context.DrawCommandList->GetCommands()[i];
            const uint16 UserBits = static_cast<uint16>(c.SortKey & 0xFFFu);
            if (UserBits == ToPostProcessUserBits(EViewModePostProcessVariant::None))
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
