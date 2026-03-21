#pragma once
#include "RenderBus.h"
#include "RenderCollectorContext.h"
#include "Component/Camera.h"
#include "Component/PrimitiveComponent.h"
#include "Render/Resource/MeshBufferManager.h"

class UWorld;
class AActor;
//class FMeshBufferManager;

class FRenderCollector {
private:
	static FMeshBufferManager MeshBufferManager;
public:
	static void Initialize(ID3D11Device * InDevice) { MeshBufferManager.Create(InDevice); }
	static void Release() { MeshBufferManager.Release(); }
	static void Collect(const FRenderCollectorContext& Context, FRenderBus& RenderBus);

private:
	static void CollectFromActor(AActor* Actor,const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	static void CollectFromComponent(UPrimitiveComponent* primitiveComponent, const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	static void CollectFromEditor(const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	static void CollectGizmo(const FRenderCollectorContext& Context,  FRenderBus& RenderBus);
	static void CollectMouseOverlay(const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	static void CollectComponentOutline(UPrimitiveComponent* primitiveComponent, const FRenderCollectorContext& Context, FRenderBus& RenderBus);
	static void CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, FRenderBus& RenderBus);
};