#include "CapsuleComponent.h"
IMPLEMENT_CLASS(UCapsuleComponent, UShapeComponent)

UCapsuleComponent::UCapsuleComponent()
    : CapsuleHalfHeight(50.0f),
      CapsuleRadius(22.0f),    
      CapsuleCollision(FVector(0.0f, 0.0f, 0.0f), 50.0f, 22.0f, FVector(0.0f, 0.0f, 1.0f))
{
}

FShapeProxy* UCapsuleComponent::CreateShapeProxy()
{
    return nullptr;
}
