#include "Render/Renderer/RenderPipelineRunner.h"

#include "Render/Core/PassTypes.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Renderer/RenderPassRegistry.h"
#include "Render/Renderer/RenderPipelineRegistry.h"
#include "Render/Renderer/RenderMarkers.h"

namespace
{
const wchar_t* GetRenderPassMarkerName(ERenderPassNodeType PassType)
{
    switch (PassType)
    {
    case ERenderPassNodeType::DepthPrePass: return L"DepthPrePass";
    case ERenderPassNodeType::BaseDrawPass: return L"OpaquePass";
    case ERenderPassNodeType::DecalPass: return L"DecalPass";
    case ERenderPassNodeType::LightingPass: return L"LightingPass";
    case ERenderPassNodeType::AdditiveDecalPass: return L"AdditiveDecalPass";
    case ERenderPassNodeType::AlphaBlendPass: return L"AlphaBlendPass";
    case ERenderPassNodeType::ViewModePostProcessPass: return L"ViewModePostProcessPass";
    case ERenderPassNodeType::SelectionMaskPass: return L"SelectionMaskPass";
    case ERenderPassNodeType::OutlinePass: return L"OutlinePass";
    case ERenderPassNodeType::DebugLinePass: return L"DebugLinePass";
    case ERenderPassNodeType::GizmoPass: return L"GizmoPass";
    case ERenderPassNodeType::OverlayTextPass: return L"OverlayTextPass";
    case ERenderPassNodeType::HeightFogPass: return L"HeightFogPass";
    case ERenderPassNodeType::FXAAPass: return L"FXAAPass";
    default: return L"RenderPass";
    }
}
}

void FRenderPipelineRunner::ExecutePipeline(
    ERenderPipelineType Root,
    FRenderPassContext& Context,
    const FFrameContext& Frame,
    const FRenderPipelineRegistry& PipelineRegistry,
    const FRenderPassRegistry& PassRegistry) const
{
    ExecutePipelineRecursive(Root, Context, Frame, PipelineRegistry, PassRegistry);
}

void FRenderPipelineRunner::ExecutePipelineRecursive(
    ERenderPipelineType Type,
    FRenderPassContext& Context,
    const FFrameContext& Frame,
    const FRenderPipelineRegistry& PipelineRegistry,
    const FRenderPassRegistry& PassRegistry) const
{
    const FRenderPipelineDesc* Desc = PipelineRegistry.FindPipeline(Type);
    if (!Desc)
    {
        return;
    }

    const bool bSkipLightingPass =
        Context.ViewModePassRegistry &&
        Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode) &&
        !Context.ViewModePassRegistry->UsesLightingPass(Context.ActiveViewMode);

    for (const FRenderNodeRef& Child : Desc->Children)
    {
        if (Child.Kind == ERenderNodeKind::Pipeline)
        {
            ExecutePipelineRecursive((ERenderPipelineType)Child.TypeValue, Context, Frame, PipelineRegistry, PassRegistry);
        }
        else
        {
            const ERenderPassNodeType PassType = (ERenderPassNodeType)Child.TypeValue;
            if (bSkipLightingPass && PassType == ERenderPassNodeType::LightingPass)
            {
                continue;
            }

            if (FRenderPass* Pass = PassRegistry.FindPass(PassType))
            {
#if WITH_RENDER_MARKERS
                FScopedGpuEvent Event(*Context.Renderer, GetRenderPassMarkerName(PassType));
#endif
                Pass->Execute(Context);
            }
        }
    }
}
