#pragma once

#include "Core/CoreMinimal.h"

/*
        카메라/뷰 이동 계층
        이동 자체, Zoom, Focus Selected, Orbit, View Mode 전환의 일부 등을 함께 관리하는 계층입니다.
    ViewportInputContext에서 입력을 받아서 카메라 이동과 관련된 입력이 들어오면 이 계층에서
   처리하도록 합니다.

*/

class FViewportNavigationController
{
  public:
    void Tick(float DeltaTime);

    void MoveForward(float Value, float DeltaTime);
    void MoveRight(float Value, float DeltaTime);
    void MoveUp(float Value, float DeltaTime);

    void AddYawInput(float Value);
    void AddPitchInput(float Value);

    void SetRotating(bool bInRotating) { bRotating = bInRotating; }
    bool IsRotating() { return bRotating; }

    FVector GetPosition() const { return Position; }
    float   GetYaw() const { return Yaw; }
    float   GetPitch() const { return Pitch; }

  private:
    FVector Position = FVector::Zero();

    float Yaw = 0.f;
    float Pitch = 0.f;

    float MoveSpeed = 100.f;   // 이동 속도
    float RotationSpeed = 5.f; // 회전 속도 (degrees per second)

    bool bRotating = false;

  private:
    FVector GetForwardVector() const;
    FVector GetRightVector() const;
    FVector GetUpVector() const;
};
