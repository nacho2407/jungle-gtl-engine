#pragma once
#include "RenderBus.h"
#include "RenderCollectorContext.h"
#include "Render/Resource/MeshBufferManager.h"

#include <functional>

class UWorld;
class AActor;
class UPrimitiveComponent;
class UGizmoComponent;
struct FCursorOverlayState;

using FCollectPhase = std::function<void(FRenderBus&)>;

class FRenderCollector {
private:
	FMeshBufferManager MeshBufferManager;
public:
	void Initialize(ID3D11Device* InDevice) { MeshBufferManager.Create(InDevice); }
	void Release() { MeshBufferManager.Release(); }

	void Collect(const FRenderCollectorContext& Context, const TArray<FCollectPhase>& Phases, FRenderBus& RenderBus);

	// Phase building blocks
	void CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus);
	void CollectMouseOverlay(const FCursorOverlayState* State, float ViewportW, float ViewportH, FRenderBus& RenderBus);

private:
	void CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, FRenderBus& RenderBus);
};
