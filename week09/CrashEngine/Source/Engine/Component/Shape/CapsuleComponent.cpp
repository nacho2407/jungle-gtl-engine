#include "CapsuleComponent.h"
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
    CapsuleHalfHeight = std::max(0.0f, CapsuleHalfHeight);
    CapsuleRadius = std::max(0.0f, CapsuleRadius);

    UShapeComponent::PostEditProperty(PropertyName);
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

    CapsuleCollision.Center = WorldMat.GetLocation(); //

    FVector Scale = WorldMat.GetScale(); //

    float MaxScaleXY = std::max(Scale.X, Scale.Y);
    CapsuleCollision.Radius = CapsuleRadius * MaxScaleXY;

    CapsuleCollision.HalfHeight = CapsuleHalfHeight * Scale.Z;

    FVector LocalUp(0.0f, 0.0f, 1.0f);

    CapsuleCollision.UpVector = WorldMat.TransformVector(LocalUp).Normalized(); //
}

