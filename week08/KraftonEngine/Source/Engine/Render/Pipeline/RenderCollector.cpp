#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/GizmoComponent.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/EditorEngine.h"
#include "Render/Pipeline/FScene.h"
#include "Render/Pipeline/PrimitiveSceneProxy.h"
#include "Component/PrimitiveComponent.h"

void FRenderCollector::CollectWorld(UWorld* World, FRenderBus& RenderBus)
{
	if (!World) return;

	// 프록시 기반 수집: FScene에서 캐싱된 렌더 데이터를 직접 제출
	FScene& Scene = World->GetScene();
	Scene.UpdateDirtyProxies();
	CollectFromScene(Scene, RenderBus);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus)
{
	FGridEntry Entry = {};
	Entry.Grid.GridSpacing = GridSpacing;
	Entry.Grid.GridHalfLineCount = GridHalfLineCount;
	RenderBus.AddGridEntry(std::move(Entry));
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, ELevelViewportType ViewportType, FRenderBus& RenderBus)
{
	if (!Gizmo) return;

	Gizmo->UpdateAxisMask(ViewportType);
	// Gizmo 렌더링은 프록시 경로로 처리 (CollectFromScene에서 UpdatePerViewport)
}

void FRenderCollector::CollectOverlayText(bool bActive, const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus)
{
	if (!bActive) return;

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

// ============================================================
// FScene 프록시 기반 수집 — FRenderCommand 생성 없음
// ============================================================
void FRenderCollector::CollectFromScene(FScene& Scene, FRenderBus& RenderBus)
{
	if (!RenderBus.GetShowFlags().bPrimitives) return;

	for (FPrimitiveSceneProxy* Proxy : Scene.GetAllProxies())
	{
		if (!Proxy) continue;
		if (!Proxy->Owner) continue;

		// per-viewport 프록시: 매 프레임 카메라 데이터로 갱신
		if (Proxy->bPerViewportUpdate)
		{
			Proxy->UpdatePerViewport(RenderBus);
			if (!Proxy->bVisible) continue;

			// Batcher 경유 렌더링 (Font, SubUV): CollectRender로 Entry 생성
			if (Proxy->bBatcherRendered)
				Proxy->Owner->CollectRender(RenderBus);
			else
				RenderBus.AddProxy(Proxy->Pass, Proxy);

			// Selection
			if (Proxy->bSelected && Proxy->Owner->SupportsOutline())
			{
				RenderBus.AddProxy(ERenderPass::SelectionMask, Proxy);
				// Billboard 계열은 AABB 제외
			}
			continue;
		}

		// 일반 프록시
		if (!Proxy->bVisible) continue;

		// Actor 가시성 체크
		AActor* OwnerActor = Proxy->Owner->GetOwner();
		if (OwnerActor && !OwnerActor->IsVisible()) continue;

		// 프록시 포인터를 패스 큐에 직접 제출
		RenderBus.AddProxy(Proxy->Pass, Proxy);

		// 선택된 오브젝트 → SelectionMask에 같은 프록시 제출
		if (Proxy->bSelected)
		{
			RenderBus.AddProxy(ERenderPass::SelectionMask, Proxy);

			if (RenderBus.GetShowFlags().bBoundingVolume)
			{
				FBoundingBox Box = Proxy->Owner->GetWorldBoundingBox();
				FAABBEntry Entry = {};
				Entry.AABB.Min = Box.Min;
				Entry.AABB.Max = Box.Max;
				Entry.AABB.Color = FColor::White();
				RenderBus.AddAABBEntry(std::move(Entry));
			}
		}
	}
}
