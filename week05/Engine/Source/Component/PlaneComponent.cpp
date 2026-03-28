#include "PlaneComponent.h"
#include "Primitive/PrimitivePlane.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(UPlaneComponent, UPrimitiveComponent)

void UPlaneComponent::PostConstruct()
{
	Primitive = std::make_unique<FPrimitivePlane>();
}
