#include "RenderCollector.h"

#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Render/Visibility/ConvexVolume.h"
#include "Render/Visibility/GPUOcclusionCulling.h"
#include "Render/DebugDraw/DebugDrawQueue.h"
#include "Render/Execution/LODContext.h"
#include "Render/Execution/Renderer.h"
#include "Render/Core/PassTypes.h"
#include "Render/Builders/MeshDrawCommandBuilder.h"
#include "Render/Builders/TextDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/DecalSceneProxy.h"
#include "Render/Scene/Core/Scene.h"
#include "Render/Scene/Proxies/Light/LightSceneProxy.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Render/Passes/Common/RenderPassContext.h"

#include <Collision/Octree.h>
#include <Collision/SpatialPartition.h>
#include <Collision/WorldPrimitivePickingBVH.h>

static ERenderPassNodeType MapPassToNodeType(ERenderPass Pass)
{
    switch (Pass)
    {
    case ERenderPass::DepthPre:
        return ERenderPassNodeType::DepthPrePass;
    case ERenderPass::Opaque:
        return ERenderPassNodeType::BaseDrawPass;
    case ERenderPass::Decal:
        return ERenderPassNodeType::DecalPass;
    case ERenderPass::Lighting:
        return ERenderPassNodeType::LightingPass;
    case ERenderPass::AdditiveDecal:
        return ERenderPassNodeType::AdditiveDecalPass;
    case ERenderPass::AlphaBlend:
        return ERenderPassNodeType::AlphaBlendPass;
    case ERenderPass::SelectionMask:
        return ERenderPassNodeType::SelectionMaskPass;
    case ERenderPass::EditorLines:
        return ERenderPassNodeType::DebugLinePass;
    case ERenderPass::FXAA:
        return ERenderPassNodeType::FXAAPass;
    case ERenderPass::GizmoOuter:
    case ERenderPass::GizmoInner:
        return ERenderPassNodeType::GizmoPass;
    case ERenderPass::OverlayFont:
        return ERenderPassNodeType::OverlayTextPass;
    case ERenderPass::PostProcess:
        return ERenderPassNodeType::HeightFogPass;
    default:
        return ERenderPassNodeType::BaseDrawPass;
    }
}

void FRenderCollector::CollectWorld(UWorld* World, const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer)
{
    if (!World)
        return;
    Scene.UpdateDirtyProxies();
    Scene.UpdateDirtyLightProxies();

    // Visible Primitive Proxises 수집 — 프러스텀 + Occlusion Culling
    CollectedPrimitives.VisibleProxies.clear();
    CollectedPrimitives.OpaqueProxies.clear();
    CollectedPrimitives.TransparentProxies.clear();
    {
        SCOPE_STAT_CAT("FrustumCulling", "3_Collect");
        const uint32 ExpectedCount = Scene.GetPrimitiveProxyCount() + static_cast<uint32>(Scene.GetNeverCullProxies().size());
        if (CollectedPrimitives.VisibleProxies.capacity() < ExpectedCount)
        {
            CollectedPrimitives.VisibleProxies.reserve(ExpectedCount);
        }

        for (FPrimitiveSceneProxy* Proxy : Scene.GetNeverCullProxies())
        {
            if (Proxy)
            {
                CollectedPrimitives.VisibleProxies.push_back(Proxy);
            }
        }

        World->GetPartition().QueryFrustumAllProxies(Frame.FrustumVolume, CollectedPrimitives.VisibleProxies);
    }

    Renderer.SetCollectedScene(&Scene);

    // Visible Primitive Proxies → pass-specific draw command build
    CollectPrimitives(CollectedPrimitives.VisibleProxies, Frame, Scene, Renderer);

    // Light Proxy → FLightConstants 배열로 수집 (드로우콜 불필요, CB 데이터만 추출)
    CollectLights(Scene, CollectedLights);
    Renderer.SetCollectedLights(&CollectedLights);
}

