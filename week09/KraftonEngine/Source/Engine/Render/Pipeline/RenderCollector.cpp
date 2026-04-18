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
#include "Render/Renderer.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Render/Proxy/FScene.h"
#include "Render/Proxy/LightSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Proxy/TextRenderSceneProxy.h"

#include <Collision/Octree.h>
#include <Collision/SpatialPartition.h>

void FRenderCollector::CollectWorld(UWorld* World, const FFrameContext& Frame, FRenderer& Renderer)
{
	if (!World) return;

	FScene& Scene = World->GetScene();
	Scene.UpdateDirtyProxies();
	Scene.UpdateDirtyLightProxies();

	// Visible Primitive Proxises 수집 — 프러스텀 + Occlusion Culling
	LastVisibleProxies.clear();
	{
		SCOPE_STAT_CAT("FrustumCulling", "3_Collect");
		const uint32 ExpectedCount = Scene.GetPrimitiveProxyCount() + static_cast<uint32>(Scene.GetNeverCullProxies().size());
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

	// Visible Primitive Proxies → FDrawCommand 직접 변환
	CollectVisibleProxies(LastVisibleProxies, Frame, Scene, Renderer);

	// Light Proxy → FLightConstants 배열로 수집 (드로우콜 불필요, CB 데이터만 추출)
	CollectLights(Scene);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene)
{
	Scene.SetGrid(GridSpacing, GridHalfLineCount);
}

void FRenderCollector::CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene)
{
	(void)OverlaySystem;
	(void)Editor;
	(void)Scene;
	// Stat 오버레이는 ImGui 기반으로 뷰포트 내부에 직접 렌더링합니다.
}

void FRenderCollector::CollectDebugDraw(const FFrameContext& Frame, FScene& Scene)
{
	if (!Frame.ShowFlags.bDebugDraw) return;

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

	TSet<FPrimitiveSceneProxy*> VisibleProxySet;
	VisibleProxySet.reserve(Proxies.size());
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (Proxy)
		{
			VisibleProxySet.insert(Proxy);
		}
	}

	const FGPUOcclusionCulling* Occlusion = Frame.OcclusionCulling;
	FGPUOcclusionCulling* OcclusionMut = Frame.OcclusionCulling;
	const FLODUpdateContext& LODCtx = Frame.LODContext;

	// GatherAABB 병합: Collect 순회에서 동시에 AABB 수집 (별도 GatherLoop 제거)
	if (OcclusionMut && OcclusionMut->IsInitialized())
	{
		OcclusionMut->BeginGatherAABB(static_cast<uint32>(Proxies.size()));
	}

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
		{
			Proxy->UpdatePerViewport(Frame);
		}

		if (!Proxy->bVisible)
		{
			continue;
		}

		// AABB 수집 — 오클루전 체크 전에 수집해야 다음 프레임에 재평가 가능
		if (OcclusionMut)
		{
			OcclusionMut->GatherAABB(Proxy);
		}

		// GPU Occlusion Culling — 이전 프레임에서 가려진 프록시 스킵
		if (Occlusion && !Proxy->bNeverCull && Occlusion->IsOccluded(Proxy))
		{
			continue;
		}

		// Font 프록시는 동적 VB 배칭 경로 (개별 FDrawCommand가 아닌 FontGeometry)
		if (Proxy->bFontBatched)
		{
			const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
			if (!TextProxy->CachedText.empty())
			{
				Renderer.AddWorldText(TextProxy, Frame);
			}
		}
		// Decal 프록시는 Decal-Receiver에 렌더링 의존하므로 특별 취급
		else if (Cast<UDecalComponent>(Proxy->Owner))
		{
			FDecalSceneProxy* DecalProxy = static_cast<FDecalSceneProxy*>(Proxy);
			if (Renderer.HasActiveViewModePipeline())
			{
				Renderer.BuildDecalCommand(*DecalProxy);
			}
			else
			{
				UDecalComponent* DecalComponent = static_cast<UDecalComponent*>(Proxy->Owner);

				for (UStaticMeshComponent* Receiver : DecalComponent->GetReceivers())
				{
					if (!Receiver)
					{
						continue;
					}

					FPrimitiveSceneProxy* ReceiverProxy = Receiver->GetSceneProxy();
					if (!ReceiverProxy || VisibleProxySet.find(ReceiverProxy) == VisibleProxySet.end())
					{
						continue;
					}

					if (LODCtx.bValid && LODCtx.ShouldRefreshLOD(ReceiverProxy->ProxyId, ReceiverProxy->LastLODUpdateFrame))
					{
						const FVector& ProxyPos = ReceiverProxy->CachedWorldPos;
						const float dx = LODCtx.CameraPos.X - ProxyPos.X;
						const float dy = LODCtx.CameraPos.Y - ProxyPos.Y;
						const float dz = LODCtx.CameraPos.Z - ProxyPos.Z;
						const float DistSq = dx * dx + dy * dy + dz * dz;
						ReceiverProxy->UpdateLOD(SelectLOD(ReceiverProxy->CurrentLOD, DistSq));
						ReceiverProxy->LastLODUpdateFrame = LODCtx.LODUpdateFrame;
					}

					if (ReceiverProxy->bPerViewportUpdate)
					{
						ReceiverProxy->UpdatePerViewport(Frame);
					}

					Renderer.BuildDecalCommandForReceiver(*ReceiverProxy, *DecalProxy);
				}
			}
		}
		else
		{
			// Proxy → FDrawCommand 직접 변환
			Renderer.BuildCommandForProxy(*Proxy, Proxy->Pass);
		}

		// 선택된 오브젝트 — 아웃라인 + AABB + 컴포넌트 디버그 시각화
		if (Proxy->bSelected)
		{
			if (Proxy->bSupportsOutline)
			{
				Renderer.BuildCommandForProxy(*Proxy, ERenderPass::SelectionMask);
			}

			if (bShowBoundingVolume && Proxy->bShowAABB)
			{
				Scene.AddDebugAABB(Proxy->CachedBounds.Min, Proxy->CachedBounds.Max, FColor::White());
			}

			//TODO: Owner 의존성 제거
			Proxy->CollectSelectedVisuals(Scene);
		}
	}

	if (OcclusionMut && OcclusionMut->IsInitialized())
	{
		OcclusionMut->EndGatherAABB();
	}
}

// ============================================================
// CollectLights — Light 프록시 → FLightConstants 배열 수집
// Light는 드로우콜이 없으므로 Proxy가 아닌 GPU 상수값만 추출해 저장한다.
// 순회·필터링은 RenderCollector가 직접 담당한다.
// ============================================================
void FRenderCollector::CollectLights(FScene& Scene)
{
    CollectedLights.clear();
    for (const FLightSceneProxy* Proxy : Scene.GetLightProxies())
    {
        if (!Proxy || !Proxy->bVisible || !Proxy->bAffectsWorld)
            continue;
        CollectedLights.push_back(Proxy->LightConstants);
    }
}
