#include "GameFramework/GameModeBase.h"

#include "Core/Logging/Log.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"

DEFINE_CLASS(AGameModeBase, AActor)
REGISTER_FACTORY(AGameModeBase)

void AGameModeBase::SetPlayerControllerClass(const FString& InClassName)
{
    if (!InClassName.empty())
    {
        PlayerControllerClass = InClassName;
    }
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

    PlayerController = Cast<APlayerController>(World->SpawnActorByTypeName(PlayerControllerClass));
    if (!PlayerController)
    {
        UE_LOG_ERROR("[GameMode] Failed to spawn PlayerController class: %s", PlayerControllerClass.c_str());
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
