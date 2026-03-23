#pragma once
#pragma once

#include "Core/CoreMinimal.h"
#include "Camera/ViewportCamera.h"
#include "Engine/ViewPort/ViewportController.h"

/*
        카메라/뷰 이동 계층
        이동 자체, Zoom, Focus Selected, Orbit, View Mode 전환의 일부 등을 함께 관리하는 계층입니다.
    ViewportInputContext에서 입력을 받아서 카메라 이동과 관련된 입력이 들어오면 이 계층에서
   처리하도록 합니다.
*/

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

  private:
    void UpdateCameraRotation();
    void EnsureTargetLocationInitialized();

  private:
    FViewportCamera* ViewportCamera = nullptr;

    float MoveSpeed = 100.f;   // 이동 속도
    const float RotationSpeed = 0.3f; // 마우스 드래그 회전 감도
    float LocationLerpSpeed = 12.0f;
    //float ZoomStepDeg = 3.0f;   // FOV 조절 속도

    float Yaw = 0.f;   // Yaw 회전값
    float Pitch = 0.f; // Pitch 회전값

    FVector TargetLocation = FVector::Zero();
    bool bHasTargetLocation = false;

    bool bRotating = false;
};
