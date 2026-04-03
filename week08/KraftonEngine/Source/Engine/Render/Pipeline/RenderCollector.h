#pragma once
#include "RenderBus.h"

class UWorld;
class UGizmoComponent;
class FOverlayStatSystem;
class UEditorEngine;
class FScene;
enum class ELevelViewportType : uint8;

class FRenderCollector {
public:
	void CollectWorld(UWorld* World, FRenderBus& RenderBus);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus);
	void CollectGizmo(UGizmoComponent* Gizmo, ELevelViewportType ViewportType, FRenderBus& RenderBus);
	void CollectOverlayText(bool bActive, const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus);

private:
	void CollectFromScene(FScene& Scene, FRenderBus& RenderBus);
};
