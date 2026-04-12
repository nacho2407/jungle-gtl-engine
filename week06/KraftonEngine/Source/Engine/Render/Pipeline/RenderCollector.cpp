#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/EditorEngine.h"
#include "Render/Proxy/FScene.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/DebugDraw/DebugDrawQueue.h"
#include "Render/Culling/GPUOcclusionCulling.h"
#include "Render/Pipeline/LODContext.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Collision/IntersectionUtils.h"
#include "Profiling/Stats.h"
#include <Collision/Octree.h>

namespace
{
	FBoundingBox BuildDecalWorldAABB(const UDecalComponent& DecalComponent)
	{
		FVector Corners[8] = {};
		DecalComponent.GetWorldCorners(Corners);

		FBoundingBox Bounds;
		for (const FVector& Corner : Corners)
		{
			Bounds.Expand(Corner);
		}
		return Bounds;
	}
}

void FRenderCollector::CollectWorld(UWorld* World, FRenderBus& RenderBus)
{
	if (!World) return;

	// Dirty 프록시 갱신 후 visible 리스트만 순회
	World->GetScene().UpdateDirtyProxies();
	const TArray<FPrimitiveSceneProxy*>& VisibleProxies = World->GetVisibleProxies();
	CollectVisibleProxies(VisibleProxies, RenderBus);
	CollectDecals(World, VisibleProxies, RenderBus);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus)
{
	FGridEntry Entry = {};
	Entry.Grid.GridSpacing = GridSpacing;
	Entry.Grid.GridHalfLineCount = GridHalfLineCount;
	RenderBus.AddGridEntry(std::move(Entry));
}

void FRenderCollector::CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus)
{
	TArray<FOverlayStatLine> Lines;
	OverlaySystem.BuildLines(Editor, Lines);
	const float TextScale = OverlaySystem.GetLayout().TextScale;

	for (const FOverlayStatLine& Line : Lines)
	{
		FFontEntry Entry = {};
		Entry.Font.Text = Line.Text;
		Entry.Font.Font = nullptr;
		Entry.Font.Scale = TextScale;
		Entry.Font.bScreenSpace = 1;
		Entry.Font.ScreenPosition = Line.ScreenPosition;

		RenderBus.AddOverlayFontEntry(std::move(Entry));
	}
}

