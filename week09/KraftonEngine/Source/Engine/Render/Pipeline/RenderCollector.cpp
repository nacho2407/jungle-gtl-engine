#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/EditorEngine.h"
#include "Render/Proxy/FScene.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/DebugDraw/DebugDrawQueue.h"
#include "Render/Culling/GPUOcclusionCulling.h"
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
	FGridEntry Entry = {};
	Entry.Grid.GridSpacing = GridSpacing;
	Entry.Grid.GridHalfLineCount = GridHalfLineCount;
	RenderBus.AddGridEntry(std::move(Entry));
}

void FRenderCollector::CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus)
{
	const TArray<FOverlayStatLine> Lines = OverlaySystem.BuildLines(Editor);
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

	const FBoundingBox& Bounds = Node->GetBounds();
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

	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		// per-viewport 프록시: 매 프레임 카메라 데이터로 갱신
		if (Proxy->bPerViewportUpdate)
			Proxy->UpdatePerViewport(RenderBus);

		if (!Proxy->bVisible) continue;

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

			if (bShowBoundingVolume)
			{
				FAABBEntry Entry = {};
				Entry.AABB.Min = Proxy->CachedBounds.Min;
				Entry.AABB.Max = Proxy->CachedBounds.Max;
				Entry.AABB.Color = FColor::White();
				RenderBus.AddAABBEntry(std::move(Entry));
			}
		}
	}
}

