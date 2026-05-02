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

void USphereComponent::OnTransformDirty()
{
    UShapeComponent::OnTransformDirty();

    const FMatrix& WorldMat = GetWorldMatrix();

    SphereCollision.Sphere.Center = WorldMat.GetLocation();

    FVector Scale = WorldMat.GetScale();

    float MaxScale = std::max({ Scale.X, Scale.Y, Scale.Z });

    SphereCollision.Sphere.Radius = SphereRadius * MaxScale;
}
