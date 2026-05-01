#include "SphereComponent.h"
IMPLEMENT_CLASS(USphereComponent, UShapeComponent)

USphereComponent::USphereComponent()
    : SphereRadius(50.0f),
      SphereCollision(FSphere(FVector(0.0f, 0.0f, 0.0f), 50.0f))
{
     //ShapeColor = FColor(0, 255, 0);
}

FShapeProxy* USphereComponent::CreateShapeProxy()
{
    return nullptr;
}
