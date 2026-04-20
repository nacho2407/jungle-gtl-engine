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

    }

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->NormalSRV = nullptr;
        Context.StateCache->PerShaderCB[0] = nullptr;
        Context.StateCache->PerShaderCB[1] = nullptr;
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

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    FDrawCommand& Command = Context.DrawCommandList->GetCommands().back();
    if (UserBits == 2)
    {
        FSceneDepthPConstants Constants = {};
        Constants.Exponent = Context.Frame->RenderOptions.Exponent;
        Constants.NearClip = Context.Frame->NearClip;
        Constants.FarClip = Context.Frame->FarClip;
        Constants.Mode = static_cast<uint32>(Context.Frame->RenderOptions.SceneDepthVisMode);

        FConstantBuffer* CB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::SceneDepth, sizeof(FSceneDepthPConstants));
        if (CB)
        {
            CB->Update(Context.Context, &Constants, sizeof(Constants));
            Command.PerShaderCB[0] = CB;
        }

        // SceneDepth/Normal visualization은 기존 scene color 위에 반투명 합성이 아니라
        // 자체가 최종 화면이어야 한다.
        Command.Blend = EBlendState::Opaque;
    }
    else if (UserBits == 3)
    {
        // Normal view visualizes the normal surface directly through t0.
        // Decal pass copies Surface1 into ModifiedSurface1 even when no decal draw
        // occurs, so prefer the post-decal surface to reflect the final normal field.
        ID3D11ShaderResourceView* NormalSRV = Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::ModifiedSurface1);
        if (!NormalSRV)
        {
            NormalSRV = Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1);
        }

        Command.DiffuseSRV = NormalSRV;
        Command.Blend = EBlendState::Opaque;
    }

    Command.SortKey = FDrawCommand::BuildSortKey(
        Command.Pass,
        Command.Shader,
        nullptr,
        Command.DiffuseSRV,
        UserBits);
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
