#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Pipelines/Registry/RenderPipelineRegistry.h"

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
} // namespace

void FRenderPipelineRegistry::Initialize()
{
    Release();

    // ==================== Default Scene Pipeline ====================
    FRenderPipelineDesc DefaultScene;
    DefaultScene.Type = ERenderPipelineType::DefaultScene;
    DefaultScene.Children = { PipelineNode(ERenderPipelineType::Scene) };
    Pipelines.emplace((int32)DefaultScene.Type, DefaultScene);

    // ==================== Editor Scene Pipeline =====================
    FRenderPipelineDesc EditorScene;
    EditorScene.Type = ERenderPipelineType::EditorScene;
    EditorScene.Children = {
        PipelineNode(ERenderPipelineType::Scene),
        PipelineNode(ERenderPipelineType::EditorOverlay)
    };
    Pipelines.emplace((int32)EditorScene.Type, EditorScene);

    // ======================= Scene Pipeline =========================
    FRenderPipelineDesc Scene;
    Scene.Type = ERenderPipelineType::Scene;
    Scene.Children = {
        PipelineNode(ERenderPipelineType::SceneViewMode),
        PassNode(ERenderPassNodeType::AdditiveDecalPass),
        PassNode(ERenderPassNodeType::AlphaBlendPass),
        PipelineNode(ERenderPipelineType::ScenePostProcess)
    };
    Pipelines.emplace((int32)Scene.Type, Scene);

    // ==================== Scene ViewMode Pipeline ===================
    FRenderPipelineDesc SceneViewMode;
    SceneViewMode.Type = ERenderPipelineType::SceneViewMode;
    SceneViewMode.Children = {
        PassNode(ERenderPassNodeType::DepthPrePass),
        PassNode(ERenderPassNodeType::BaseDrawPass),
        PassNode(ERenderPassNodeType::DecalPass),
        PassNode(ERenderPassNodeType::LightingPass)
    };
    Pipelines.emplace((int32)SceneViewMode.Type, SceneViewMode);

    // =================== Scene PostProcess Pipeline =================
    FRenderPipelineDesc ScenePostProcess;
    ScenePostProcess.Type = ERenderPipelineType::ScenePostProcess;
    ScenePostProcess.Children = {
        PassNode(ERenderPassNodeType::ViewModeResolvePass),
        PassNode(ERenderPassNodeType::HeightFogPass),
        PassNode(ERenderPassNodeType::FXAAPass)
    };
    Pipelines.emplace((int32)ScenePostProcess.Type, ScenePostProcess);

    // ==================== Editor Overlay Pipeline ===================
    FRenderPipelineDesc EditorOverlay;
    EditorOverlay.Type = ERenderPipelineType::EditorOverlay;
    EditorOverlay.Children = {
        PipelineNode(ERenderPipelineType::Outline),
        PassNode(ERenderPassNodeType::DebugLinePass),
        PassNode(ERenderPassNodeType::GizmoPass),
        PassNode(ERenderPassNodeType::OverlayTextPass)
    };
    Pipelines.emplace((int32)EditorOverlay.Type, EditorOverlay);

    // ====================== Outline Pipeline ========================
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