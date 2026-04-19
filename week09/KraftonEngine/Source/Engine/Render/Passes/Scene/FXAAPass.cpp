#include "Render/Passes/Scene/FXAAPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//FullscreenDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FFXAAPass::PrepareInputs(FRenderPassContext& Context)
{
    if (!Context.Frame || !Context.Frame->SceneColorCopySRV)
    {
        return;
    }

    if (Context.Frame->ViewportRenderTexture && Context.Frame->SceneColorCopyTexture &&
        Context.Frame->ViewportRenderTexture != Context.Frame->SceneColorCopyTexture)
    {
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Context.Frame->SceneColorCopyTexture, Context.Frame->ViewportRenderTexture);
    }

    ID3D11ShaderResourceView* SceneColorSRV = Context.Frame->SceneColorCopySRV;
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

    if (!Context.Frame->SceneColorCopySRV || !Context.Frame->SceneColorCopyTexture)
    {
        return;
    }

    FFullscreenDrawCommandBuilder::Build(ERenderPass::FXAA, Context, *Context.DrawCommandList);
}

void FFXAAPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
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
