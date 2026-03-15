#include "Component/USphereComponent.h"

USphereComponent::USphereComponent()
    : Radius(50.0f)
{
}

USphereComponent::~USphereComponent()
{
}

void USphereComponent::BeginPlay()
{
    UPrimitiveComponent::BeginPlay();
}

void USphereComponent::TickComponent(float DeltaTime)
{
    UPrimitiveComponent::TickComponent(DeltaTime);
}

const char* USphereComponent::GetObjClassName() const
{
    return "USphereComponent";
}

void USphereComponent::SetRadius(float InRadius)
{
    Radius = InRadius;
}

float USphereComponent::GetRadius() const
{
    return Radius;
}