void FRenderCollector::BuildFramePassCommands(const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer)
{
    Renderer.SetCollectedScene(&Scene);
    FRenderPassContext PassContext = Renderer.CreatePassContext(Frame, nullptr, &Scene, &CollectedPrimitives.VisibleProxies);

    if (Renderer.HasActiveViewModePassConfig())
    {
        const FViewModePassRegistry* ViewModeRegistry = Renderer.GetViewModePassRegistry();
        if (ViewModeRegistry && ViewModeRegistry->UsesLightingPass(Frame.ViewMode))
        {
            if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::LightingPass))
                Pass->BuildDrawCommands(PassContext);
        }
    }

    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::ViewModePostProcessPass))
        Pass->BuildDrawCommands(PassContext);
    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::HeightFogPass))
        Pass->BuildDrawCommands(PassContext);
    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::OutlinePass))
        Pass->BuildDrawCommands(PassContext);
    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::DebugLinePass))
        Pass->BuildDrawCommands(PassContext);
    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::OverlayTextPass))
        Pass->BuildDrawCommands(PassContext);
    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::FXAAPass))
        Pass->BuildDrawCommands(PassContext);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene)
{
    Scene.SetGrid(GridSpacing, GridHalfLineCount);
}

void FRenderCollector::CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene)
{
    TArray<FOverlayStatLine> Lines;
    OverlaySystem.BuildLines(Editor, Lines);

    const float Scale = OverlaySystem.GetLayout().TextScale;
    for (const FOverlayStatLine& Line : Lines)
    {
        if (!Line.Text.empty())
        {
            Scene.AddOverlayText(Line.Text, Line.ScreenPosition, Scale);
        }
    }
}

void FRenderCollector::CollectDebugDraw(const FFrameContext& Frame, FScene& Scene)
{
    if (!Frame.ShowFlags.bDebugDraw)
        return;

    for (const FDebugDrawItem& Item : Scene.GetDebugDrawQueue().GetItems())
    {
        Scene.AddDebugLine(Item.Start, Item.End, Item.Color);
    }
}

// ============================================================
// Octree 디버그 시각화 — 깊이별 색상으로 노드 AABB 표시
// ============================================================
static const FColor OctreeDebugColor = FColor(0, 255, 255);
static const FColor BVHDebugColor = FColor(0, 255, 0);
static const FColor WorldBoundDebugColor = FColor(255, 0, 255);

void FRenderCollector::CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth)
{
    if (!Node)
        return;

    const FBoundingBox& Bounds = Node->GetCellBounds();
    if (!Bounds.IsValid())
        return;

    const FColor& Color = OctreeDebugColor;
    const FVector& Min = Bounds.Min;
    const FVector& Max = Bounds.Max;

    // 8개 꼭짓점
    FVector V[8] = {
        FVector(Min.X, Min.Y, Min.Z), // 0
        FVector(Max.X, Min.Y, Min.Z), // 1
        FVector(Max.X, Max.Y, Min.Z), // 2
        FVector(Min.X, Max.Y, Min.Z), // 3
        FVector(Min.X, Min.Y, Max.Z), // 4
        FVector(Max.X, Min.Y, Max.Z), // 5
        FVector(Max.X, Max.Y, Max.Z), // 6
        FVector(Min.X, Max.Y, Max.Z), // 7
    };

    // 12에지
    static constexpr int32 Edges[][2] = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };

    for (const auto& E : Edges)
    {
        Scene.AddDebugLine(V[E[0]], V[E[1]], Color);
    }

    // 자식 노드 재귀
    for (const FOctree* Child : Node->GetChildren())
    {
        CollectOctreeDebug(Child, Scene, Depth + 1);
    }
}


void FRenderCollector::CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH, FScene& Scene)
{
    const TArray<FWorldPrimitivePickingBVH::FNode>& Nodes = BVH.GetNodes();
    for (const FWorldPrimitivePickingBVH::FNode& Node : Nodes)
    {
        if (!Node.Bounds.IsValid())
        {
            continue;
        }
        Scene.AddDebugAABB(Node.Bounds.Min, Node.Bounds.Max, BVHDebugColor);
    }
}

void FRenderCollector::CollectWorldBoundsDebug(const TArray<FPrimitiveSceneProxy*>& Proxies, FScene& Scene)
{
    for (FPrimitiveSceneProxy* Proxy : Proxies)
    {
        if (!Proxy || !Proxy->CachedBounds.IsValid())
        {
            continue;
        }
        Scene.AddDebugAABB(Proxy->CachedBounds.Min, Proxy->CachedBounds.Max, WorldBoundDebugColor);
    }
}

