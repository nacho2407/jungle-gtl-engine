#include "SkyComponent.h"
#include "Primitive/PrimitiveSky.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(USkyComponent, UPrimitiveComponent)

void USkyComponent::PostConstruct()
{
	bDrawDebugBounds = false;
	Primitive = std::make_unique<FPrimitiveSky>();
}

FBoxSphereBounds USkyComponent::GetWorldBounds() const
{
	return { FVector(0,0,0), FLT_MAX, FVector(FLT_MAX,FLT_MAX,FLT_MAX) };
}
