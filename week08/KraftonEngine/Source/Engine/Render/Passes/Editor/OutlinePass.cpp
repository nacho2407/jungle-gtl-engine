#include "Render/Passes/Editor/OutlinePass.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/View/SceneView.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Submission/Commands/DrawCommand.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/FullscreenDrawCommandBuilder.h"
#include "Render/Resources/Pools/ConstantBufferPool.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/View/ViewportRenderTargets.h"

void FOutlinePass::PrepareInputs(FRenderPassContext& Context)
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

    if (Targets && Targets->StencilCopySRV)
    {
        ID3D11ShaderResourceView* StencilSRV = Targets->StencilCopySRV;
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
