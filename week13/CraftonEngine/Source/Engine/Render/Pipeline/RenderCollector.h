#pragma once
#include "RenderBus.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Resource/ShaderManager.h"

class UWorld;
class AActor;
class UPrimitiveComponent;
class UGizmoComponent;

class FRenderCollector {
public:
	void CollectWorld(UWorld* World, const TArray<AActor*>& SelectedActors, FRenderBus& RenderBus);
	void CollectGizmo(UGizmoComponent* Gizmo, FRenderBus& RenderBus);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus);
	void CollectOverlayText(const TArray<FScreenTextItem>& Items, float TextScale, FRenderBus& RenderBus);

private:
	void CollectFromActor(AActor* Actor, bool bSelected, FRenderBus& RenderBus);
	void CollectFromComponent(UPrimitiveComponent* Primitive, FRenderBus& RenderBus);
	void CollectSelectionEffects(UPrimitiveComponent* Primitive, FRenderBus& RenderBus);
	void CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, FRenderBus& RenderBus);
};
