#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/EditorEngine.h"
#include "Render/Proxy/FScene.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/DebugDraw/DebugDrawQueue.h"
#include "Render/Culling/GPUOcclusionCulling.h"
#include "Render/Pipeline/LODContext.h"
#include "Profiling/Stats.h"
#include <Collision/Octree.h>

void FRenderCollector::CollectWorld(UWorld* World, FRenderBus& RenderBus)
{
	if (!World) return;

	// Dirty 프록시 갱신 후 visible 리스트만 순회
	World->GetScene().UpdateDirtyProxies();
	CollectVisibleProxies(World->GetVisibleProxies(), RenderBus);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus)
{
	RenderBus.SetGrid(GridSpacing, GridHalfLineCount);
}

void FRenderCollector::CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus)
{
	TArray<FOverlayStatLine> Lines;
	OverlaySystem.BuildLines(Editor, Lines);
	const float TextScale = OverlaySystem.GetLayout().TextScale;

	for (FOverlayStatLine& Line : Lines)
	{
		RenderBus.AddOverlayText(std::move(Line.Text), Line.ScreenPosition, TextScale);
	}
}

void FRenderCollector::CollectDebugDraw(const FDebugDrawQueue& Queue, FRenderBus& RenderBus)
{
	if (!RenderBus.Frame.ShowFlags.bDebugDraw) return;

	for (const FDebugDrawItem& Item : Queue.GetItems())
	{
		RenderBus.AddDebugLine(Item.Start, Item.End, Item.Color);
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
		RenderBus.AddDebugLine(V[E[0]], V[E[1]], Color);
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
	if (!RenderBus.Frame.ShowFlags.bPrimitives) return;

	const bool bShowBoundingVolume = RenderBus.Frame.ShowFlags.bBoundingVolume;
	SCOPE_STAT_CAT("CollectVisibleProxy", "3_Collect");

	const FFrameContext& Frame = RenderBus.Frame;
	const FGPUOcclusionCulling* Occlusion = Frame.OcclusionCulling;
	FGPUOcclusionCulling* OcclusionMut = Frame.OcclusionCulling;
	const FLODUpdateContext& LODCtx = Frame.LODContext;

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
			Proxy->UpdatePerViewport(Frame);

		if (!Proxy->bVisible) continue;

		// AABB 수집 — 오클루전 체크 전에 수집해야 다음 프레임에 재평가 가능
		if (OcclusionMut)
			OcclusionMut->GatherAABB(Proxy);

		// GPU Occlusion Culling — 이전 프레임에서 가려진 프록시 스킵
		if (Occlusion && !Proxy->bNeverCull && Occlusion->IsOccluded(Proxy))
			continue;

		// 모든 프록시를 직접 ProxyQueue에 제출
		RenderBus.AddProxy(Proxy->Pass, Proxy);

		// 선택된 오브젝트
		if (Proxy->bSelected)
		{
			if (Proxy->bSupportsOutline)
				RenderBus.AddProxy(ERenderPass::SelectionMask, Proxy);

			if (bShowBoundingVolume && Proxy->bShowAABB)
			{
				RenderBus.AddDebugAABB(
					Proxy->CachedBounds.Min,
					Proxy->CachedBounds.Max,
					FColor::White());
			}
		}
	}

	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->EndGatherAABB();
}
