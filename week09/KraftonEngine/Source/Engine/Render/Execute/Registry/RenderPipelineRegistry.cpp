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
            PipelineNode(ERenderPipelineType::LitPipeline),
            PipelineNode(ERenderPipelineType::UnlitPipeline),
            PipelineNode(ERenderPipelineType::WorldNormalPipeline),
            PipelineNode(ERenderPipelineType::SceneDepthPipeline),
            PipelineNode(ERenderPipelineType::PostProcessPipeline),
        });

    AddPipeline(
        ERenderPipelineType::LitPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::LightCullingPass),
            PassNode(ERenderPassNodeType::OpaquePass),
            PassNode(ERenderPassNodeType::DecalPass),
            PassNode(ERenderPassNodeType::LightingPass),
        });

    AddPipeline(
        ERenderPipelineType::UnlitPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::OpaquePass),
            PassNode(ERenderPassNodeType::DecalPass),
        });

    AddPipeline(
        ERenderPipelineType::WorldNormalPipeline,
        {
            PassNode(ERenderPassNodeType::DepthPrePass),
            PassNode(ERenderPassNodeType::OpaquePass),
            PassNode(ERenderPassNodeType::DecalPass),
            PassNode(ERenderPassNodeType::NonLitViewModePass),
        });

    AddPipeline(
        ERenderPipelineType::SceneDepthPipeline,
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
            PipelineNode(ERenderPipelineType::Outline),
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
        ERenderPipelineType::Outline,
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
