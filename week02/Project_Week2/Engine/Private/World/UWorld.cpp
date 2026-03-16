#include "World/UWorld.h"
#include "Actor/AActor.h"
#include "Component/UActorComponent.h"
#include "Component/UPrimitiveComponent.h"
#include "World/FScene.h"

UWorld::UWorld()
    : bHasBegunPlay(false)
{
}

UWorld::~UWorld()
{
}

void UWorld::AddActor(AActor* InActor)
{
    if (!InActor)
    {
        return;
    }

    Actors.push_back(InActor);
}

void UWorld::BeginPlay()
{
    if (bHasBegunPlay)
    {
        return;
    }

    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            Actor->BeginPlay();
        }
    }

    bHasBegunPlay = true;
}

void UWorld::Tick(float DeltaTime)
{
    if (!bHasBegunPlay)
    {
        BeginPlay();
    }

    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            Actor->Tick(DeltaTime);
        }
    }
}

void UWorld::BuildScene(FScene& OutScene) const
{
    OutScene.Clear();

    for (AActor* Actor : Actors)
    {
        if (!Actor)
        {
            continue;
        }

        const std::vector<UActorComponent*>& Components = Actor->GetComponents();

        for (UActorComponent* Component : Components)
        {
            // @@@ 아직 이거는 Primitive 외에 확장성은 고려하지 않은건가?
            UPrimitiveComponent* PrimitiveComponent = dynamic_cast<UPrimitiveComponent*>(Component);
            if (!PrimitiveComponent)
            {
                continue;
            }

            if (!PrimitiveComponent->IsVisible())
            {
                continue;
            }

            FRenderItem Item = PrimitiveComponent->CreateRenderItem();
            if (Item.Mesh == nullptr)
            {
                continue;
            }
            OutScene.RenderItems.push_back(Item);
        }
    }
}

const std::vector<AActor*>& UWorld::GetActors() const
{
    return Actors;
}