// ============================================================
// Visible 프록시 수집 — Proxy → FDrawCommand 직접 변환
// ============================================================
void FRenderCollector::CollectPrimitives(const TArray<FPrimitiveSceneProxy*>& Proxies, const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer)
{
    if (!Frame.ShowFlags.bPrimitives)
        return;

    SCOPE_STAT_CAT("CollectVisibleProxy", "3_Collect");

    // 입력 배열과 출력 배열이 같은 객체일 수 있으므로 먼저 복사해둔다.
    const TArray<FPrimitiveSceneProxy*> CandidateProxies = Proxies;

    CollectedPrimitives.VisibleProxies.clear();
    CollectedPrimitives.OpaqueProxies.clear();
    CollectedPrimitives.TransparentProxies.clear();

    FRenderPassContext PassContext = Renderer.CreatePassContext(Frame, nullptr, &Scene, &CollectedPrimitives.VisibleProxies);
    // Pass-specific command building now happens during pipeline execution.
    const FViewModePassRegistry* ViewModeRegistry = Renderer.GetViewModePassRegistry();
    const bool bHasViewModeConfig = Renderer.HasActiveViewModePassConfig() && ViewModeRegistry && ViewModeRegistry->HasConfig(Frame.ViewMode);
    const bool bUsesBaseDraw = bHasViewModeConfig && ViewModeRegistry->UsesBaseDraw(Frame.ViewMode);
    const bool bUsesDecal = bHasViewModeConfig && ViewModeRegistry->UsesDecal(Frame.ViewMode);
    const bool bSuppressSceneExtras = bHasViewModeConfig && ViewModeRegistry->SuppressesSceneExtras(Frame.ViewMode);

    TSet<FPrimitiveSceneProxy*> VisibleProxySet;
    VisibleProxySet.reserve(CandidateProxies.size());
    for (FPrimitiveSceneProxy* Proxy : CandidateProxies)
    {
        if (Proxy)
            VisibleProxySet.insert(Proxy);
    }

    const FGPUOcclusionCulling* Occlusion = Frame.OcclusionCulling;
    FGPUOcclusionCulling* OcclusionMut = Frame.OcclusionCulling;
    const FLODUpdateContext& LODCtx = Frame.LODContext;

    if (OcclusionMut && OcclusionMut->IsInitialized())
    {
        OcclusionMut->BeginGatherAABB(static_cast<uint32>(CandidateProxies.size()));
    }

    LOD_STATS_RESET();

    for (FPrimitiveSceneProxy* Proxy : CandidateProxies)
    {
        if (LODCtx.bValid && LODCtx.ShouldRefreshLOD(Proxy->ProxyId, Proxy->LastLODUpdateFrame))
        {
            const FVector& ProxyPos = Proxy->CachedWorldPos;
            const float dx = LODCtx.CameraPos.X - ProxyPos.X;
            const float dy = LODCtx.CameraPos.Y - ProxyPos.Y;
            const float dz = LODCtx.CameraPos.Z - ProxyPos.Z;
            const float DistSq = dx * dx + dy * dy + dz * dz;
            Proxy->UpdateLOD(SelectLOD(Proxy->CurrentLOD, DistSq));
            Proxy->LastLODUpdateFrame = LODCtx.LODUpdateFrame;
        }

        LOD_STATS_RECORD(Proxy->CurrentLOD);

        if (Proxy->bPerViewportUpdate)
        {
            Proxy->UpdatePerViewport(Frame);
        }

        // if (Proxy->Pass == ERenderPass::GizmoOuter || Proxy->Pass == ERenderPass::GizmoInner)
        //{
        //     CollectorDebugLog("[CollectPrimitives] Gizmo candidate pass=%d visible=%d mesh=%p shader=%p owner=%p",
        //         static_cast<int>(Proxy->Pass), Proxy->bVisible ? 1 : 0, Proxy->MeshBuffer, Proxy->Shader, Proxy->Owner);
        // }

        if (!Proxy->bVisible)
            continue;

        if (OcclusionMut)
            OcclusionMut->GatherAABB(Proxy);

        if (Occlusion && !Proxy->bNeverCull && Occlusion->IsOccluded(Proxy))
            continue;

        CollectedPrimitives.VisibleProxies.push_back(Proxy);

        if (Proxy->Blend == EBlendState::AlphaBlend)
            CollectedPrimitives.TransparentProxies.push_back(Proxy);
        else
            CollectedPrimitives.OpaqueProxies.push_back(Proxy);

        if (Proxy->bFontBatched)
        {
            const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
            if (!TextProxy->CachedText.empty())
            {
                FTextDrawCommandBuilder::BuildWorld(*TextProxy, PassContext, *PassContext.DrawCommandList);
            }
        }
        else if (Cast<UDecalComponent>(Proxy->Owner))
        {
            FDecalSceneProxy* DecalProxy = static_cast<FDecalSceneProxy*>(Proxy);

            if (bHasViewModeConfig)
            {
                if (bUsesDecal)
                {
                    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::DecalPass))
                    {
                        Pass->BuildDrawCommands(PassContext, *DecalProxy);
                    }
                }
            }
            else
            {
                UDecalComponent* DecalComponent = static_cast<UDecalComponent*>(Proxy->Owner);
                for (UStaticMeshComponent* Receiver : DecalComponent->GetReceivers())
                {
                    if (!Receiver)
                        continue;

                    FPrimitiveSceneProxy* ReceiverProxy = Receiver->GetSceneProxy();
                    if (!ReceiverProxy || VisibleProxySet.find(ReceiverProxy) == VisibleProxySet.end())
                        continue;

                    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::DecalPass))
                    {
                        Pass->BuildDrawCommands(PassContext, *ReceiverProxy);
                    }
                }
            }
        }
        else
        {
            if (bHasViewModeConfig && Proxy->Pass == ERenderPass::Opaque &&
                Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::DepthPrePass))
            {
                FMeshDrawCommandBuilder::Build(*Proxy, ERenderPass::DepthPre, PassContext, *PassContext.DrawCommandList);
            }

            if (bHasViewModeConfig)
            {
                if (Proxy->Pass == ERenderPass::Opaque && !bUsesBaseDraw)
                {
                    continue;
                }

                if (bSuppressSceneExtras &&
                    (Proxy->Pass == ERenderPass::AdditiveDecal || Proxy->Pass == ERenderPass::AlphaBlend))
                {
                    continue;
                }
            }

            if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(MapPassToNodeType(Proxy->Pass)))
            {
                Pass->BuildDrawCommands(PassContext, *Proxy);
            }
        }

        if (Proxy->bSelected && Proxy->bSupportsOutline)
        {
            if (FRenderPass* SelectionMaskPass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::SelectionMaskPass))
            {
                SelectionMaskPass->BuildDrawCommands(PassContext, *Proxy);
            }
        }
    }
}

