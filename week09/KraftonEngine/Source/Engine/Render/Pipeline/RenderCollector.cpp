#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/EditorEngine.h"
#include "Render/Proxy/FScene.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Proxy/TextRenderSceneProxy.h"
#include "Render/DebugDraw/DebugDrawQueue.h"
#include "Render/Culling/GPUOcclusionCulling.h"
#include "Render/Pipeline/LODContext.h"
#include "Render/Pipeline/Renderer.h"
#include "Profiling/Stats.h"
#include <Collision/Octree.h>

void FRenderCollector::CollectWorld(UWorld* World, const FFrameContext& Frame, FRenderer& Renderer)
{
	if (!World) return;

	// Dirty 프록시 갱신 후 visible 리스트만 순회
	World->GetScene().UpdateDirtyProxies();
	CollectVisibleProxies(World->GetVisibleProxies(), Frame, World->GetScene(), Renderer);
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

void FRenderCollector::CollectDebugDraw(const FDebugDrawQueue& Queue, const FFrameContext& Frame, FScene& Scene)
{
	if (!Frame.ShowFlags.bDebugDraw) return;

	for (const FDebugDrawItem& Item : Queue.GetItems())
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
		Scene.AddDebugLine(V[E[0]], V[E[1]], Color);
	}

	// 자식 노드 재귀
	for (const FOctree* Child : Node->GetChildren())
	{
		CollectOctreeDebug(Child, Scene, Depth + 1);
	}
}


// ============================================================
// Visible 프록시 수집 — Proxy → FDrawCommand 직접 변환
// ============================================================
void FRenderCollector::CollectVisibleProxies(const TArray<FPrimitiveSceneProxy*>& Proxies, const FFrameContext& Frame, FScene& Scene, FRenderer& Renderer)
{
	if (!Frame.ShowFlags.bPrimitives) return;

	const bool bShowBoundingVolume = Frame.ShowFlags.bBoundingVolume;
	SCOPE_STAT_CAT("CollectVisibleProxy", "3_Collect");

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

		// Font 프록시는 동적 VB 배칭 경로 (개별 FDrawCommand가 아닌 FontGeometry)
		if (Proxy->Pass == ERenderPass::Font)
		{
			const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
			if (!TextProxy->CachedText.empty())
				Renderer.AddWorldText(TextProxy, Frame);
		}
		else
		{
			// Proxy → FDrawCommand 직접 변환
			Renderer.BuildCommandForProxy(*Proxy, Proxy->Pass);
		}

		// 선택된 오브젝트
		if (Proxy->bSelected)
		{
			if (Proxy->bSupportsOutline)
				Renderer.BuildCommandForProxy(*Proxy, ERenderPass::SelectionMask);

			if (bShowBoundingVolume && Proxy->bShowAABB)
			{
				Scene.AddDebugAABB(
					Proxy->CachedBounds.Min,
					Proxy->CachedBounds.Max,
					FColor::White());
			}
		}
	}

	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->EndGatherAABB();
}
