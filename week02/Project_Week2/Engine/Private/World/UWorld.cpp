#include "World/UWorld.h"

#include "Actor/ASphere.h"
#include "Actor/ACube.h"
#include "Actor/ATorus.h"
#include "Actor/ATriangle.h"

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
    bHasBegunPlay = false;
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
    for (int targetIndex = 0; targetIndex < Actors.size(); targetIndex++)
    {
        if (Actors[targetIndex] == InActor)
        {
            AActor* Target = Actors[targetIndex];
            Actors[targetIndex] = Actors.back();
            Actors.pop_back();
            Destroy(Target);
            break;
        }
    }
}

void UWorld::Destroy(AActor* InActor)
{
    RemoveActor(InActor);
}

AActor* UWorld::SpawnMeshActor(
    ESpawnMeshType Type,
    const FVector& Location,
    const FVector& Rotation,
    const FVector& Scale
)
{
    AActor* Actor = nullptr;

    switch (Type)
    {
    case ESpawnMeshType::Sphere:
        Actor = NewObject<ASphere>();
        break;
    case ESpawnMeshType::Cube:
        Actor = NewObject<ACube>();
        break;
    case ESpawnMeshType::Torus:
        Actor = NewObject<ATorus>();
        break;
    case ESpawnMeshType::Triangle:
        Actor = NewObject<ATriangle>();
        break;
    default:
        return nullptr;
    }

    Actor->GetRootComponent()->SetRelativeLocation(Location);
    Actor->GetRootComponent()->SetRelativeRotation(Rotation);
    Actor->GetRootComponent()->SetRelativeScale(Scale);

    AddActor(Actor);
    return Actor;
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
            if (dynamic_cast<AGizmoActor*>(Actor))
            {
                Item.bIsGizmo = true;
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
