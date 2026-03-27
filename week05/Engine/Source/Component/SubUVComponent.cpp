#include "SubUVComponent.h"
#include "Object/Class.h"
#include "Primitive/PrimitiveBase.h"

IMPLEMENT_RTTI(USubUVComponent, UPrimitiveComponent)

void USubUVComponent::Initialize()
{
	// SubUV 렌더링용 메시 객체 생성
	bDrawDebugBounds = true;
	SubUVMesh = std::make_shared<FMeshData>();
}

FBoxSphereBounds USubUVComponent::GetWorldBounds() const
{
	const FVector Center = GetWorldLocation();
	const FVector WorldScale = GetWorldTransform().GetScaleVector();

	const float HalfW = Size.X * 0.5f * WorldScale.X;
	const float HalfH = Size.Y * 0.5f * WorldScale.Y;
	const float HalfZ = ((HalfW > HalfH) ? HalfW : HalfH);

	const FVector BoxExtent(HalfW, HalfH, HalfZ);
	return { Center, BoxExtent.Size(), BoxExtent };
}