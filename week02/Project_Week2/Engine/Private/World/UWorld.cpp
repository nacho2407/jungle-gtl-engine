#include "World/UWorld.h"


UWorld::UWorld() : bHasBegunPlay(false)
{
}

UWorld::~UWorld()
{
}

void UWorld::Clear()
{
    for (const auto& actor : Actors)
    {
        Destroy(actor);
    }

    Actors.clear();
}

void UWorld::AddActor(AActor* InActor)
{
    if (!InActor)
    {
        return;
    }

    Actors.push_back(InActor);
}

void UWorld::RemoveActor(AActor* InActor)
{
    int targetIndex = 0;
    for (targetIndex = 0; targetIndex < Actors.size(); targetIndex++)
    {
        Actors[targetIndex] == InActor;
        break;
    }

    auto target = Actors[targetIndex];
    Actors[targetIndex] = Actors.back();
    Actors.pop_back();
    Destroy(target);
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
    for (AActor* Actor : Actors)
    {
        if (!Actor)
        {
            continue;
        }

        const TArray<UActorComponent*>& Components = Actor->GetComponents();
        
        for (UActorComponent* Component : Components)
        {
            // @@@ 아직 이거는 Primitive 외에 확장성은 고려하지 않은건가? <- BuildScene이라 필요없네ㅎ,ㅎ
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

//ACamera* UWorld::GetCameraActor()
//{
//    return Camera;
//}

//AActor* UWorld::GetWorldAxisActor()
//{
//    return WorldAxisActor;
//}
//
//AActor* UWorld::GetGridActor()
//{
//    return GridActor;
//}
//
//AActor* UWorld::GetGizmoActor()
//{
//    return GizmoActor;
//}

const TArray<AActor*>& UWorld::GetActors() const
{
    return Actors;
}