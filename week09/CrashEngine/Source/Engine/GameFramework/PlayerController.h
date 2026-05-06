#pragma once

#include "GameFramework/AActor.h"

class APlayerCameraManager;

class APlayerController : public AActor
{
public:
    DECLARE_CLASS(APlayerController, AActor)

    APlayerController();

    void BeginPlay() override;
    void Tick(float DeltaTime) override;

    AActor* GetPossessedActor() const { return PossessedActor; }
    bool Possess(AActor* Actor);
    void UnPossess();
    bool TryAutoPossessPlayer();

    APlayerCameraManager* GetCameraManager() const { return CameraManager; }

private:
    AActor* PossessedActor = nullptr;
    APlayerCameraManager* CameraManager = nullptr;
};
