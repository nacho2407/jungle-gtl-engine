#include "Render/Execute/Passes/Scene/NonLitViewModePass.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"

#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

namespace
{
EViewModePostProcessVariant GetViewModePostProcessVariant(const FRenderPipelineContext& Context)
{
    if (!Context.ViewMode.Registry || !Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode))
    {
        return EViewModePostProcessVariant::None;
    }

    return Context.ViewMode.Registry->GetPostProcessVariant(Context.ViewMode.ActiveViewMode);
}

} // namespace

void FNonLitViewModePass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets*     Targets = Context.Targets;
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

void FNonLitViewModePass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    const EViewModePostProcessVariant Variant = GetViewModePostProcessVariant(Context);
    if (Variant == EViewModePostProcessVariant::None)
    {
        return;
    }

    if (Variant == EViewModePostProcessVariant::WorldNormal && !Context.ViewMode.Surfaces)
    {
        return;
    }

    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, Variant);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    FDrawCommand& Command = Context.DrawCommandList->GetCommands().back();
    switch (Variant)
    {
    case EViewModePostProcessVariant::SceneDepth:
    {
        FSceneDepthPCBData Constants = {};
        Constants.Exponent           = Context.SceneView->RenderOptions.Exponent;
        Constants.NearClip           = Context.SceneView->NearClip;
        Constants.FarClip            = Context.SceneView->FarClip;
        Constants.Range              = Context.SceneView->RenderOptions.Range;
        Constants.Mode               = static_cast<uint32>(Context.SceneView->RenderOptions.SceneDepthVisMode);

        FConstantBuffer* CB = FConstantBufferCache::Get().GetBuffer(ERenderCBKey::SceneDepth, sizeof(FSceneDepthPCBData));
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
        ID3D11ShaderResourceView* NormalSRV = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::ModifiedSurface1);
        if (!NormalSRV)
        {
            NormalSRV = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::Surface1);
        }

        Command.DiffuseSRV = NormalSRV;
        Command.Blend      = EBlendState::Opaque;
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

void FNonLitViewModePass::SubmitDrawCommands(FRenderPipelineContext& Context)
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
    uint32 End   = 0;
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
