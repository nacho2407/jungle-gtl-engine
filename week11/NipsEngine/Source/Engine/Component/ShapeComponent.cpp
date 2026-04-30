#include "ShapeComponent.h"

DEFINE_CLASS(UShapeComponent, UPrimitiveComponent)
REGISTER_FACTORY(UShapeComponent)

void UShapeComponent::UpdateWorldAABB() const
{
}

bool UShapeComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    return false;
}

EPrimitiveType UShapeComponent::GetPrimitiveType() const
{
    return EPrimitiveType::EPT_Shape;
}
