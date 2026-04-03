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
	Gizmo->CollectRender(RenderBus);
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
// FScene 프록시 기반 수집
// ============================================================
void FRenderCollector::CollectFromScene(FScene& Scene, FRenderBus& RenderBus)
{
	if (!RenderBus.GetShowFlags().bPrimitives) return;

	for (FPrimitiveSceneProxy* Proxy : Scene.GetAllProxies())
	{
		if (!Proxy) continue;
		if (!Proxy->bVisible) continue;
		if (!Proxy->Owner) continue;

		// per-viewport 프록시(Gizmo, Billboard)는 기존 경로로 폴백
		if (Proxy->bPerViewportUpdate)
		{
			Proxy->Owner->CollectRender(RenderBus);

			if (Proxy->bSelected)
			{
				Proxy->Owner->CollectSelection(RenderBus);
			}
			continue;
		}

		// Actor 가시성 체크
		AActor* OwnerActor = Proxy->Owner->GetOwner();
		if (OwnerActor && !OwnerActor->IsVisible()) continue;

		// 프록시 포인터를 패스 큐에 직접 제출 (FRenderCommand 복사 없음)
		RenderBus.AddProxy(Proxy->Pass, Proxy);

		// 선택된 오브젝트 → SelectionMask에 같은 프록시 제출 (FRenderCommand 생성 없음)
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
