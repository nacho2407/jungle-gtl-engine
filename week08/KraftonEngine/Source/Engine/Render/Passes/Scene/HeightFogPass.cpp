#include "Render/Passes/Scene/HeightFogPass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//FullscreenDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FHeightFogPass::PrepareInputs(FRenderPassContext& Context)
{
    if (!Context.Frame)
    {
        return;
    }

    if (Context.Frame->ViewportRenderTexture && Context.Frame->SceneColorCopyTexture &&
        Context.Frame->ViewportRenderTexture != Context.Frame->SceneColorCopyTexture)
    {
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Context.Frame->SceneColorCopyTexture, Context.Frame->ViewportRenderTexture);
    }

    if (Context.Frame->DepthTexture && Context.Frame->DepthCopyTexture && Context.Frame->DepthTexture != Context.Frame->DepthCopyTexture)
    {
        Context.Context->CopyResource(Context.Frame->DepthCopyTexture, Context.Frame->DepthTexture);
    }

    if (Context.Frame->SceneColorCopySRV)
    {
        ID3D11ShaderResourceView* SceneColorSRV = Context.Frame->SceneColorCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);
    }

    if (Context.Frame->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Context.Frame->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }
}

void FHeightFogPass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FHeightFogPass::BuildDrawCommands(FRenderPassContext& Context)
{
    if (!Context.Frame || !Context.Frame->ShowFlags.bFog)
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

    FFullscreenDrawCommandBuilder::Build(ERenderPass::PostProcess, Context, *Context.DrawCommandList, 0);
}

void FHeightFogPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FHeightFogPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, s, e);
        for (uint32 i = s; i < e; ++i)
        {
            const auto& c = Context.DrawCommandList->GetCommands()[i];
            const uint16 UserBits = static_cast<uint16>(c.SortKey & 0xFFFu);
            if (UserBits == 0)
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
