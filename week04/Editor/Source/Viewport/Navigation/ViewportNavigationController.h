#pragma once

#include "Core/CoreMinimal.h"
#include "Camera/ViewportCamera.h"
#include "Engine/ViewPort/ViewportController.h"

class AActor;

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

    void ModifyFOVorOrthoHeight(float Delta);

    void ModifyFOV(float DeltaFOV);
    void ModifyOrthoHeight(float DeltaHeight);
    void AdjustMoveSpeed(float Step) { MoveSpeed = FMath::Clamp(MoveSpeed + Step, 10.0f, 2000.0f); }

    float GetMoveSpeed() const { return MoveSpeed; }
    void  SetMoveSpeed(float InMoveSpeed) { MoveSpeed = FMath::Clamp(InMoveSpeed, 10.0f, 2000.0f); }

    float GetRotationSpeed() const { return RotationSpeed; }
    void  SetRotationSpeed(float InRotationSpeed)
    {
        RotationSpeed = FMath::Clamp(InRotationSpeed, 0.01f, 10.0f);
    }

    void SetRotationLocked(bool bInLocked) { bRotationLocked = bInLocked; }
    void SetTranslationLocked(bool bInLocked) { bTranslationLocked = bInLocked; }
    void ToggleHasTargetLocation() { bHasTargetLocation = !bHasTargetLocation; }
    void ResetTargetLocation(const FVector& InLocation) { TargetLocation = InLocation; bHasTargetLocation = true; }

    /* Orbiting */
    void SetSelectionController(class FViewportSelectionController* InController)
    {
        SelectionController = InController;
    }

    void    BeginOrbit();
    FVector ResolveOrbitPivot() const;
    void    UpdateOrbitCamera();
    void    EndOrbit();

    /* Dolly */
    void Dolly(float Value);

    /* Pan */
    void BeginPanning();
    void EndPanning();
    void AddPanInput(float DeltaX, float DeltaY);

    bool IsPanning() const { return bPanning; }

    float GetPanSpeed() const { return PanSpeed; }
    void  SetPanSpeed(float InPanSpeed) { PanSpeed = FMath::Clamp(InPanSpeed, 0.01f, 10.0f); }
    
    /* Focus */
    void FocusActors();
    void FocusActors(const TArray<AActor*>& Actors);
    
    /* With Gizmo */
    void TranslateWithGizmoDelta(const FVector& Delta);
    float GetGizmoFollowSpeedScale() const { return GizmoFollowSpeedScale; }
    void  SetGizmoFollowSpeedScale(float InScale)
    {
        GizmoFollowSpeedScale = FMath::Clamp(InScale, 0.01f, 5.0f);
    }
    
  private:
    void UpdateCameraRotation();
    void EnsureTargetLocationInitialized();

  private:
    FViewportCamera*                    ViewportCamera = nullptr;
    class FViewportSelectionController* SelectionController = nullptr;

    /* Rotation / Translation Lock */
    bool bRotationLocked    = false;
    bool bTranslationLocked = false;

    /* Movement */
    float MoveSpeed = 100.0f;
    float RotationSpeed = 0.3f;
    float LocationLerpSpeed = 12.0f;

    float Yaw = 0.0f;
    float Pitch = 0.0f;

    FVector TargetLocation = FVector::Zero();
    bool    bHasTargetLocation = false;

    bool bRotating = false;

    /* Orbiting */
    bool    bOrbiting = false;
    FVector OrbitPivot = FVector::Zero();
    float   OrbitRadius = 0.0f;
    float   DefaultOrbitRadius = 300.0f;

    /* Dolly */
    float DollySpeed = 1.0f;
    float MinOrbitRadius = 10.0f;

    /* Panning */
    bool  bPanning = false;
    float PanSpeed = 0.1f;

    float GizmoFollowSpeedScale = 0.5f;
};
