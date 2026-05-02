#include "SphereComponent.h"
IMPLEMENT_COMPONENT_CLASS(USphereComponent, UShapeComponent, EEditorComponentCategory::Shapes)

USphereComponent::USphereComponent()
    : SphereRadius(1.0f),
      SphereCollision(FSphere(FVector(0.0f, 0.0f, 0.0f), 1.0f))
{
}

void USphereComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << SphereRadius;
    Ar << SphereCollision;
}

void USphereComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Radius", EPropertyType::Float, &SphereRadius, 0.0f, 100.0f, 0.1f });
}

void USphereComponent::PostEditProperty(const char* PropertyName)
{
    SphereRadius = std::max(0.0f, SphereRadius);
    UShapeComponent::PostEditProperty(PropertyName);
}

void USphereComponent::OnComponentOverlap(UPrimitiveComponent* Other) const
{
    if (bGenerateOverlapEvents)
        return;
}

void USphereComponent::OnTransformDirty()
{
    UShapeComponent::OnTransformDirty();

    const FMatrix& WorldMat = GetWorldMatrix();

    SphereCollision.Sphere.Center = WorldMat.GetLocation();

    FVector Scale = WorldMat.GetScale();

    float MaxScale = std::max({ Scale.X, Scale.Y, Scale.Z });

    SphereCollision.Sphere.Radius = SphereRadius * MaxScale;
}
