#include "SkyComponent.h"
#include "Primitive/PrimitiveSky.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(USkyComponent, UPrimitiveComponent)

void USkyComponent::Initialize()
{
	bDrawDebugBounds = false;
	Primitive = std::make_unique<CPrimitiveSky>();
}

FBoxSphereBounds USkyComponent::GetWorldBounds() const
{
	return { FVector(0,0,0), FLT_MAX, FVector(FLT_MAX,FLT_MAX,FLT_MAX) };
}
