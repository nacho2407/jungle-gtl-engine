#include "BoxComponent.h"
#include "Serialization/Archive.h"

#include "GameFrameWork/World.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_COMPONENT_CLASS(UBoxComponent, UShapeComponent, EEditorComponentCategory::Shapes)

UBoxComponent::UBoxComponent()
    : BoxExtent(1.0f, 1.0f, 1.0f),
      BoxCollision(FOBB(FVector(0.0f, 0.0f, 0.0f), FVector(0.5f, 0.5f, 0.5f), FRotator(0.0f, 0.0f, 0.0f)))
{
}

void UBoxComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << BoxCollision;
    Ar << BoxExtent;
}

void UBoxComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "BoxExtent", EPropertyType::Vec3, &BoxExtent, 0.f, 100.0f, 0.1f });
}

void UBoxComponent::PostEditProperty(const char* PropertyName)
{
    SetBoxExtent(BoxExtent);
    UShapeComponent::PostEditProperty(PropertyName);
}

void UBoxComponent::SetBoxExtent(const FVector& Extent)
{
    BoxExtent.X = std::max(0.0f, Extent.X);
    BoxExtent.Y = std::max(0.0f, Extent.Y);
    BoxExtent.Z = std::max(0.0f, Extent.Z);
    OnTransformDirty();
}

void UBoxComponent::OnComponentOverlap(UPrimitiveComponent* Other) const
{
    if (bGenerateOverlapEvents)
        return;
}

void UBoxComponent::OnTransformDirty()
{
    UShapeComponent::OnTransformDirty();

    FMatrix CollisionWorldMat = FMatrix::MakeScaleMatrix(BoxExtent) * GetWorldMatrix();
    BoxCollision.Bounds.UpdateAsOBB(CollisionWorldMat);

    NotifyCollisionShapeChanged();
}

void UBoxComponent::UpdateWorldAABB() const
{
    const FOBB& OBB = BoxCollision.Bounds;

    const FVector AxisX = OBB.Rotation.GetForwardVector();
    const FVector AxisY = OBB.Rotation.GetRightVector();
    const FVector AxisZ = OBB.Rotation.GetUpVector();

    const FVector Extent(
        std::abs(AxisX.X) * OBB.Extent.X + std::abs(AxisY.X) * OBB.Extent.Y + std::abs(AxisZ.X) * OBB.Extent.Z,
        std::abs(AxisX.Y) * OBB.Extent.X + std::abs(AxisY.Y) * OBB.Extent.Y + std::abs(AxisZ.Y) * OBB.Extent.Z,
        std::abs(AxisX.Z) * OBB.Extent.X + std::abs(AxisY.Z) * OBB.Extent.Y + std::abs(AxisZ.Z) * OBB.Extent.Z);

    WorldAABBMinLocation = OBB.Center - Extent;
    WorldAABBMaxLocation = OBB.Center + Extent;
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}
