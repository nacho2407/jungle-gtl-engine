#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Pipelines/RenderPipelineRunner.h"

#include "Render/Pipelines/Registry/ViewModePassConfig.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Registry/RenderPassRegistry.h"
#include "Render/Pipelines/Registry/RenderPipelineRegistry.h"
#include "Render/Pipelines/Context/RenderMarkers.h"

namespace
{
const wchar_t* GetRenderPassMarkerName(ERenderPassNodeType PassType)
{
    switch (PassType)
    {
    case ERenderPassNodeType::DepthPrePass:
        return L"DepthPrePass";
    case ERenderPassNodeType::BaseDrawPass:
        return L"OpaquePass";
    case ERenderPassNodeType::DecalPass:
        return L"DecalPass";
    case ERenderPassNodeType::LightingPass:
        return L"LightingPass";
    case ERenderPassNodeType::AdditiveDecalPass:
        return L"AdditiveDecalPass";
    case ERenderPassNodeType::AlphaBlendPass:
        return L"AlphaBlendPass";
    case ERenderPassNodeType::ViewModeResolvePass:
        return L"ViewModeResolvePass";
    case ERenderPassNodeType::SelectionMaskPass:
        return L"SelectionMaskPass";
    case ERenderPassNodeType::OutlinePass:
        return L"OutlinePass";
    case ERenderPassNodeType::DebugLinePass:
        return L"DebugLinePass";
    case ERenderPassNodeType::GizmoPass:
        return L"GizmoPass";
    case ERenderPassNodeType::OverlayTextPass:
        return L"OverlayTextPass";
    case ERenderPassNodeType::HeightFogPass:
        return L"HeightFogPass";
    case ERenderPassNodeType::FXAAPass:
        return L"FXAAPass";
    default:
        return L"RenderPass";
    }
}
} // namespace

void FRenderPipelineRunner::ExecutePipeline(
    ERenderPipelineType Root,
    FRenderPipelineContext& Context,
    const FSceneView& SceneView,
    const FRenderPipelineRegistry& PipelineRegistry,
    const FRenderPassRegistry& PassRegistry) const
{
    ExecutePipelineRecursive(Root, Context, SceneView, PipelineRegistry, PassRegistry);
}

void FRenderPipelineRunner::ExecutePipelineRecursive(
    ERenderPipelineType Type,
    FRenderPipelineContext& Context,
    const FSceneView& SceneView,
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

    const bool bSkipDepthPrePass =
        Context.ViewModePassRegistry &&
        Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode) &&
        !Context.ViewModePassRegistry->UsesDepthPrePass(Context.ActiveViewMode);

    for (const FRenderNodeRef& Child : Desc->Children)
    {
        if (Child.Kind == ERenderNodeKind::Pipeline)
        {
            ExecutePipelineRecursive((ERenderPipelineType)Child.TypeValue, Context, SceneView, PipelineRegistry, PassRegistry);
        }
        else
        {
            const ERenderPassNodeType PassType = (ERenderPassNodeType)Child.TypeValue;
            if (bSkipLightingPass && PassType == ERenderPassNodeType::LightingPass)
            {
                continue;
            }

            if (bSkipDepthPrePass && PassType == ERenderPassNodeType::DepthPrePass)
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
