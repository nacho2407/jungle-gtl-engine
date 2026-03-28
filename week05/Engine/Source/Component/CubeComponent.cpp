#include "CubeComponent.h"
#include "Primitive/PrimitiveCube.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(UCubeComponent, UPrimitiveComponent)

void UCubeComponent::Initialize()
{
	Primitive = std::make_unique<CPrimitiveCube>();
}