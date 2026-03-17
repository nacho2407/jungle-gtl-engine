#include "Actor/APrimitiveActor.h"
#include "Component/UPrimitiveComponent.h"
#include "Component/USceneComponent.h"

APrimitiveActor::APrimitiveActor()
    : AActor()
{
}

//APrimitiveActor::APrimitiveActor(const FUObjectInitializer& ObjectInitializer)
//    : AActor(ObjectInitializer)
//{
//}

APrimitiveActor::~APrimitiveActor()
{
}

const char* APrimitiveActor::GetObjClassName() const
{
    return "APrimitiveActor";
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