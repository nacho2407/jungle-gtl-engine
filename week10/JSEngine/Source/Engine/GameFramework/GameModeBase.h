#pragma once

#include "GameFramework/AActor.h"

class APawn;
class APlayerController;
class FViewportCamera;

class AGameModeBase : public AActor
{
public:
    DECLARE_CLASS(AGameModeBase, AActor)

    void SetPlayerControllerClass(const FString& InClassName);
    const FString& GetPlayerControllerClass() const { return PlayerControllerClass; }
    void SetDefaultPawnClass(const FString& InClassName);
    const FString& GetDefaultPawnClass() const { return DefaultPawnClass; }
    void SetDefaultPawnPrefabPath(const FString& InPrefabPath);
    const FString& GetDefaultPawnPrefabPath() const { return DefaultPawnPrefabPath; }

    APlayerController* GetPlayerController() const { return PlayerController; }
    APawn* GetDefaultPawn() const { return DefaultPawn; }
    APawn* SpawnDefaultPawn();
    APlayerController* BootstrapPlayer(
        const FViewportCamera* SourceCamera = nullptr,
        uint32 RuntimeCameraWidth = 0,
        uint32 RuntimeCameraHeight = 0,
        FViewportCamera* FallbackCamera = nullptr);

private:
    APlayerController* EnsurePlayerController(
        const FViewportCamera* SourceCamera = nullptr,
        uint32 RuntimeCameraWidth = 0,
        uint32 RuntimeCameraHeight = 0,
        FViewportCamera* FallbackCamera = nullptr);
    AActor* FindPlayerStart() const;
    void ApplyPlayerStartTransform(APawn* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation) const;

private:
    FString PlayerControllerClass = "APlayerController";
    FString DefaultPawnClass = "ADefaultPawn";
    FString DefaultPawnPrefabPath;
    APlayerController* PlayerController = nullptr;
    APawn* DefaultPawn = nullptr;
};
