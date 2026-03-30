#pragma once
#include "RenderBus.h"

class UWorld;
class AActor;

class FRenderCollector {
public:
	void CollectWorld(UWorld* World, const TArray<AActor*>& SelectedActors, FRenderBus& RenderBus);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus);
	void CollectOverlayText(const TArray<FScreenTextItem>& Items, float TextScale, FRenderBus& RenderBus);

private:
	void CollectFromActor(AActor* Actor, bool bSelected, FRenderBus& RenderBus);
};
