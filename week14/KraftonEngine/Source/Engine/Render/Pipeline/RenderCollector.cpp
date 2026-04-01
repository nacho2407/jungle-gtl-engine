#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/EditorEngine.h"

#include <algorithm>

void FRenderCollector::CollectWorld(UWorld* World, const TArray<AActor*>& SelectedActors, FRenderBus& RenderBus)
{
	if (!World) return;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;
		bool bSelected = std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end();
		CollectFromActor(Actor, bSelected, RenderBus);
	}
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

void FRenderCollector::CollectFromActor(AActor* Actor, bool bSelected, FRenderBus& RenderBus)
{
	if (!Actor->IsVisible()) return;

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		if (!Primitive->IsVisible()) continue;

		Primitive->CollectRender(RenderBus);

		if (bSelected)
		{
			Primitive->CollectSelection(RenderBus);
		}
	}
}
