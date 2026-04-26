#include "RenderCollector.h"

#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"
#include "Editor/EditorEngine.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Collision/ConvexVolume.h"
#include "Render/Culling/GPUOcclusionCulling.h"
#include "Debug/DebugDrawQueue.h"
#include "Render/Types/LODContext.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Scene/FScene.h"

#include <Collision/Octree.h>
#include <Collision/SpatialPartition.h>

// ============================================================
// UpdateProxyLOD — LOD 갱신 공통 헬퍼 (Collector + Builder 공유)
// ============================================================
void UpdateProxyLOD(FPrimitiveSceneProxy* Proxy, const FLODUpdateContext& LODCtx)
{
	if (!LODCtx.bValid || !LODCtx.ShouldRefreshLOD(Proxy->GetProxyId(), Proxy->GetLastLODUpdateFrame()))
		return;

	const FVector& Pos = Proxy->GetCachedWorldPos();
	const float dx = LODCtx.CameraPos.X - Pos.X;
	const float dy = LODCtx.CameraPos.Y - Pos.Y;
	const float dz = LODCtx.CameraPos.Z - Pos.Z;
	Proxy->UpdateLOD(SelectLOD(Proxy->GetCurrentLOD(), dx * dx + dy * dy + dz * dz));
	Proxy->SetLastLODUpdateFrame(LODCtx.LODUpdateFrame);
}

void FRenderCollector::Collect(UWorld* World, const FFrameContext& Frame, FCollectOutput& Output)
{
	if (!World) return;

	FScene& Scene = World->GetScene();
	Scene.UpdateDirtyProxies();

	Output.FrustumVisibleProxies.clear();
	{
		SCOPE_STAT_CAT("FrustumCulling", "3_Collect");
		const uint32 ExpectedCount = Scene.GetProxyCount()
			+ static_cast<uint32>(Scene.GetNeverCullProxies().size());
		if (Output.FrustumVisibleProxies.capacity() < ExpectedCount)
		{
			Output.FrustumVisibleProxies.reserve(ExpectedCount);
		}

		for (FPrimitiveSceneProxy* Proxy : Scene.GetNeverCullProxies())
		{
			if (Proxy)
			{
				Output.FrustumVisibleProxies.push_back(Proxy);
			}
		}

		World->GetPartition().QueryFrustumAllProxies(Frame.FrustumVolume, Output.FrustumVisibleProxies);
	}

	FilterVisibleProxies(Frame, Scene, Output);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene)
{
	Scene.SetGrid(GridSpacing, GridHalfLineCount);
}

void FRenderCollector::CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene)
{
	TArray<FOverlayStatLine> Lines;
	OverlaySystem.BuildLines(Editor, Lines);
	const float TextScale = OverlaySystem.GetLayout().TextScale;

	for (FOverlayStatLine& Line : Lines)
	{
		Scene.AddOverlayText(std::move(Line.Text), Line.ScreenPosition, TextScale);
	}
}

void FRenderCollector::CollectDebugDraw(const FFrameContext& Frame, FScene& Scene)
{
	if (!Frame.RenderOptions.ShowFlags.bDebugDraw) return;

	for (const FDebugDrawItem& Item : Scene.GetDebugDrawQueue().GetItems())
	{
		Scene.AddDebugLine(Item.Start, Item.End, Item.Color);
	}
}

// ============================================================
// Octree 디버그 시각화 — 깊이별 색상으로 노드 AABB 표시
// ============================================================
static const FColor OctreeDepthColors[] = {
	FColor(255,   0,   0),	// 0: Red
	FColor(255, 165,   0),	// 1: Orange
	FColor(255, 255,   0),	// 2: Yellow
	FColor(0, 255,   0),	// 3: Green
	FColor(0, 255, 255),	// 4: Cyan
	FColor(0,   0, 255),	// 5: Blue
};

void FRenderCollector::CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth)
{
	if (!Node) return;

	const FBoundingBox& Bounds = Node->GetCellBounds();
	if (!Bounds.IsValid()) return;

	Scene.AddDebugAABB(Bounds.Min, Bounds.Max, OctreeDepthColors[Depth % 6]);

	for (const FOctree* Child : Node->GetChildren())
	{
		CollectOctreeDebug(Child, Scene, Depth + 1);
	}
}

// ============================================================
// FilterVisibleProxies — visibility/occlusion 필터 → RenderableProxies
// ============================================================
void FRenderCollector::FilterVisibleProxies(const FFrameContext& Frame, FScene& Scene, FCollectOutput& Output)
{
	if (!Frame.RenderOptions.ShowFlags.bPrimitives) return;

	SCOPE_STAT_CAT("CollectVisibleProxy", "3_Collect");

	Output.VisibleProxySet.reserve(Output.FrustumVisibleProxies.size());
	for (FPrimitiveSceneProxy* Proxy : Output.FrustumVisibleProxies)
	{
		if (Proxy)
			Output.VisibleProxySet.insert(Proxy);
	}

	const FGPUOcclusionCulling* Occlusion = Frame.OcclusionCulling;
	FGPUOcclusionCulling* OcclusionMut = Frame.OcclusionCulling;

	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->BeginGatherAABB(static_cast<uint32>(Output.FrustumVisibleProxies.size()));

	LOD_STATS_RESET();

	Output.RenderableProxies.reserve(Output.FrustumVisibleProxies.size());

	for (FPrimitiveSceneProxy* Proxy : Output.FrustumVisibleProxies)
	{
		// Light View에서는 EditorOnly 프록시(빌보드 아이콘 등) 제외
		if (Frame.bIsLightView && Proxy->HasProxyFlag(EPrimitiveProxyFlags::EditorOnly))
			continue;

		UpdateProxyLOD(Proxy, Frame.LODContext);
		LOD_STATS_RECORD(Proxy->GetCurrentLOD());

		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::PerViewportUpdate))
			Proxy->UpdatePerViewport(Frame);

		if (!Proxy->IsVisible())
			continue;

		if (OcclusionMut)
			OcclusionMut->GatherAABB(Proxy);

		if (Occlusion && !Proxy->HasProxyFlag(EPrimitiveProxyFlags::NeverCull) && Occlusion->IsOccluded(Proxy))
			continue;

		Output.RenderableProxies.push_back(Proxy);
	}

	// 선택된 Actor의 컴포넌트 디버그 시각화 (빛 등 프록시 없는 Comp 포함)
	CollectSelectedActorVisuals(Scene);

	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->EndGatherAABB();
}

// ============================================================
// CollectSelectedActorVisuals — Actor 단위 디버그 시각화 (빛 등 프록시 없는 Comp 포함)
// ============================================================
void FRenderCollector::CollectSelectedActorVisuals(FScene& Scene)
{
	for (AActor* Actor : Scene.GetSelectedActors())
	{
		if (!Actor) continue;
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp)
				Comp->ContributeSelectedVisuals(Scene);
		}
	}
}
