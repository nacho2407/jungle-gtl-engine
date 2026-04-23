#include "Render/Execute/Registry/RenderPipelineRegistry.h"

namespace
{
// ========== Node Helpers ==========

FRenderNodeRef PipelineNode(ERenderPipelineType Type)
{
    return { ERenderNodeKind::Pipeline, (int32)Type };
}

FRenderNodeRef PassNode(ERenderPassNodeType Type)
{
    return { ERenderNodeKind::Pass, (int32)Type };
}
} // namespace

// ========== Lifecycle ==========

void FRenderPipelineRegistry::Initialize()
{
    Release();

    // ---------- Roots ----------
    FRenderPipelineDesc DefaultRootPipeline = { ERenderPipelineType::DefaultRootPipeline, { PipelineNode(ERenderPipelineType::ScenePipeline) } };
    Pipelines.emplace((int32)DefaultRootPipeline.Type, DefaultRootPipeline);

    FRenderPipelineDesc EditorRootPipeline = { ERenderPipelineType::EditorRootPipeline, { PipelineNode(ERenderPipelineType::ScenePipeline), PipelineNode(ERenderPipelineType::OverlayPipeline), PipelineNode(ERenderPipelineType::PresentPipeline) } };
    Pipelines.emplace((int32)EditorRootPipeline.Type, EditorRootPipeline);

    // ---------- Scene View Modes ----------
    FRenderPipelineDesc Scene = { ERenderPipelineType::ScenePipeline, { PipelineNode(ERenderPipelineType::Lit), PipelineNode(ERenderPipelineType::Unlit), PipelineNode(ERenderPipelineType::WorldNormal), PipelineNode(ERenderPipelineType::SceneDepth), PipelineNode(ERenderPipelineType::PostProcessPipeline) } };
    Pipelines.emplace((int32)Scene.Type, Scene);

    FRenderPipelineDesc Lit = { ERenderPipelineType::Lit, { PassNode(ERenderPassNodeType::DepthPrePass), PassNode(ERenderPassNodeType::LightCullingPass), PassNode(ERenderPassNodeType::OpaquePass), PassNode(ERenderPassNodeType::DecalPass), PassNode(ERenderPassNodeType::LightingPass) } };
    Pipelines.emplace((int32)Lit.Type, Lit);

    FRenderPipelineDesc Unlit = { ERenderPipelineType::Unlit, { PassNode(ERenderPassNodeType::DepthPrePass), PassNode(ERenderPassNodeType::OpaquePass), PassNode(ERenderPassNodeType::DecalPass) } };
    Pipelines.emplace((int32)Unlit.Type, Unlit);

    FRenderPipelineDesc WorldNormal = { ERenderPipelineType::WorldNormal, { PassNode(ERenderPassNodeType::DepthPrePass), PassNode(ERenderPassNodeType::OpaquePass), PassNode(ERenderPassNodeType::DecalPass), PassNode(ERenderPassNodeType::NonLitViewModePass) } };
    Pipelines.emplace((int32)WorldNormal.Type, WorldNormal);

    FRenderPipelineDesc SceneDepth = { ERenderPipelineType::SceneDepth, { PassNode(ERenderPassNodeType::DepthPrePass), PassNode(ERenderPassNodeType::NonLitViewModePass) } };
    Pipelines.emplace((int32)SceneDepth.Type, SceneDepth);

    // ---------- Post, Overlay, Present ----------
    FRenderPipelineDesc PostProcessPipeline = { ERenderPipelineType::PostProcessPipeline, { PassNode(ERenderPassNodeType::HeightFogPass), PassNode(ERenderPassNodeType::FXAAPass)

                                                                                          } };
    Pipelines.emplace((int32)PostProcessPipeline.Type, PostProcessPipeline);

    FRenderPipelineDesc OverlayPipeline = { ERenderPipelineType::OverlayPipeline, { PassNode(ERenderPassNodeType::LightHitMapPass), PassNode(ERenderPassNodeType::DebugLinePass), PipelineNode(ERenderPipelineType::Outline), PassNode(ERenderPassNodeType::OverlayBillboardPass), PassNode(ERenderPassNodeType::GizmoPass), PassNode(ERenderPassNodeType::OverlayTextPass) } };
    Pipelines.emplace((int32)OverlayPipeline.Type, OverlayPipeline);

    FRenderPipelineDesc PresentPipeline = { ERenderPipelineType::PresentPipeline, { PassNode(ERenderPassNodeType::PresentPass) } };
    Pipelines.emplace((int32)PresentPipeline.Type, PresentPipeline);

    FRenderPipelineDesc Outline = { ERenderPipelineType::Outline, { PassNode(ERenderPassNodeType::SelectionMaskPass), PassNode(ERenderPassNodeType::OutlinePass) } };
    Pipelines.emplace((int32)Outline.Type, Outline);
}

void FRenderPipelineRegistry::Release()
{
    Pipelines.clear();
}

// ========== Lookup ==========

const FRenderPipelineDesc* FRenderPipelineRegistry::FindPipeline(ERenderPipelineType Type) const
{
    auto It = Pipelines.find((int32)Type);
    return It != Pipelines.end() ? &It->second : nullptr;
}
