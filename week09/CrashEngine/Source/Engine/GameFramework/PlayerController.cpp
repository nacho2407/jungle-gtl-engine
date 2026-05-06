#include "GameFramework/PlayerController.h"

#include "CameraManage/APlayerCameraManager.h"
#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(APlayerController, AActor)

APlayerController::APlayerController() = default;

void APlayerController::BeginPlay()
{
    AActor::BeginPlay();

    if (!CameraManager)
    {
        if (UWorld* World = GetWorld())
        {
            CameraManager = World->SpawnActor<APlayerCameraManager>();
        }
    }

    TryAutoPossessPlayer();
}

void APlayerController::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);

    if (PossessedActor && PossessedActor->GetWorld() != GetWorld())
    {
        UnPossess();
    }

    if (!PossessedActor)
    {
        TryAutoPossessPlayer();
    }

    if (!CameraManager)
    {
        return;
    }

    UCameraComponent* Camera = ResolveViewTargetCamera();
    CameraManager->SetViewTarget(Camera);
    CameraManager->UpdateCamera(DeltaTime);
}

bool APlayerController::Possess(AActor* Actor)
{
    if (!Actor || Actor == this || Actor == CameraManager)
    {
        return false;
    }

    PossessedActor = Actor;
    return true;
}

void APlayerController::UnPossess()
{
    PossessedActor = nullptr;
}

bool APlayerController::TryAutoPossessPlayer()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (Actor && Actor != this && Actor != CameraManager && Actor->HasTag(FName("Player")))
        {
            return Possess(Actor);
        }
    }

    return false;
}

UCameraComponent* APlayerController::ResolveViewTargetCamera() const
{
    UCameraComponent* FirstCamera = nullptr;
    if (PossessedActor)
    {
        for (UActorComponent* Component : PossessedActor->GetComponents())
        {
            UCameraComponent* Camera = Cast<UCameraComponent>(Component);
            if (!Camera)
            {
                continue;
            }

            if (Camera->IsMainCamera())
            {
                if (UWorld* World = GetWorld())
                {
                    World->SetActiveCamera(Camera);
                }
                return Camera;
            }

            if (!FirstCamera)
            {
                FirstCamera = Camera;
            }
        }
    }

    if (FirstCamera)
    {
        if (UWorld* World = GetWorld())
        {
            World->SetActiveCamera(FirstCamera);
        }
        return FirstCamera;
    }

    UWorld* World = GetWorld();
    return World ? World->GetActiveCamera() : nullptr;
}
