#include "CapsuleComponent.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_COMPONENT_CLASS(UCapsuleComponent, UShapeComponent, EEditorComponentCategory::Shapes)

UCapsuleComponent::UCapsuleComponent()
    : CapsuleHalfHeight(1.0f),
      CapsuleRadius(1.0f),
      CapsuleCollision(FVector(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, FVector(0.0f, 0.0f, 1.0f))
{
}

void UCapsuleComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << CapsuleHalfHeight;
    Ar << CapsuleRadius;
    Ar << CapsuleCollision;
}

void UCapsuleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "CapsuleHalfHeight", EPropertyType::Float, &CapsuleHalfHeight, 0.0f, 100.f, 0.01f });
    OutProps.push_back({ "CapsuleRadius", EPropertyType::Float, &CapsuleRadius, 0.0f, 100.0f, 0.01f });
}

void UCapsuleComponent::PostEditProperty(const char* PropertyName)
{
    SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
    UShapeComponent::PostEditProperty(PropertyName);
}

void UCapsuleComponent::SetCapsuleSize(float Radius, float HalfHeight)
{
    CapsuleRadius = std::max(0.0f, Radius);
    CapsuleHalfHeight = std::max(0.0f, HalfHeight);
    OnTransformDirty();
}

void UCapsuleComponent::SetCapsuleRadius(float Radius)
{
    SetCapsuleSize(Radius, CapsuleHalfHeight);
}

void UCapsuleComponent::SetCapsuleHalfHeight(float HalfHeight)
{
    SetCapsuleSize(CapsuleRadius, HalfHeight);
}

void UCapsuleComponent::OnComponentOverlap(UPrimitiveComponent* Other) const
{
    if (bGenerateOverlapEvents)
        return;
}

void UCapsuleComponent::OnTransformDirty()
{
    UShapeComponent::OnTransformDirty();

    const FMatrix& WorldMat = GetWorldMatrix();
    CapsuleCollision.Center = WorldMat.GetLocation();

    FVector Scale = WorldMat.GetScale();

    const float MaxScaleXY = std::max(Scale.X, Scale.Y);
    CapsuleCollision.Radius = CapsuleRadius * MaxScaleXY;
    CapsuleCollision.HalfHeight = CapsuleHalfHeight * Scale.Z;

    FVector LocalUp(0.0f, 0.0f, 1.0f);
    CapsuleCollision.UpVector = WorldMat.TransformVector(LocalUp).Normalized();

    NotifyCollisionShapeChanged();
}

void UCapsuleComponent::UpdateWorldAABB() const
{
    const FVector Up = CapsuleCollision.UpVector.Normalized();
    const float Radius = CapsuleCollision.Radius;
    const float HalfHeight = CapsuleCollision.HalfHeight;

    const FVector Extent(
        std::abs(Up.X) * HalfHeight + Radius,
        std::abs(Up.Y) * HalfHeight + Radius,
        std::abs(Up.Z) * HalfHeight + Radius);

    WorldAABBMinLocation = CapsuleCollision.Center - Extent;
    WorldAABBMaxLocation = CapsuleCollision.Center + Extent;
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}
