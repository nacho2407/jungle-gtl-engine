#include "World/UWorld.h"


UWorld::UWorld() : bHasBegunPlay(false)
{
    // 카메라 액터
    //Camera = NewObject<ACamera>();
    //WorldAxisActor = NewObject<AAxisActor>();
    //GizmoActor = NewObject<AGizmoActor>();
    //GridActor = NewObject<AGridActor>();

    //GridActor = NewObject<AActor>();

    //MeshComp = NewObject<UStaticMeshComponent>("UStaticMeshComponent");
    //MeshComp->SetStaticMesh(BuiltInMeshFactory::CreateGridMesh(20, 1.0f));
    //MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

    //GridActor->AddComponent(MeshComp);
    //GridActor->SetRootComponent(MeshComp);

    //GizmoActor = NewObject<AActor>();

    //auto GizmoArrowMesh = BuiltInMeshFactory::CreateGizmoArrowMesh();
    //UStaticMeshComponent* GizmoXComp = NewObject<UStaticMeshComponent>();
    //UStaticMeshComponent* GizmoYComp = NewObject<UStaticMeshComponent>();
    //UStaticMeshComponent* GizmoZComp = NewObject<UStaticMeshComponent>();

    //GizmoXComp->SetStaticMesh(GizmoArrowMesh);
    //GizmoYComp->SetStaticMesh(GizmoArrowMesh);
    //GizmoZComp->SetStaticMesh(GizmoArrowMesh);

    //GizmoXComp->SetRelativeLocation(FVector::ZeroVector);
    //GizmoYComp->SetRelativeLocation(FVector::ZeroVector);
    //GizmoZComp->SetRelativeLocation(FVector::ZeroVector);

    //// arrow mesh가 +X 방향 기준이라고 가정
    //GizmoXComp->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
    //GizmoYComp->SetRelativeRotation(FVector(0.0f, 0.0f, 1.5707963f));
    //GizmoZComp->SetRelativeRotation(FVector(0.0f, -1.5707963f, 0.0f));

    //GizmoXComp->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));
    //GizmoYComp->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));
    //GizmoZComp->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));

    //GizmoXComp->SetRenderColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    //GizmoYComp->SetRenderColor(FVector4(0.0f, 1.0f, 0.0f, 1.0f));
    //GizmoZComp->SetRenderColor(FVector4(0.0f, 0.45f, 1.0f, 1.0f));

    //GizmoXComp->SetVisibility(false);
    //GizmoYComp->SetVisibility(false);
    //GizmoZComp->SetVisibility(false);

    //GizmoActor->AddComponent(GizmoXComp);
    //GizmoActor->AddComponent(GizmoYComp);
    //GizmoActor->AddComponent(GizmoZComp);
    //GizmoActor->SetRootComponent(GizmoXComp);
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