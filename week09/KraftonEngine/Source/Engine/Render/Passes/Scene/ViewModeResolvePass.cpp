#include "Render/Passes/Scene/ViewModeResolvePass.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"

#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Pipelines/Registry/ViewModePassRegistry.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/ViewMode/SceneViewModeSurfaces.h"
#include "Render/Resources/Buffers/ConstantBufferPool.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

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
        if (Targets)
        {
            CopyViewportDepthToReadable(Context);
            BindDepthInput(Context);
        }
        break;

    case EViewModePostProcessVariant::WorldNormal:
    case EViewModePostProcessVariant::Outline:
    case EViewModePostProcessVariant::None:
    default:
        break;
    }

}

void FViewModeResolvePass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    const EViewModePostProcessVariant Variant = GetViewModePostProcessVariant(Context);
    if (Variant == EViewModePostProcessVariant::None)
    {
        return;
    }

    if (Variant == EViewModePostProcessVariant::WorldNormal && !Context.ActiveViewSurfaces)
    {
        return;
    }

    DrawCommandBuilder::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, Variant);

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

        FConstantBuffer* CB = FConstantBufferPool::Get().GetBuffer(ERenderCBKey::SceneDepth, sizeof(FSceneDepthPConstants));
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
        ID3D11ShaderResourceView* NormalSRV = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::ModifiedSurface1);
        if (!NormalSRV)
        {
            NormalSRV = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::Surface1);
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
