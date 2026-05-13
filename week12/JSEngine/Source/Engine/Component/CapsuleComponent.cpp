#include "CapsuleComponent.h"
#include "Object/Object.h"

DEFINE_CLASS(UCapsuleComponent, UShapeComponent)
REGISTER_FACTORY(UCapsuleComponent)

void UCapsuleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "CapsuleHalfHeight", EPropertyType::Float, &CapsuleHalfHeight });
    OutProps.push_back({ "CapsuleRadius", EPropertyType::Float, &CapsuleRadius });
}

void UCapsuleComponent::PostDuplicate(UObject* Original)
{
    UShapeComponent::PostDuplicate(Original);

	UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(Original);
    CapsuleHalfHeight = CapsuleComp->CapsuleHalfHeight;
    CapsuleRadius = CapsuleComp->CapsuleRadius;
}

void UCapsuleComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << "CapsuleHalfHeight" << CapsuleHalfHeight;
    Ar << "CapsuleRadius" << CapsuleRadius;
}

void UCapsuleComponent::UpdateWorldAABB() const
{
    FTransform T = GetWorldTransform();

    FVector Center = T.GetLocation();
    FVector Axis = T.GetUnitAxis(EAxis::Z);

    float HalfHeight = GetScaledCapsuleHalfHeight();
    float Radius = GetScaledCapsuleRadius();

    FVector A = Center + Axis * HalfHeight;
    FVector B = Center - Axis * HalfHeight;

    FVector Min(
        std::min(A.X, B.X),
        std::min(A.Y, B.Y),
        std::min(A.Z, B.Z));

    FVector Max(
        std::max(A.X, B.X),
        std::max(A.Y, B.Y),
        std::max(A.Z, B.Z));

    // radius expansion (AABB padding)
    Min -= FVector(Radius, Radius, Radius);
    Max += FVector(Radius, Radius, Radius);

    WorldAABB.Min = Min;
    WorldAABB.Max = Max;
}

bool UCapsuleComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    return false;
}

EPrimitiveType UCapsuleComponent::GetPrimitiveType() const
{
    return EPrimitiveType::EPT_Capsule;
}
