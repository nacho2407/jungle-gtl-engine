#include "CapsuleComponent.h"
#include "Object/Object.h"

DEFINE_CLASS(UCapsuleComponent, UShapeComponent)
REGISTER_FACTORY(UCapsuleComponent)

void UCapsuleComponent::UpdateWorldAABB() const
{
    const FVector Center = GetWorldLocation();

    // scale 반영 (비균일 대응)
    const float Radius = GetScaledCapsuleRadius();

    const float HalfHeight = GetScaledCapsuleHalfHeight();

    // capsule segment endpoints (Z-up 기준)
    const FVector Up = FVector(0, 0, 1);

    FVector A = Center + Up * HalfHeight;
    FVector B = Center - Up * HalfHeight;

    // segment AABB 먼저 만들고 radius expand
    FVector Min = FVector(
        std::min(A.X, B.X),
        std::min(A.Y, B.Y),
        std::min(A.Z, B.Z));

    FVector Max = FVector(
        std::max(A.X, B.X),
        std::max(A.Y, B.Y),
        std::max(A.Z, B.Z));

    FVector R(Radius, Radius, Radius);

    WorldAABB.Min = Min - R;
    WorldAABB.Max = Max + R;
}

bool UCapsuleComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    return false;
}

EPrimitiveType UCapsuleComponent::GetPrimitiveType() const
{
    return EPrimitiveType::EPT_Capsule;
}
