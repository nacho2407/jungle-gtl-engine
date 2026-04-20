#include "Render/Passes/PostProcess/FXAAPass.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/View/SceneView.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/FullscreenDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/View/ViewportRenderTargets.h"

void FFXAAPass::PrepareInputs(FRenderPassContext& Context)
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

void FFXAAPass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FFXAAPass::BuildDrawCommands(FRenderPassContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.Frame || !Context.Frame->ShowFlags.bFXAA)
    {
        return;
    }

    if (Context.ViewModePassRegistry &&
        Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode) &&
        Context.ViewModePassRegistry->SuppressesSceneExtras(Context.ActiveViewMode))
    {
        return;
    }

    if (!Targets || !Targets->SceneColorCopySRV || !Targets->SceneColorCopyTexture)
    {
        return;
    }

    FFullscreenDrawCommandBuilder::Build(ERenderPass::FXAA, Context, *Context.DrawCommandList);
}

void FFXAAPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::FXAA, s, e);
        if (s < e)
            Context.DrawCommandList->SubmitRange(s, e, *Context.Device, Context.Context, *Context.StateCache);
    }
}