void FRenderCollector::CollectDebugDraw(const FDebugDrawQueue& Queue, FRenderBus& RenderBus)
{
	if (!RenderBus.GetShowFlags().bDebugDraw) return;

	for (const FDebugDrawItem& Item : Queue.GetItems())
	{
		FDebugLineEntry Entry;
		Entry.Start = Item.Start;
		Entry.End = Item.End;
		Entry.Color = Item.Color;
		RenderBus.AddDebugLineEntry(std::move(Entry));
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

void FRenderCollector::CollectOctreeDebug(const FOctree* Node, FRenderBus& RenderBus, uint32 Depth)
{
	if (!Node) return;

	const FBoundingBox& Bounds = Node->GetCellBounds();
	if (!Bounds.IsValid()) return;

	const FColor& Color = OctreeDepthColors[Depth % 6];
	const FVector& Min = Bounds.Min;
	const FVector& Max = Bounds.Max;

	// 8개 꼭짓점
	FVector V[8] = {
		FVector(Min.X, Min.Y, Min.Z),	// 0
		FVector(Max.X, Min.Y, Min.Z),	// 1
		FVector(Max.X, Max.Y, Min.Z),	// 2
		FVector(Min.X, Max.Y, Min.Z),	// 3
		FVector(Min.X, Min.Y, Max.Z),	// 4
		FVector(Max.X, Min.Y, Max.Z),	// 5
		FVector(Max.X, Max.Y, Max.Z),	// 6
		FVector(Min.X, Max.Y, Max.Z),	// 7
	};

	// 12에지
	static constexpr int32 Edges[][2] = {
		{0,1},{1,2},{2,3},{3,0},
		{4,5},{5,6},{6,7},{7,4},
		{0,4},{1,5},{2,6},{3,7}
	};

	for (const auto& E : Edges)
	{
		FDebugLineEntry Entry;
		Entry.Start = V[E[0]];
		Entry.End = V[E[1]];
		Entry.Color = Color;
		RenderBus.AddDebugLineEntry(std::move(Entry));
	}

	// 자식 노드 재귀
	for (const FOctree* Child : Node->GetChildren())
	{
		CollectOctreeDebug(Child, RenderBus, Depth + 1);
	}
}


// ============================================================
// Visible 프록시 수집 — UpdateVisibleProxies에서 구축한 dense 리스트만 순회
// ============================================================
void FRenderCollector::CollectVisibleProxies(const TArray<FPrimitiveSceneProxy*>& Proxies, FRenderBus& RenderBus)
{
	if (!RenderBus.GetShowFlags().bPrimitives) return;

	const bool bShowBoundingVolume = RenderBus.GetShowFlags().bBoundingVolume;
	SCOPE_STAT_CAT("CollectVisibleProxy", "3_Collect");

	const FGPUOcclusionCulling* Occlusion = RenderBus.GetOcclusionCulling();
	FGPUOcclusionCulling* OcclusionMut = RenderBus.GetOcclusionCullingMutable();
	const FLODUpdateContext& LODCtx = RenderBus.GetLODContext();

	// GatherAABB 병합: Collect 순회에서 동시에 AABB 수집 (별도 GatherLoop 제거)
	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->BeginGatherAABB(static_cast<uint32>(Proxies.size()));

	LOD_STATS_RESET();

	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{

		// LOD 갱신 — WorldTick에서 이동, 단일 순회에 병합
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

		// per-viewport 프록시: 매 프레임 카메라 데이터로 갱신
		if (Proxy->bPerViewportUpdate)
			Proxy->UpdatePerViewport(RenderBus);

		if (!Proxy->bVisible) continue;

		// AABB 수집 — 오클루전 체크 전에 수집해야 다음 프레임에 재평가 가능
		if (OcclusionMut)
			OcclusionMut->GatherAABB(Proxy);

		// GPU Occlusion Culling — 이전 프레임에서 가려진 프록시 스킵
		if (Occlusion && !Proxy->bNeverCull && Occlusion->IsOccluded(Proxy))
			continue;

		// Batcher 경유 렌더링 (Font, SubUV)
		if (Proxy->bBatcherRendered)
			Proxy->CollectEntries(RenderBus);
		else
			RenderBus.AddProxy(Proxy->Pass, Proxy);

		// 선택된 오브젝트
		if (Proxy->bSelected)
		{
			if (Proxy->bSupportsOutline)
				RenderBus.AddProxy(ERenderPass::SelectionMask, Proxy);

			if (bShowBoundingVolume && Proxy->bShowAABB)
			{
				FAABBEntry Entry = {};
				Entry.AABB.Min = Proxy->CachedBounds.Min;
				Entry.AABB.Max = Proxy->CachedBounds.Max;
				Entry.AABB.Color = FColor::White();
				RenderBus.AddAABBEntry(std::move(Entry));
			}
		}
	}

	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->EndGatherAABB();
}

void FRenderCollector::CollectDecals(UWorld* World, const TArray<FPrimitiveSceneProxy*>& VisibleProxies, FRenderBus& RenderBus)
{
	SCOPE_STAT_CAT("Decal::Collect", "Decal");

	if (!World) return;
	if (!RenderBus.GetShowFlags().bPrimitives || !RenderBus.GetShowFlags().bDecals) return;

	TMap<const UStaticMeshComponent*, const FPrimitiveSceneProxy*> VisibleStaticMeshProxies;
	VisibleStaticMeshProxies.reserve(VisibleProxies.size());
	FDecalStats::Reset();
	FDecalFrameStats& DecalStats = FDecalStats::GetMutable();

	for (const FPrimitiveSceneProxy* VisibleProxy : VisibleProxies)
	{
		if (!VisibleProxy || !VisibleProxy->bVisible) continue;

		const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(VisibleProxy->Owner);
		if (!StaticMeshComponent || !StaticMeshComponent->GetReceivesDecals()) continue;

		VisibleStaticMeshProxies[StaticMeshComponent] = VisibleProxy;
	}

	DecalStats.VisibleReceivers = static_cast<int32>(VisibleStaticMeshProxies.size());

	const FOctree* Octree = World->GetOctree();
	int32 BroadPhaseCandidateCount = 0;
	int32 NarrowPhaseAcceptedCount = 0;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;

		for (UActorComponent* Component : Actor->GetComponents())
		{
			UDecalComponent* DecalComponent = Cast<UDecalComponent>(Component);
			if (!DecalComponent || !DecalComponent->IsVisible()) continue;
			if (DecalComponent->GetFadeAlpha() <= 0.0f) continue;

			const FTextureResource* DecalTexture = DecalComponent->GetTexture();
			if (!DecalTexture) continue;
			++DecalStats.VisibleDecals;

			TArray<UPrimitiveComponent*> BroadPhaseCandidates;
			if (Octree)
			{
				const FBoundingBox DecalAABB = BuildDecalWorldAABB(*DecalComponent);
				if (DecalAABB.IsValid())
				{
					Octree->QueryAABB(DecalAABB, BroadPhaseCandidates);
				}
			}

			if (BroadPhaseCandidates.empty())
			{
				continue;
			}

			TSet<const UStaticMeshComponent*> UniqueCandidates;
			UniqueCandidates.reserve(BroadPhaseCandidates.size());

			for (UPrimitiveComponent* CandidatePrimitive : BroadPhaseCandidates)
			{
				UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(CandidatePrimitive);
				if (!StaticMeshComponent || !StaticMeshComponent->GetReceivesDecals()) continue;

				const auto VisibleProxyIt = VisibleStaticMeshProxies.find(StaticMeshComponent);
				if (VisibleProxyIt == VisibleStaticMeshProxies.end()) continue;

				UniqueCandidates.insert(StaticMeshComponent);
			}

			if (UniqueCandidates.empty())
			{
				continue;
			}

			DecalStats.UniqueCandidates += static_cast<int32>(UniqueCandidates.size());

			const FMatrix DecalWorldMatrix = DecalComponent->GetWorldMatrix();
			const FVector DecalHalfExtents = DecalComponent->GetHalfExtents();

			for (const UStaticMeshComponent* StaticMeshComponent : UniqueCandidates)
			{
				++BroadPhaseCandidateCount;

				const auto VisibleProxyIt = VisibleStaticMeshProxies.find(StaticMeshComponent);
				if (VisibleProxyIt == VisibleStaticMeshProxies.end()) continue;

				const FBoundingBox ReceiverBounds = StaticMeshComponent->GetWorldBoundingBox();
				if (!FIntersectionUtils::IntersectOBBAndAABB(DecalWorldMatrix, DecalHalfExtents, ReceiverBounds))
				{
					continue;
				}

				++NarrowPhaseAcceptedCount;

				FDecalDrawEntry Entry = {};
				Entry.ReceiverProxy = VisibleProxyIt->second;
				Entry.Texture = DecalTexture;
				Entry.Decal.WorldToDecal = DecalComponent->GetWorldInverseMatrix();
				Entry.Decal.DecalHalfExtents = DecalHalfExtents;
				Entry.Decal.FadeAlpha = DecalComponent->GetFadeAlpha();
				RenderBus.AddDecalEntry(std::move(Entry));
				++DecalStats.SubmittedDraws;
			}
		}
	}

	DecalStats.BroadCandidates = BroadPhaseCandidateCount;
	DecalStats.SATAccepted = NarrowPhaseAcceptedCount;
}

