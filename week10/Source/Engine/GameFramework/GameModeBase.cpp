#include "GameFramework/GameModeBase.h"

#include "Component/CameraComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Serialization/PrefabManager.h"

DEFINE_CLASS(AGameModeBase, AActor)
REGISTER_FACTORY(AGameModeBase)

void AGameModeBase::SetPlayerControllerClass(const FString& InClassName)
{
    if (!InClassName.empty())
    {
        PlayerControllerClass = InClassName;
    }
}

void AGameModeBase::SetDefaultPawnClass(const FString& InClassName)
{
    if (!InClassName.empty())
    {
        DefaultPawnClass = InClassName;
    }
}

void AGameModeBase::SetDefaultPawnPrefabPath(const FString& InPrefabPath)
{
    DefaultPawnPrefabPath = InPrefabPath;
}

APlayerController* AGameModeBase::EnsurePlayerController(
    const FViewportCamera* SourceCamera,
    uint32 RuntimeCameraWidth,
    uint32 RuntimeCameraHeight,
    FViewportCamera* FallbackCamera)
{
    if (PlayerController)
    {
        return PlayerController;
    }

    UWorld* World = GetFocusedWorld();
    if (!World)
    {
        return nullptr;
    }

    AActor* ControllerActor = World->SpawnActorByTypeName(PlayerControllerClass);
    PlayerController = Cast<APlayerController>(ControllerActor);
    if (!PlayerController)
    {
        UE_LOG_ERROR("[GameMode] Failed to spawn PlayerController class: %s", PlayerControllerClass.c_str());
        if (ControllerActor)
        {
            World->DestroyActor(ControllerActor);
        }
        if (FallbackCamera)
        {
            World->SetActiveCamera(FallbackCamera);
        }
        return nullptr;
    }

    PlayerController->SetFName(FName(PlayerControllerClass));
    PlayerController->ConfigureRuntimeCameraFromViewport(SourceCamera);

    FViewportCamera* RuntimeCamera = PlayerController->GetRuntimeCamera();
    if (RuntimeCamera)
    {
        if (RuntimeCameraWidth > 0 && RuntimeCameraHeight > 0)
        {
            RuntimeCamera->OnResize(RuntimeCameraWidth, RuntimeCameraHeight);
        }
        World->SetActiveCamera(RuntimeCamera);
    }
    else if (FallbackCamera)
    {
        World->SetActiveCamera(FallbackCamera);
    }

    UE_LOG("[GameMode] Spawned PlayerController class: %s", PlayerControllerClass.c_str());
    return PlayerController;
}

APawn* AGameModeBase::SpawnDefaultPawn()
{
    UWorld* World = GetFocusedWorld();
    if (!World)
    {
        return nullptr;
    }

    if (DefaultPawn)
    {
        bool bPawnStillInWorld = false;
        for (AActor* Actor : World->GetActors())
        {
            if (Actor == DefaultPawn)
            {
                bPawnStillInWorld = true;
                break;
            }
        }

        if (bPawnStillInWorld)
        {
            return DefaultPawn;
        }

        UE_LOG_WARNING("[GameMode] Cached DefaultPawn was no longer in the world. Respawning default pawn.");
        DefaultPawn = nullptr;
    }

    FVector SpawnLocation = FVector::ZeroVector;
    FVector SpawnRotation = FVector::ZeroVector;
    if (AActor* Start = FindPlayerStart())
    {
        SpawnLocation = Start->GetActorLocation();
        SpawnRotation = Start->GetActorRotation();
    }

    if (!DefaultPawnPrefabPath.empty())
    {
        AActor* PrefabActor = FPrefabManager::SpawnActorFromPrefab(World, DefaultPawnPrefabPath);
        if (APawn* PrefabPawn = Cast<APawn>(PrefabActor))
        {
            DefaultPawn = PrefabPawn;
            ApplyPlayerStartTransform(DefaultPawn, SpawnLocation, SpawnRotation);
            UE_LOG("[GameMode] Spawned DefaultPawnPrefab: %s", DefaultPawnPrefabPath.c_str());
            return DefaultPawn;
        }

        UE_LOG_ERROR("[GameMode] DefaultPawnPrefab must contain an APawn root actor: %s",
            DefaultPawnPrefabPath.c_str());
        if (PrefabActor)
        {
            World->DestroyActor(PrefabActor);
        }
        UE_LOG_WARNING("[GameMode] Falling back to DefaultPawnClass: %s", DefaultPawnClass.c_str());
    }

    AActor* Actor = World->SpawnActorByTypeName(DefaultPawnClass);
    DefaultPawn = Cast<APawn>(Actor);
    if (!DefaultPawn)
    {
        UE_LOG_ERROR("[GameMode] DefaultPawnClass must derive from APawn: %s", DefaultPawnClass.c_str());
        if (Actor)
        {
            World->DestroyActor(Actor);
        }
        return nullptr;
    }

    DefaultPawn->SetFName(FName(DefaultPawnClass));
    ApplyPlayerStartTransform(DefaultPawn, SpawnLocation, SpawnRotation);
    UE_LOG("[GameMode] Spawned DefaultPawnClass: %s", DefaultPawnClass.c_str());
    return DefaultPawn;
}

APlayerController* AGameModeBase::BootstrapPlayer(
    const FViewportCamera* SourceCamera,
    uint32 RuntimeCameraWidth,
    uint32 RuntimeCameraHeight,
    FViewportCamera* FallbackCamera)
{
    APlayerController* Controller = EnsurePlayerController(
        SourceCamera,
        RuntimeCameraWidth,
        RuntimeCameraHeight,
        FallbackCamera);
    if (!Controller)
    {
        return nullptr;
    }

    if (!Controller->GetPawn())
    {
        if (APawn* Pawn = SpawnDefaultPawn())
        {
            Controller->Possess(Pawn);
            UE_LOG("[GameMode] PlayerController possessed pawn: %s",
                Pawn->GetFName().ToString().c_str());
        }
        else
        {
            UE_LOG_ERROR("[GameMode] Failed to spawn default pawn. PlayerController will remain unpossessed.");
        }
    }

    return Controller;
}

AActor* AGameModeBase::FindPlayerStart() const
{
    const UWorld* World = GetFocusedWorld();
    if (!World)
    {
        return nullptr;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (Actor && Actor->IsA<APlayerStart>())
        {
            return Actor;
        }
    }
    return nullptr;
}

void AGameModeBase::ApplyPlayerStartTransform(APawn* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation) const
{
    if (!Pawn)
    {
        return;
    }

    if (UCameraComponent* Camera = Pawn->FindComponent<UCameraComponent>())
    {
        Pawn->SetActorLocation(SpawnLocation);
        if (Pawn->GetRootComponent() == Camera)
        {
            Camera->SetRelativeRotation(FVector(0.0f, SpawnRotation.Y, SpawnRotation.Z));
        }
        else
        {
            Pawn->SetActorRotation(FVector(0.0f, 0.0f, SpawnRotation.Z));
            Camera->SetRelativeRotation(FVector(0.0f, SpawnRotation.Y, 0.0f));
        }
        return;
    }

    Pawn->SetActorLocation(SpawnLocation);
    Pawn->SetActorRotation(FVector(0.0f, 0.0f, SpawnRotation.Z));
}
