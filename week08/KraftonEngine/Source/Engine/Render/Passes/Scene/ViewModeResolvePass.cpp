#include "Render/Passes/Scene/ViewModeResolvePass.h"
#include "Render/Pipelines/Context/View/ViewportRenderTargets.h"

#include "Render/Submission/Builders/FullscreenDrawCommandBuilder.h"
#include "Render/Submission/Commands/DrawCommand.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Pipelines/Registry/ViewModePassConfig.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/View/ViewModeSurfaceSet.h"
#include "Render/Resources/ConstantBufferPool.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

namespace
{
EViewModePostProcessVariant GetViewModePostProcessVariant(const FRenderPipelineContext& Context)
{
    if (!Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        return EViewModePostProcessVariant::None;
    }

    return Context.ViewModePassRegistry->GetPostProcessVariant(Context.ActiveViewMode);
}

} // namespace

void FViewModeResolvePass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    const EViewModePostProcessVariant Variant = GetViewModePostProcessVariant(Context);
    if (Variant == EViewModePostProcessVariant::None || !Context.SceneView)
    {
        return;
    }

    switch (Variant)
    {
    case EViewModePostProcessVariant::SceneDepth:
        if (Targets && Targets->DepthTexture && Targets->DepthCopyTexture &&
            Targets->DepthTexture != Targets->DepthCopyTexture)
        {
            Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
            Context.Context->CopyResource(Targets->DepthCopyTexture, Targets->DepthTexture);
        }

        if (Targets && Targets->DepthCopySRV)
        {
            ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
            Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
        }
        break;

    case EViewModePostProcessVariant::WorldNormal:
    case EViewModePostProcessVariant::Outline:
    case EViewModePostProcessVariant::None:
    default:
        break;
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

void FViewModeResolvePass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FViewModeResolvePass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    const EViewModePostProcessVariant Variant = GetViewModePostProcessVariant(Context);
    if (Variant == EViewModePostProcessVariant::None)
    {
        return;
    }

    if (Variant == EViewModePostProcessVariant::WorldNormal && !Context.ActiveViewSurfaceSet)
    {
        return;
    }

    FFullscreenDrawCommandBuilder::Build(ERenderPass::PostProcess, Context, *Context.DrawCommandList, Variant);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    FDrawCommand& Command = Context.DrawCommandList->GetCommands().back();
    switch (Variant)
    {
    case EViewModePostProcessVariant::SceneDepth:
    {
        FSceneDepthPConstants Constants = {};
        Constants.Exponent = Context.SceneView->RenderOptions.Exponent;
        Constants.NearClip = Context.SceneView->NearClip;
        Constants.FarClip = Context.SceneView->FarClip;
        Constants.Range = Context.SceneView->RenderOptions.Range;
        Constants.Mode = static_cast<uint32>(Context.SceneView->RenderOptions.SceneDepthVisMode);

        FConstantBuffer* CB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::SceneDepth, sizeof(FSceneDepthPConstants));
        if (CB)
        {
            CB->Update(Context.Context, &Constants, sizeof(Constants));
            Command.PerShaderCB[0] = CB;
        }

        Command.Blend = EBlendState::Opaque;
        break;
    }
    case EViewModePostProcessVariant::WorldNormal:
    {
        ID3D11ShaderResourceView* NormalSRV = Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::ModifiedSurface1);
        if (!NormalSRV)
        {
            NormalSRV = Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::Surface1);
        }

        Command.DiffuseSRV = NormalSRV;
        Command.Blend = EBlendState::Opaque;
        break;
    }
    case EViewModePostProcessVariant::Outline:
    case EViewModePostProcessVariant::None:
    default:
        break;
    }

    Command.SortKey = FDrawCommand::BuildSortKey(
        Command.Pass,
        Command.Shader,
        nullptr,
        Command.DiffuseSRV,
        ToPostProcessUserBits(Variant));
}

void FViewModeResolvePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    const uint16 ExpectedUserBits = ToPostProcessUserBits(GetViewModePostProcessVariant(Context));
    if (ExpectedUserBits == 0)
    {
        return;
    }

    uint32 Start = 0;
    uint32 End = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, Start, End);
    for (uint32 Index = Start; Index < End; ++Index)
    {
        const FDrawCommand& Command = Context.DrawCommandList->GetCommands()[Index];
        if (static_cast<uint16>(Command.SortKey & 0xFFFu) == ExpectedUserBits)
        {
            Context.DrawCommandList->SubmitRange(Index, Index + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
