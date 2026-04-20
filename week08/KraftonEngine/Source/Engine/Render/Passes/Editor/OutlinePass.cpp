#include "Render/Passes/Editor/OutlinePass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Commands/DrawCommand.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Builders//FullscreenDrawCommandBuilder.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FOutlinePass::PrepareInputs(FRenderPassContext& Context)
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

    if (Context.Frame->StencilCopySRV)
    {
        ID3D11ShaderResourceView* StencilSRV = Context.Frame->StencilCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &StencilSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->NormalSRV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FOutlinePass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FOutlinePass::BuildDrawCommands(FRenderPassContext& Context)
{
    FFullscreenDrawCommandBuilder::Build(ERenderPass::PostProcess, Context, *Context.DrawCommandList, 1);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    FOutlinePostProcessConstants Constants = {};
    Constants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
    Constants.OutlineThickness = 1.0f;

    FConstantBuffer* OutlineCB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::Outline, sizeof(FOutlinePostProcessConstants));
    if (!OutlineCB)
    {
        return;
    }

    OutlineCB->Update(Context.Context, &Constants, sizeof(Constants));
    Context.DrawCommandList->GetCommands().back().PerShaderCB[0] = OutlineCB;
}

void FOutlinePass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FOutlinePass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, s, e);
        for (uint32 i = s; i < e; ++i)
        {
            const auto& c = Context.DrawCommandList->GetCommands()[i];
            if ((c.SortKey & 0xFFFu) == 1)
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
