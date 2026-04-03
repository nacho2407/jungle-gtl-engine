#pragma once
#include "RenderBus.h"

class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;

class FRenderCollector {
public:
	void CollectWorld(UWorld* World, FRenderBus& RenderBus);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus);
	void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus);

private:
	void CollectFromScene(FScene& Scene, FRenderBus& RenderBus);
};