// ============================================================
// CollectLights — Light 프록시 → FLightConstants 배열 수집
// Light는 드로우콜이 없으므로 Proxy가 아닌 GPU 상수값만 추출해 저장한다.
// 순회·필터링은 RenderCollector가 직접 담당한다.
// ============================================================
void FRenderCollector::CollectLights(FScene& Scene, FCollectedLights& OutLights)
{
    const TArray<FLightSceneProxy*>& LightProxies = Scene.GetLightProxies();
    OutLights.GlobalLights = FGlobalLightConstants();
    OutLights.LocalLights.clear();

    for (FLightSceneProxy* Proxy : LightProxies)
    {
        if (!Proxy)
            continue;

        FLightConstants& LC = Proxy->LightConstants;
        if (LC.LightType == static_cast<uint32>(ELightType::Ambient))
        {
            OutLights.GlobalLights.Ambient.Color = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
            OutLights.GlobalLights.Ambient.Intensity = LC.Intensity;
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Directional))
        {
            if (OutLights.GlobalLights.NumDirectionalLights < MAX_DIRECTIONAL_LIGHTS)
            {
                uint32 Index = OutLights.GlobalLights.NumDirectionalLights;
                OutLights.GlobalLights.Directional[Index].Color = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
                OutLights.GlobalLights.Directional[Index].Intensity = LC.Intensity;
                OutLights.GlobalLights.Directional[Index].Direction = LC.Direction;
                OutLights.GlobalLights.NumDirectionalLights++;
            }
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Point) || LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            // 제한 없이 배열에 추가 — GPU 상수 배열로 전달, 셰이더에서 최대 개수만큼 처리
            FLocalLightInfo LocalLight = {};
            LocalLight.Color = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
            LocalLight.Intensity = LC.Intensity;
            LocalLight.Position = LC.Position;
            LocalLight.AttenuationRadius = LC.AttenuationRadius;
            LocalLight.Direction = LC.Direction;
            LocalLight.InnerConeAngle = LC.InnerConeAngle;
            LocalLight.OuterConeAngle = LC.OuterConeAngle;
            OutLights.LocalLights.push_back(LocalLight);
        }

        Proxy->VisualizeLightsInEditor(Scene);
    }

    // Local Lights의 개수를 저장
    OutLights.GlobalLights.NumLocalLights = static_cast<int32>(OutLights.LocalLights.size());
}
