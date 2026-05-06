#pragma once

#include "GameFramework/AActor.h"

class APlayerController;
class FViewportCamera;

class AGameModeBase : public AActor
{
public:
    DECLARE_CLASS(AGameModeBase, AActor)

    void SetPlayerControllerClass(const FString& InClassName);
    const FString& GetPlayerControllerClass() const { return PlayerControllerClass; }

    APlayerController* GetPlayerController() const { return PlayerController; }
    APlayerController* EnsurePlayerController(
        const FViewportCamera* SourceCamera = nullptr,
        uint32 RuntimeCameraWidth = 0,
        uint32 RuntimeCameraHeight = 0,
        FViewportCamera* FallbackCamera = nullptr);

private:
    FString PlayerControllerClass = "APlayerController";
    APlayerController* PlayerController = nullptr;
};
