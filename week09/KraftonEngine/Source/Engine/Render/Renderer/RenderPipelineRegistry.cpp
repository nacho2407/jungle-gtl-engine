#include "Render/Renderer/RenderPipelineRegistry.h"

namespace
{
FRenderNodeRef PipelineNode(ERenderPipelineType Type)
{
    FRenderNodeRef Ref;
    Ref.Kind = ERenderNodeKind::Pipeline;
    Ref.TypeValue = (int32)Type;
    return Ref;
}

FRenderNodeRef PassNode(ERenderPassNodeType Type)
{
    FRenderNodeRef Ref;
    Ref.Kind = ERenderNodeKind::Pass;
    Ref.TypeValue = (int32)Type;
    return Ref;
}
}

void FRenderPipelineRegistry::Initialize()
{
    Release();

    FRenderPipelineDesc DefaultScene;
    DefaultScene.Type = ERenderPipelineType::DefaultScene;
    DefaultScene.Children = { PipelineNode(ERenderPipelineType::Scene) };
    Pipelines.emplace((int32)DefaultScene.Type, DefaultScene);

    FRenderPipelineDesc EditorScene;
    EditorScene.Type = ERenderPipelineType::EditorScene;
    EditorScene.Children = {
        PipelineNode(ERenderPipelineType::Scene),
        PipelineNode(ERenderPipelineType::EditorOverlay)
    };
    Pipelines.emplace((int32)EditorScene.Type, EditorScene);

    FRenderPipelineDesc Scene;
    Scene.Type = ERenderPipelineType::Scene;
    Scene.Children = {
        PassNode(ERenderPassNodeType::DepthPrePass),
        PipelineNode(ERenderPipelineType::SceneViewMode),
        PassNode(ERenderPassNodeType::AdditiveDecalPass),
        PassNode(ERenderPassNodeType::AlphaBlendPass),
        PipelineNode(ERenderPipelineType::ScenePostProcess)
    };
    Pipelines.emplace((int32)Scene.Type, Scene);

    FRenderPipelineDesc SceneViewMode;
    SceneViewMode.Type = ERenderPipelineType::SceneViewMode;
    SceneViewMode.Children = {
        PassNode(ERenderPassNodeType::BaseDrawPass),
        PassNode(ERenderPassNodeType::DecalPass),
        PassNode(ERenderPassNodeType::LightingPass)
    };
    Pipelines.emplace((int32)SceneViewMode.Type, SceneViewMode);

    FRenderPipelineDesc ScenePostProcess;
    ScenePostProcess.Type = ERenderPipelineType::ScenePostProcess;
    ScenePostProcess.Children = {
        PassNode(ERenderPassNodeType::HeightFogPass),
        PassNode(ERenderPassNodeType::FXAAPass)
    };
    Pipelines.emplace((int32)ScenePostProcess.Type, ScenePostProcess);

    FRenderPipelineDesc EditorOverlay;
    EditorOverlay.Type = ERenderPipelineType::EditorOverlay;
    EditorOverlay.Children = {
        PipelineNode(ERenderPipelineType::Outline),
        PassNode(ERenderPassNodeType::DebugLinesPass),
        PassNode(ERenderPassNodeType::GizmoRenderPass),
        PassNode(ERenderPassNodeType::OverlayFontRenderPass)
    };
    Pipelines.emplace((int32)EditorOverlay.Type, EditorOverlay);

    FRenderPipelineDesc Outline;
    Outline.Type = ERenderPipelineType::Outline;
    Outline.Children = {
        PassNode(ERenderPassNodeType::SelectionMaskPass),
        PassNode(ERenderPassNodeType::OutlinePass)
    };
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
