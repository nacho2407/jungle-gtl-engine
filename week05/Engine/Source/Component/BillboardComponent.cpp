#include "BillboardComponent.h"
#include "Object/Class.h"
#include "Renderer/MeshData.h"

IMPLEMENT_RTTI(UBillboardComponent, UPrimitiveComponent)


void UBillboardComponent::PostConstruct()
{
	bDrawDebugBounds = false;
	BillboardMesh = std::make_shared<FDynamicMesh>();
}

FBoxSphereBounds UBillboardComponent::GetWorldBounds() const
{
	FBoxSphereBounds Bounds;
	Bounds.Center = GetWorldTransform().GetTranslation();
	Bounds.Radius = (std::max)(Size.X, Size.Y) * 0.5f;
	return Bounds;
}

FRenderMesh* UBillboardComponent::GetRenderMesh() const 
{ 
	return BillboardMesh.get(); 
}
