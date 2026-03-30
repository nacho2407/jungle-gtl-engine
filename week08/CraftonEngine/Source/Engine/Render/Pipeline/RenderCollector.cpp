#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"

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

void FRenderCollector::CollectOverlayText(const TArray<FScreenTextItem>& Items, float TextScale, FRenderBus& RenderBus)
{
	for (const FScreenTextItem& Item : Items)
	{
		FFontEntry Entry = {};
		Entry.Font.Text = Item.Text;
		Entry.Font.Font = nullptr;
		Entry.Font.Scale = TextScale;
		Entry.Font.bScreenSpace = 1;
		Entry.Font.ScreenPosition = Item.ScreenPosition;

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
