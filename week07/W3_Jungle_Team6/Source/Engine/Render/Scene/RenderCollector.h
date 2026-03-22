#pragma once
#include "RenderBus.h"
#include "RenderCollectorContext.h"
#include "Render/Resource/MeshBufferManager.h"

class UWorld;
class AActor;
class UPrimitiveComponent;

class FRenderCollector {
private:
	FMeshBufferManager MeshBufferManager;
public:
	void Initialize(ID3D11Device * InDevice) { MeshBufferManager.Create(InDevice); }
	void Release() { MeshBufferManager.Release(); }
	void Collect(const FRenderCollectorContext& Context, FRenderBus& RenderBus);

private:
	void CollectFromActor(AActor* Actor,const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	void CollectFromComponent(UPrimitiveComponent* primitiveComponent, const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	void CollectFromEditor(const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	void CollectGizmo(const FRenderCollectorContext& Context,  FRenderBus& RenderBus);
	void CollectMouseOverlay(const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	void CollectComponentOutline(UPrimitiveComponent* primitiveComponent, const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	void CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, FRenderBus& RenderBus);
};