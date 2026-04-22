#include "Render/Execute/Registry/RenderPipelineRegistry.h"

namespace
{
FRenderNodeRef PipelineNode(ERenderPipelineType Type)
{
    return { ERenderNodeKind::Pipeline, (int32)Type };
}

FRenderNodeRef PassNode(ERenderPassNodeType Type)
{
    return { ERenderNodeKind::Pass, (int32)Type };
}
} // namespace

void FRenderPipelineRegistry::Initialize()
{
    Release();

    FRenderPipelineDesc DefaultRootPipeline = { ERenderPipelineType::DefaultRootPipeline, { PipelineNode(ERenderPipelineType::ScenePipeline) } };
    Pipelines.emplace((int32)DefaultRootPipeline.Type, DefaultRootPipeline);

    FRenderPipelineDesc EditorRootPipeline = { ERenderPipelineType::EditorRootPipeline, {
        PipelineNode(ERenderPipelineType::ScenePipeline),
        PipelineNode(ERenderPipelineType::OverlayPipeline)
    } };
    Pipelines.emplace((int32)EditorRootPipeline.Type, EditorRootPipeline);

    FRenderPipelineDesc Scene = { ERenderPipelineType::ScenePipeline, {
        PipelineNode(ERenderPipelineType::LitPipeline),
        PipelineNode(ERenderPipelineType::PostProcessPipeline)
    } };
    Pipelines.emplace((int32)Scene.Type, Scene);

    FRenderPipelineDesc LitPipeline = { ERenderPipelineType::LitPipeline, {
        PassNode(ERenderPassNodeType::DepthPrePass),
        PassNode(ERenderPassNodeType::LightCullingPass),
        PassNode(ERenderPassNodeType::OpaquePass),
        PassNode(ERenderPassNodeType::DecalPass),
        PassNode(ERenderPassNodeType::LightingPass)
    } };
    Pipelines.emplace((int32)LitPipeline.Type, LitPipeline);

    FRenderPipelineDesc PostProcessPipeline = { ERenderPipelineType::PostProcessPipeline, {
        PassNode(ERenderPassNodeType::NonLitViewModePass),
        PassNode(ERenderPassNodeType::HeightFogPass),
        PassNode(ERenderPassNodeType::FXAAPass)

    } };
    Pipelines.emplace((int32)PostProcessPipeline.Type, PostProcessPipeline);

    FRenderPipelineDesc OverlayPipeline = { ERenderPipelineType::OverlayPipeline, {
        PassNode(ERenderPassNodeType::LightHitMapPass),
        PassNode(ERenderPassNodeType::DebugLinePass),
        PipelineNode(ERenderPipelineType::Outline),
        PassNode(ERenderPassNodeType::OverlayBillboardPass),
        PassNode(ERenderPassNodeType::GizmoPass),
        PassNode(ERenderPassNodeType::OverlayTextPass),
		PipelineNode(ERenderPipelineType::Outline),
    } };
    Pipelines.emplace((int32)OverlayPipeline.Type, OverlayPipeline);

    FRenderPipelineDesc Outline = { ERenderPipelineType::Outline, {
        PassNode(ERenderPassNodeType::SelectionMaskPass),
        PassNode(ERenderPassNodeType::OutlinePass)
    } };
    Pipelines.emplace((int32)Outline.Type, Outline);
}

void FRenderPipelineRegistry::Release()
{
    Pipelines.clear();
}

const FRenderPipelineDesc* FRenderPipelineRegistry::FindPipeline(ERenderPipelineType Type) const
{
    auto It = Pipelines.find((int32)Type);
    return It != Pipelines.end() ? &It->second : nullptr;
}
