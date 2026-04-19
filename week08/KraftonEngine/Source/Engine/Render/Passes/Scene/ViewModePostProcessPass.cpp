#include "Render/Passes/Scene/ViewModePostProcessPass.h"

#include "Render/Builders//FullscreenDrawCommandBuilder.h"
#include "Render/Commands/DrawCommand.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/FrameContext.h"
#include "Render/Core/PassTypes.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/D3D11/Frame/ViewModeSurfaceSet.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

namespace
{
uint16 GetViewModePostProcessBits(const FRenderPassContext& Context)
{
    if (!Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        return 0;
    }

    return Context.ViewModePassRegistry->GetPostProcessUserBits(Context.ActiveViewMode);
}
}

void FViewModePostProcessPass::PrepareInputs(FRenderPassContext& Context)
{
    const uint16 UserBits = GetViewModePostProcessBits(Context);
    if (UserBits == 0 || !Context.Frame)
    {
        return;
    }

    if (UserBits == 2)
    {
        if (Context.Frame->DepthTexture && Context.Frame->DepthCopyTexture &&
            Context.Frame->DepthTexture != Context.Frame->DepthCopyTexture)
        {
            Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
            Context.Context->CopyResource(Context.Frame->DepthCopyTexture, Context.Frame->DepthTexture);
        }

        if (Context.Frame->DepthCopySRV)
        {
            ID3D11ShaderResourceView* DepthSRV = Context.Frame->DepthCopySRV;
            Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
        }

        FSceneDepthPConstants Constants = {};
        Constants.Exponent = Context.Frame->RenderOptions.Exponent;
        Constants.NearClip = Context.Frame->NearClip;
        Constants.FarClip = Context.Frame->FarClip;
        Constants.Mode = static_cast<uint32>(Context.Frame->RenderOptions.SceneDepthVisMode);

        FConstantBuffer* CB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::SceneDepth, sizeof(FSceneDepthPConstants));
        if (CB)
        {
            CB->Update(Context.Context, &Constants, sizeof(Constants));
            ID3D11Buffer* RawCB = CB->GetBuffer();
            Context.Context->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
        }
    }
    else if (UserBits == 3)
    {
        if (!Context.ActiveViewSurfaceSet)
        {
            return;
        }

        ID3D11ShaderResourceView* NormalSRV = Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::ModifiedSurface1);
        if (!NormalSRV)
        {
            NormalSRV = Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1);
        }

        Context.Context->PSSetShaderResources(0, 1, &NormalSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FViewModePostProcessPass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FViewModePostProcessPass::BuildDrawCommands(FRenderPassContext& Context)
{
    const uint16 UserBits = GetViewModePostProcessBits(Context);
    if (UserBits == 0)
    {
        return;
    }

    if (UserBits == 3 && !Context.ActiveViewSurfaceSet)
    {
        return;
    }

    FFullscreenDrawCommandBuilder::Build(ERenderPass::PostProcess, Context, *Context.DrawCommandList, UserBits);
}

void FViewModePostProcessPass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FViewModePostProcessPass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    const uint16 UserBits = GetViewModePostProcessBits(Context);
    if (UserBits == 0)
    {
        return;
    }

    uint32 Start = 0;
    uint32 End = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, Start, End);
    for (uint32 Index = Start; Index < End; ++Index)
    {
        const FDrawCommand& Command = Context.DrawCommandList->GetCommands()[Index];
        if (static_cast<uint16>(Command.SortKey & 0xFFFu) == UserBits)
        {
            Context.DrawCommandList->SubmitRange(Index, Index + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
