#include "SphereComponent.h"
#include "Primitive/PrimitiveSphere.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(USphereComponent, UPrimitiveComponent)

void USphereComponent::Initialize()
{
	Primitive = std::make_unique<CPrimitiveSphere>();
}