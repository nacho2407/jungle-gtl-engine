#include "PlaneComponent.h"
#include "Primitive/PrimitivePlane.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(UPlaneComponent, UPrimitiveComponent)

void UPlaneComponent::Initialize()
{
	Primitive = std::make_unique<CPrimitivePlane>();
}