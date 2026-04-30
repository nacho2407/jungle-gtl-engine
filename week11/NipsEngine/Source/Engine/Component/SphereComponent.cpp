#include "SphereComponent.h"
#include "Object/Object.h"

DEFINE_CLASS(USphereComponent, UShapeComponent)
REGISTER_FACTORY(USphereComponent)

void USphereComponent::UpdateWorldAABB() const
{
    const FVector Center = GetWorldLocation();

    // World scale 반영 (비균일 스케일까지 고려)
    const FVector Scale = GetWorldScale();

    const float ScaledRadius = GetScaledSphereRadius();
    WorldAABB.Min = Center - FVector(ScaledRadius, ScaledRadius, ScaledRadius);
    WorldAABB.Max = Center + FVector(ScaledRadius, ScaledRadius, ScaledRadius);
}

bool USphereComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    return false;
}

EPrimitiveType USphereComponent::GetPrimitiveType() const
{
    return EPrimitiveType::EPT_Sphere;
}
