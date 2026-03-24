#pragma once

#include "Core/CoreMinimal.h"
#include "Camera/ViewportCamera.h"
#include "Engine/ViewPort/ViewportController.h"

class FViewportNavigationController : public Engine::Viewport::IViewportController
{
  public:
    void SetCamera(FViewportCamera* InCamera) { ViewportCamera = InCamera; }

    void Tick(float DeltaTime);

    void MoveForward(float Value, float DeltaTime);
    void MoveRight(float Value, float DeltaTime);
    void MoveUp(float Value, float DeltaTime);

    void AddYawInput(float Value);
    void AddPitchInput(float Value);

    void SetRotating(bool bInRotating);
    bool IsRotating() { return bRotating; }

    void ModifyFOV(float DeltaFOV);
    void AdjustMoveSpeed(float Step)
    {
        MoveSpeed = FMath::Clamp(MoveSpeed + Step, 10.0f, 2000.0f);
    }

    float GetMoveSpeed() const { return MoveSpeed; }
    void SetMoveSpeed(float InMoveSpeed)
    {
        MoveSpeed = FMath::Clamp(InMoveSpeed, 10.0f, 2000.0f);
    }

    float GetRotationSpeed() const { return RotationSpeed; }
    void SetRotationSpeed(float InRotationSpeed)
    {
        RotationSpeed = FMath::Clamp(InRotationSpeed, 0.01f, 10.0f);
    }

    /* Orbiting */
    void SetSelectionController(class FViewportSelectionController* InController)
    {
        SelectionController = InController;
    }
  
    void       BeginOrbit();
    FVector ResolveOrbitPivot() const;
    void       UpdateOrbitCamera();
    void       EndOrbit();

  private:
    void UpdateCameraRotation();
    void EnsureTargetLocationInitialized();

  private:
    FViewportCamera* ViewportCamera = nullptr;
    class FViewportSelectionController* SelectionController = nullptr;

    /* Movement */
    float MoveSpeed = 100.0f;
    float RotationSpeed = 0.3f;
    float LocationLerpSpeed = 12.0f;

    float Yaw = 0.0f;
    float Pitch = 0.0f;

    FVector TargetLocation = FVector::Zero();
    bool bHasTargetLocation = false;

    bool bRotating = false;

    /* Orbiting */
    bool bOrbiting = false;
    FVector OrbitPivot = FVector::Zero();
    float   OrbitRadius = 0.0f;
    float   DefaultOrbitRadius = 300.0f;
};
