#include "BoxComponent.h" 
#include "Object/Object.h"


DEFINE_CLASS(UBoxComponent, UShapeComponent)
REGISTER_FACTORY(UBoxComponent)

bool UBoxComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    return false;
}

EPrimitiveType UBoxComponent::GetPrimitiveType() const
{
    return EPrimitiveType::EPT_Box;
}
