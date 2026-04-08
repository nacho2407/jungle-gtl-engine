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
	const FVector Center = GetWorldLocation();
	const FVector WorldScale = GetWorldTransform().GetScaleVector();

	const float HalfW = Size.X * 0.5f * WorldScale.X;
	const float HalfH = Size.Y * 0.5f * WorldScale.Y;
	const float HalfZ = ((HalfW > HalfH) ? HalfW : HalfH);

	const FVector BoxExtent(HalfW, HalfH, HalfZ);
	return { Center, BoxExtent.Size(), BoxExtent };
}

FRenderMesh* UBillboardComponent::GetRenderMesh() const 
{ 
	return BillboardMesh.get(); 
}
