#include "Render/Pipelines/Runner/RenderPipelineRunner.h"

#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Registry/RenderPassRegistry.h"
#include "Render/Pipelines/Registry/RenderPipelineRegistry.h"
#include "Render/Pipelines/Registry/ViewModePassRegistry.h"
#include "Render/Pipelines/Runner/RenderMarkers.h"

namespace
{
const wchar_t* GetRenderPassMarkerName(ERenderPassNodeType PassType)
{
    switch (PassType)
    {
    case ERenderPassNodeType::DepthPrePass: return L"DepthPrePass";
    case ERenderPassNodeType::LightCullingPass: return L"LightCullingPass";
    case ERenderPassNodeType::BaseDrawPass: return L"OpaquePass";
    case ERenderPassNodeType::DecalPass: return L"DecalPass";
    case ERenderPassNodeType::LightingPass: return L"LightingPass";
    case ERenderPassNodeType::AdditiveDecalPass: return L"AdditiveDecalPass";
    case ERenderPassNodeType::AlphaBlendPass: return L"AlphaBlendPass";
    case ERenderPassNodeType::ViewModeResolvePass: return L"ViewModeResolvePass";
    case ERenderPassNodeType::SelectionMaskPass: return L"SelectionMaskPass";
    case ERenderPassNodeType::OutlinePass: return L"OutlinePass";
    case ERenderPassNodeType::DebugLinePass: return L"DebugLinePass";
    case ERenderPassNodeType::GizmoPass: return L"GizmoPass";
    case ERenderPassNodeType::OverlayTextPass: return L"OverlayTextPass";
    case ERenderPassNodeType::HeightFogPass: return L"HeightFogPass";
    case ERenderPassNodeType::FXAAPass: return L"FXAAPass";
    case ERenderPassNodeType::PresentPass: return L"PresentPass";
    default: return L"RenderPass";
    }
}

bool ShouldExecutePass(const FRenderPipelineContext& Context, ERenderPassNodeType PassType)
{
    const FViewModePassRegistry* Registry = Context.ViewModePassRegistry;
    if (!Registry || !Registry->HasConfig(Context.ActiveViewMode))
    {
        return true;
    }

    switch (PassType)
    {
    case ERenderPassNodeType::DepthPrePass:
        return Registry->UsesDepthPrePass(Context.ActiveViewMode);
    case ERenderPassNodeType::BaseDrawPass:
        return Registry->UsesBaseDraw(Context.ActiveViewMode);
    case ERenderPassNodeType::DecalPass:
        return Registry->UsesDecal(Context.ActiveViewMode);
    case ERenderPassNodeType::LightingPass:
        return Registry->UsesLightingPass(Context.ActiveViewMode);
    case ERenderPassNodeType::AdditiveDecalPass:
        return Registry->UsesAdditiveDecal(Context.ActiveViewMode);
    case ERenderPassNodeType::AlphaBlendPass:
        return Registry->UsesAlphaBlend(Context.ActiveViewMode);
    case ERenderPassNodeType::ViewModeResolvePass:
        return Registry->UsesViewModeResolve(Context.ActiveViewMode);
    case ERenderPassNodeType::HeightFogPass:
        return Registry->UsesHeightFog(Context.ActiveViewMode);
    case ERenderPassNodeType::FXAAPass:
        return Registry->UsesFXAA(Context.ActiveViewMode);
    default:
        return true;
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

    for (const FRenderNodeRef& Child : Desc->Children)
    {
        if (Child.Kind == ERenderNodeKind::Pipeline)
        {
            ExecutePipelineRecursive((ERenderPipelineType)Child.TypeValue, Context, SceneView, PipelineRegistry, PassRegistry);
            continue;
        }

        const ERenderPassNodeType PassType = (ERenderPassNodeType)Child.TypeValue;
        if (!ShouldExecutePass(Context, PassType))
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
