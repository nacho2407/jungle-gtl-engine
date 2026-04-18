#include "RenderCollector.h"

#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Render/Culling/ConvexVolume.h"
#include "Render/Culling/GPUOcclusionCulling.h"
#include "Render/DebugDraw/DebugDrawQueue.h"
#include "Render/Pipeline/LODContext.h"
#include "Render/Pipeline/DrawCommandBuilder.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Render/Proxy/FScene.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Proxy/TextRenderSceneProxy.h"

#include <Collision/Octree.h>
#include <Collision/SpatialPartition.h>

void FRenderCollector::CollectWorld(UWorld* World, const FFrameContext& Frame, FDrawCommandBuilder& Builder)
{
	if (!World) return;

	FScene& Scene = World->GetScene();
	Scene.UpdateDirtyProxies();

	LastVisibleProxies.clear();
	{
		SCOPE_STAT_CAT("FrustumCulling", "3_Collect");
		const uint32 ExpectedCount = Scene.GetProxyCount()
			+ static_cast<uint32>(Scene.GetNeverCullProxies().size());
		if (LastVisibleProxies.capacity() < ExpectedCount)
		{
			LastVisibleProxies.reserve(ExpectedCount);
		}

		for (FPrimitiveSceneProxy* Proxy : Scene.GetNeverCullProxies())
		{
			if (Proxy)
			{
				LastVisibleProxies.push_back(Proxy);
			}
		}

		World->GetPartition().QueryFrustumAllProxies(Frame.FrustumVolume, LastVisibleProxies);
	}

	CollectVisibleProxies(LastVisibleProxies, Frame, Scene, Builder);
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
// UpdateProxyLOD — LOD 갱신 공통 헬퍼 (메인 루프 + Decal Receiver 중복 제거)
// ============================================================
static void UpdateProxyLOD(FPrimitiveSceneProxy* Proxy, const FLODUpdateContext& LODCtx)
{
	if (!LODCtx.bValid || !LODCtx.ShouldRefreshLOD(Proxy->ProxyId, Proxy->LastLODUpdateFrame))
		return;

	const FVector& Pos = Proxy->CachedWorldPos;
	const float dx = LODCtx.CameraPos.X - Pos.X;
	const float dy = LODCtx.CameraPos.Y - Pos.Y;
	const float dz = LODCtx.CameraPos.Z - Pos.Z;
	Proxy->UpdateLOD(SelectLOD(Proxy->CurrentLOD, dx * dx + dy * dy + dz * dz));
	Proxy->LastLODUpdateFrame = LODCtx.LODUpdateFrame;
}

// ============================================================
// CollectFontProxy — Font 배칭 경로
// ============================================================
void FRenderCollector::CollectFontProxy(const FPrimitiveSceneProxy* Proxy, const FFrameContext& Frame, FDrawCommandBuilder& Builder)
{
	const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
	if (!TextProxy->CachedText.empty())
	{
		Builder.AddWorldText(TextProxy, Frame);
	}
}

// ============================================================
// CollectDecalProxy — Decal → Receiver 순회 + 커맨드 생성
// ============================================================
void FRenderCollector::CollectDecalProxy(FPrimitiveSceneProxy* Proxy, const FFrameContext& Frame,
	const TSet<FPrimitiveSceneProxy*>& VisibleSet, FDrawCommandBuilder& Builder)
{
	UDecalComponent* DecalComponent = static_cast<UDecalComponent*>(Proxy->Owner);
	FDecalSceneProxy* DecalProxy = static_cast<FDecalSceneProxy*>(Proxy);

	for (UStaticMeshComponent* Receiver : DecalComponent->GetReceivers())
	{
		if (!Receiver) continue;

		FPrimitiveSceneProxy* ReceiverProxy = Receiver->GetSceneProxy();
		if (!ReceiverProxy || VisibleSet.find(ReceiverProxy) == VisibleSet.end())
			continue;

		UpdateProxyLOD(ReceiverProxy, Frame.LODContext);

		if (ReceiverProxy->bPerViewportUpdate)
			ReceiverProxy->UpdatePerViewport(Frame);

		Builder.BuildDecalCommandForReceiver(*ReceiverProxy, *DecalProxy);
	}
}

// ============================================================
// CollectMeshProxy — 일반 메시 (PreDepth + 메인 패스)
// ============================================================
void FRenderCollector::CollectMeshProxy(const FPrimitiveSceneProxy* Proxy, FDrawCommandBuilder& Builder)
{
	if (Proxy->Pass == ERenderPass::Opaque)
		Builder.BuildCommandForProxy(*Proxy, ERenderPass::PreDepth);

	Builder.BuildCommandForProxy(*Proxy, Proxy->Pass);
}

// ============================================================
// CollectSelectionVisuals — 아웃라인 + AABB + 컴포넌트 디버그
// ============================================================
void FRenderCollector::CollectSelectionVisuals(FPrimitiveSceneProxy* Proxy, bool bShowBoundingVolume,
	FScene& Scene, FDrawCommandBuilder& Builder)
{
	if (Proxy->bSupportsOutline)
		Builder.BuildCommandForProxy(*Proxy, ERenderPass::SelectionMask);

	if (bShowBoundingVolume && Proxy->bShowAABB)
		Scene.AddDebugAABB(Proxy->CachedBounds.Min, Proxy->CachedBounds.Max, FColor::White());

	//TODO: Owner 의존성 제거
	Proxy->CollectSelectedVisuals(Scene);
}

// ============================================================
// Visible 프록시 수집 — 오케스트레이터
// ============================================================
void FRenderCollector::CollectVisibleProxies(const TArray<FPrimitiveSceneProxy*>& Proxies, const FFrameContext& Frame, FScene& Scene, FDrawCommandBuilder& Builder)
{
	if (!Frame.RenderOptions.ShowFlags.bPrimitives) return;

	const bool bShowBoundingVolume = Frame.RenderOptions.ShowFlags.bBoundingVolume;
	SCOPE_STAT_CAT("CollectVisibleProxy", "3_Collect");

	TSet<FPrimitiveSceneProxy*> VisibleProxySet;
	VisibleProxySet.reserve(Proxies.size());
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (Proxy)
			VisibleProxySet.insert(Proxy);
	}

	const FGPUOcclusionCulling* Occlusion = Frame.OcclusionCulling;
	FGPUOcclusionCulling* OcclusionMut = Frame.OcclusionCulling;

	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->BeginGatherAABB(static_cast<uint32>(Proxies.size()));

	LOD_STATS_RESET();

	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		UpdateProxyLOD(Proxy, Frame.LODContext);
		LOD_STATS_RECORD(Proxy->CurrentLOD);

		if (Proxy->bPerViewportUpdate)
			Proxy->UpdatePerViewport(Frame);

		if (!Proxy->bVisible)
			continue;

		if (OcclusionMut)
			OcclusionMut->GatherAABB(Proxy);

		if (Occlusion && !Proxy->bNeverCull && Occlusion->IsOccluded(Proxy))
			continue;

		// 프록시 타입별 분기
		if (Proxy->bFontBatched)
			CollectFontProxy(Proxy, Frame, Builder);
		else if (Cast<UDecalComponent>(Proxy->Owner))
			CollectDecalProxy(Proxy, Frame, VisibleProxySet, Builder);
		else
			CollectMeshProxy(Proxy, Builder);

		// 선택된 오브젝트 시각화
		if (Proxy->bSelected)
			CollectSelectionVisuals(Proxy, bShowBoundingVolume, Scene, Builder);
	}

	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->EndGatherAABB();
}
