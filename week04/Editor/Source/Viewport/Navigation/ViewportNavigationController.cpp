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
        const FVector Forward = ViewportCamera->GetForwardVector().GetSafeNormal();
        Pitch = FMath::RadiansToDegrees(std::asin(FMath::Clamp(Forward.Z, -1.0f, 1.0f)));
        Yaw = FMath::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
    }
}

void FViewportNavigationController::ModifyFOV(float DeltaFOV)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(DeltaFOV))
    {
        return;
    }

    constexpr float ZoomStepRad = FMath::DegreesToRadians(3.0f);

    float Direction = (DeltaFOV > 0.f) ? 2.f : -2.f;

    float NewFOV = ViewportCamera->GetFOV() + Direction * ZoomStepRad;
    NewFOV = FMath::Clamp(NewFOV, FMath::DegreesToRadians(30.f),
                          FMath::DegreesToRadians(120.f)); // FOV를 30도에서 120도로 제한
    ViewportCamera->SetFOV(NewFOV);
}

void FViewportNavigationController::MoveForward(float Value, float DeltaTime)
{
    // if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
    if (ViewportCamera == nullptr)
    {
        return;
    }

    FVector NewLocation = ViewportCamera->GetLocation();
    NewLocation += ViewportCamera->GetForwardVector() * (Value * MoveSpeed * DeltaTime);
    ViewportCamera->SetLocation(NewLocation);

    // MessageBox(nullptr, L"MoveForward called", L"Debug", MB_OK);
}

void FViewportNavigationController::MoveRight(float Value, float DeltaTime)
{
    // if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
    if (ViewportCamera == nullptr)
    {
        return;
    }

    FVector NewLocation = ViewportCamera->GetLocation();
    NewLocation += ViewportCamera->GetRightVector() * (Value * MoveSpeed * DeltaTime);
    ViewportCamera->SetLocation(NewLocation);
}

void FViewportNavigationController::MoveUp(float Value, float DeltaTime)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
        return;

    FVector NewLocation = ViewportCamera->GetLocation();
    NewLocation += FVector(0.f, 0.f, 1.f) * (Value * MoveSpeed * DeltaTime);
    ViewportCamera->SetLocation(NewLocation);
}

void FViewportNavigationController::AddYawInput(float Value)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
    {
        return;
    }

    Yaw += Value * RotationSpeed;
    Yaw = FRotator::NormalizeAxis(Yaw);

    UpdateCameraRotation();
}

void FViewportNavigationController::AddPitchInput(float Value)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
    {
        return;
    }

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

    const float PitchRad = FMath::DegreesToRadians(Pitch);
    const float YawRad = FMath::DegreesToRadians(Yaw);

    FVector Forward(
        std::cos(PitchRad) * std::cos(YawRad),
        std::cos(PitchRad) * std::sin(YawRad),
        std::sin(PitchRad));
    Forward = Forward.GetSafeNormal();

    FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    if (Right.IsNearlyZero())
    {
        return;
    }

    FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();

    FMatrix RotationMatrix = FMatrix::Identity;
    RotationMatrix.SetAxes(Forward, Right, Up);

    FQuat NewRotation(RotationMatrix);
    NewRotation.Normalize();
    ViewportCamera->SetRotation(NewRotation);
}