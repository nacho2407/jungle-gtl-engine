#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/EditorEngine.h"
#include "Render/Proxy/FScene.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"

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

// ============================================================
// FScene 프록시 기반 수집 — Owner 참조 없이 프록시 데이터만 사용
// ============================================================
void FRenderCollector::CollectFromScene(FScene& Scene, FRenderBus& RenderBus)
{
	if (!RenderBus.GetShowFlags().bPrimitives) return;

	const bool bShowBoundingVolume = RenderBus.GetShowFlags().bBoundingVolume;

	for (FPrimitiveSceneProxy* Proxy : Scene.GetAllProxies())
	{
		if (!Proxy) continue;

		// per-viewport 프록시: 매 프레임 카메라 데이터로 갱신
		if (Proxy->bPerViewportUpdate)
			Proxy->UpdatePerViewport(RenderBus);

		if (!Proxy->bVisible) continue;

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
