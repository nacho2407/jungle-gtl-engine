#include "Render/Passes/Editor/OutlinePass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Submission/Commands/DrawCommand.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/FullscreenDrawCommandBuilder.h"
#include "Render/Resources/ConstantBufferPool.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Pipelines/Context/View/ViewportRenderTargets.h"
#include "Render/Pipelines/Registry/ViewModePassConfig.h"

void FOutlinePass::PrepareInputs(FRenderPipelineContext& Context)
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

void FOutlinePass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FOutlinePass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    FFullscreenDrawCommandBuilder::Build(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::Outline);

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

void FOutlinePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, s, e);
        for (uint32 i = s; i < e; ++i)
        {
            const auto& c = Context.DrawCommandList->GetCommands()[i];
            if ((c.SortKey & 0xFFFu) == ToPostProcessUserBits(EViewModePostProcessVariant::Outline))
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
