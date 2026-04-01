#pragma once
#include "RenderBus.h"

class UWorld;
class AActor;
class UGizmoComponent;
class FOverlayStatSystem;
class UEditorEngine;
enum class ELevelViewportType : uint8;

class FRenderCollector {
public:
	void CollectWorld(UWorld* World, const TArray<AActor*>& SelectedActors, FRenderBus& RenderBus);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus);
	void CollectGizmo(UGizmoComponent* Gizmo, ELevelViewportType ViewportType, FRenderBus& RenderBus);
	void CollectOverlayText(bool bActive, const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FRenderBus& RenderBus);

private:
	void CollectFromActor(AActor* Actor, bool bSelected, FRenderBus& RenderBus);
};
