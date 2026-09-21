#include "Actor/APrimitiveActor.h"
#include "Component/UPrimitiveComponent.h"
#include "Component/USceneComponent.h"

APrimitiveActor::APrimitiveActor()
    : AActor()
{
}

APrimitiveActor::~APrimitiveActor()
{
}

UPrimitiveComponent* APrimitiveActor::GetPrimitiveComponent() const
{
    return PrimitiveComponent;
}

void APrimitiveActor::SetPrimitiveComponent(UPrimitiveComponent* InPrimitiveComponent)
{
    PrimitiveComponent = InPrimitiveComponent;

    if (PrimitiveComponent)
    {
        AddComponent(PrimitiveComponent);
        SetRootComponent(PrimitiveComponent);
    }
}