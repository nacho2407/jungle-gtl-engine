#include "Render/Passes/Scene/HeightFogPass.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/Frame/FrameContext.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders/FullscreenDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Frame/ViewportRenderTargets.h"

void FHeightFogPass::PrepareInputs(FRenderPassContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.Frame)
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
