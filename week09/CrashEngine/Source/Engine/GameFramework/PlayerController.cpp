#include "GameFramework/PlayerController.h"

#include "CameraManage/APlayerCameraManager.h"
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

    UWorld* World = GetWorld();
    CameraManager->SetViewTarget(PossessedActor && World ? World->GetActiveCamera() : nullptr);
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
