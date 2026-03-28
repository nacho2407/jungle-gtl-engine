#include "SphereComponent.h"
#include "Primitive/PrimitiveSphere.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(USphereComponent, UPrimitiveComponent)

void USphereComponent::PostConstruct()
{
	Primitive = std::make_unique<FPrimitiveSphere>();
}
