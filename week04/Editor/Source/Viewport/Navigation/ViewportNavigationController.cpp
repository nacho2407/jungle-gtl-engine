#include "ViewportNavigationController.h"

void FViewportNavigationController::Tick(float DeltaTime)
{
    //  Tick에서 카메라 이동 처리
}

void FViewportNavigationController::SetRotating(bool bInRotating)
{
    if (bRotating == bInRotating)
    {
        return;
    }

    bRotating = bInRotating;

    if (bRotating && ViewportCamera != nullptr)
    {
        const FRotator CurrentRotator(ViewportCamera->GetRotation());
        Pitch = CurrentRotator.Pitch;
        Yaw = CurrentRotator.Yaw;
    }
}

void FViewportNavigationController::MoveForward(float Value, float DeltaTime)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
        return;

    FVector NewLocation = ViewportCamera->GetLocation();
    NewLocation += ViewportCamera->GetForwardVector() * (Value * MoveSpeed * DeltaTime);
    ViewportCamera->SetLocation(NewLocation);

    //MessageBox(nullptr, L"MoveForward called", L"Debug", MB_OK);
}

void FViewportNavigationController::MoveRight(float Value, float DeltaTime)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
        return;

    FVector NewLocation = ViewportCamera->GetLocation();
    NewLocation += ViewportCamera->GetRightVector() * (Value * MoveSpeed * DeltaTime);
    ViewportCamera->SetLocation(NewLocation);
}

void FViewportNavigationController::MoveUp(float Value, float DeltaTime)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
        return;

    FVector NewLocation = ViewportCamera->GetLocation();
    NewLocation += ViewportCamera->GetUpVector() * (Value * MoveSpeed * DeltaTime);
    ViewportCamera->SetLocation(NewLocation);
}

void FViewportNavigationController::AddYawInput(float Value)
{
    Yaw += Value * RotationSpeed;
    UpdateCameraRotation();
}

void FViewportNavigationController::AddPitchInput(float Value)
{
    Pitch += Value * RotationSpeed;
    Pitch = FMath::Clamp(Pitch, -89.f, 89.f); // Pitch는 -89도에서 89도로 제한
    UpdateCameraRotation();
}

void FViewportNavigationController::UpdateCameraRotation()
{
    if (ViewportCamera == nullptr)
    {
        return;
    }

    ViewportCamera->SetRotation(FRotator(Pitch, Yaw, 0.f));
}