#include "Render/Pipelines/Registry/RenderPipelineRegistry.h"

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

    FRenderPipelineDesc DefaultScene = { ERenderPipelineType::DefaultScene, { PipelineNode(ERenderPipelineType::Scene) } };
    Pipelines.emplace((int32)DefaultScene.Type, DefaultScene);

    FRenderPipelineDesc EditorScene = { ERenderPipelineType::EditorScene, {
        PipelineNode(ERenderPipelineType::Scene),
        PipelineNode(ERenderPipelineType::EditorOverlay)
    } };
    Pipelines.emplace((int32)EditorScene.Type, EditorScene);

    FRenderPipelineDesc Scene = { ERenderPipelineType::Scene, {
        PipelineNode(ERenderPipelineType::SceneMain),
        PassNode(ERenderPassNodeType::AdditiveDecalPass),
        PassNode(ERenderPassNodeType::AlphaBlendPass),
        PipelineNode(ERenderPipelineType::ScenePostProcess)
    } };
    Pipelines.emplace((int32)Scene.Type, Scene);

    FRenderPipelineDesc SceneMain = { ERenderPipelineType::SceneMain, {
        PassNode(ERenderPassNodeType::DepthPrePass),
        PassNode(ERenderPassNodeType::LightCullingPass),
        PassNode(ERenderPassNodeType::BaseDrawPass),
        PassNode(ERenderPassNodeType::DecalPass),
        PassNode(ERenderPassNodeType::LightingPass)
    } };
    Pipelines.emplace((int32)SceneMain.Type, SceneMain);

    FRenderPipelineDesc ScenePostProcess = { ERenderPipelineType::ScenePostProcess, {
        PassNode(ERenderPassNodeType::ViewModeResolvePass),
        PassNode(ERenderPassNodeType::HeightFogPass),
        PassNode(ERenderPassNodeType::FXAAPass)

    } };
    Pipelines.emplace((int32)ScenePostProcess.Type, ScenePostProcess);

    FRenderPipelineDesc EditorOverlay = { ERenderPipelineType::EditorOverlay, {
        PassNode(ERenderPassNodeType::LightHitMapPass),
        PassNode(ERenderPassNodeType::DebugLinePass),
        PipelineNode(ERenderPipelineType::Outline),
        PassNode(ERenderPassNodeType::OverlayBillboardPass),
        PassNode(ERenderPassNodeType::GizmoPass),
        PassNode(ERenderPassNodeType::OverlayTextPass),
		PipelineNode(ERenderPipelineType::Outline),
    } };
    Pipelines.emplace((int32)EditorOverlay.Type, EditorOverlay);

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
