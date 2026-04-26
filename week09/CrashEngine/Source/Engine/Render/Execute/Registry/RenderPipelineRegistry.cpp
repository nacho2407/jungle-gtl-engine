// 렌더 영역의 세부 동작을 구현합니다.
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

    auto AddPipeline =
        [this](ERenderPipelineType Type, std::initializer_list<FRenderNodeRef> Nodes)
    {
        FRenderPipelineDesc Desc = {
            Type,
            { Nodes.begin(),
              Nodes.end() }
        };

        Pipelines.emplace((int32)Desc.Type, Desc);
    };

    // ---------- Roots ----------
    AddPipeline(
        ERenderPipelineType::DefaultRootPipeline,
        {
            PipelineNode(ERenderPipelineType::ScenePipeline),
        });

    AddPipeline(
        ERenderPipelineType::EditorRootPipeline,
        {
            PipelineNode(ERenderPipelineType::ScenePipeline),
            PipelineNode(ERenderPipelineType::OverlayPipeline),
        });

    // ---------- Scene View Modes ----------
    AddPipeline(
        ERenderPipelineType::ScenePipeline,
        {
            PipelineNode(ERenderPipelineType::DeferredPipeline),
            PipelineNode(ERenderPipelineType::ForwardPipeline),
            PipelineNode(ERenderPipelineType::PostProcessPipeline),
        });

    AddPipeline(
        ERenderPipelineType::DeferredPipeline,
        {
            PipelineNode(ERenderPipelineType::DeferredLitPipeline),
            PipelineNode(ERenderPipelineType::DeferredUnlitPipeline),
            PipelineNode(ERenderPipelineType::DeferredWorldNormalPipeline),
            PipelineNode(ERenderPipelineType::DeferredSceneDepthPipeline),
        });

    AddPipeline(
        ERenderPipelineType::DeferredLitPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::ShadowMapPass),
            PassNode(ERenderPassNodeType::LightCullingPass),
            PassNode(ERenderPassNodeType::DeferredOpaquePass),
            PassNode(ERenderPassNodeType::DeferredDecalPass),
            PassNode(ERenderPassNodeType::DeferredLightingPass),
        });

    AddPipeline(
        ERenderPipelineType::DeferredUnlitPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::DeferredOpaquePass),
            PassNode(ERenderPassNodeType::DeferredDecalPass),
        });

    AddPipeline(
        ERenderPipelineType::DeferredWorldNormalPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::DeferredOpaquePass),
            PassNode(ERenderPassNodeType::DeferredDecalPass),
            PassNode(ERenderPassNodeType::NonLitViewModePass),
        });

    AddPipeline(
        ERenderPipelineType::DeferredSceneDepthPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::NonLitViewModePass),
        });

    AddPipeline(
        ERenderPipelineType::ForwardPipeline,
        {
            PipelineNode(ERenderPipelineType::ForwardLitPipeline),
            PipelineNode(ERenderPipelineType::ForwardUnlitPipeline),
            PipelineNode(ERenderPipelineType::ForwardWorldNormalPipeline),
            PipelineNode(ERenderPipelineType::ForwardSceneDepthPipeline),
        });

    AddPipeline(
        ERenderPipelineType::ForwardLitPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::ShadowMapPass),
            PassNode(ERenderPassNodeType::ForwardOpaquePass),
        });

    AddPipeline(
        ERenderPipelineType::ForwardUnlitPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::ForwardOpaquePass),
        });

    AddPipeline(
        ERenderPipelineType::ForwardWorldNormalPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::ForwardOpaquePass),
        });

    AddPipeline(
        ERenderPipelineType::ForwardSceneDepthPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::NonLitViewModePass),
        });

    // ---------- Post, Overlay, Present ----------
    AddPipeline(
        ERenderPipelineType::PostProcessPipeline,
        {
            PassNode(ERenderPassNodeType::HeightFogPass),
            PassNode(ERenderPassNodeType::FXAAPass),
        });

    AddPipeline(
        ERenderPipelineType::OverlayPipeline,
        {
            PassNode(ERenderPassNodeType::LightHitMapPass),
            PassNode(ERenderPassNodeType::DebugLinePass),
            PipelineNode(ERenderPipelineType::OutlinePipeline),
            PassNode(ERenderPassNodeType::OverlayBillboardPass),
            PassNode(ERenderPassNodeType::GizmoPass),
            PassNode(ERenderPassNodeType::OverlayTextPass),
        });

    AddPipeline(
        ERenderPipelineType::PresentPipeline,
        {
            PassNode(ERenderPassNodeType::PresentPass),
        });

    AddPipeline(
        ERenderPipelineType::OutlinePipeline,
        {
            PassNode(ERenderPassNodeType::SelectionMaskPass),
            PassNode(ERenderPassNodeType::OutlinePass),
        });
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
