#include "ViewportNavigationController.h"
#include "Engine/Game/Actor.h"
#include "Viewport/Selection/ViewportSelectionController.h"

void FViewportNavigationController::Tick(float DeltaTime)
{
    if (ViewportCamera == nullptr)
    {
        return;
    }
    //  Orbiting 중에는 Translation Linear Interpolation을 하지 않습니다.
    if (bOrbiting)
    {
        return;
    }

    EnsureTargetLocationInitialized();

    const FVector CurrentLocation = ViewportCamera->GetLocation();
    const float   LerpAlpha = FMath::Clamp(DeltaTime * LocationLerpSpeed, 0.0f, 1.0f);
    const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
    ViewportCamera->SetLocation(NewLocation);
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
    //  Orbiting 중에는 이동 입력을 무시합니다.
    if (bOrbiting)
    {
        return;
    }

    EnsureTargetLocationInitialized();
    TargetLocation += ViewportCamera->GetForwardVector() * (Value * MoveSpeed * DeltaTime);

    // MessageBox(nullptr, L"MoveForward called", L"Debug", MB_OK);
}

void FViewportNavigationController::MoveRight(float Value, float DeltaTime)
{
    // if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
    if (ViewportCamera == nullptr)
    {
        return;
    }
    //  Orbiting 중에는 이동 입력을 무시합니다.
    if (bOrbiting)
    {
        return;
    }

    EnsureTargetLocationInitialized();
    TargetLocation += ViewportCamera->GetRightVector() * (Value * MoveSpeed * DeltaTime);
}

void FViewportNavigationController::MoveUp(float Value, float DeltaTime)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
        return;

    EnsureTargetLocationInitialized();
    TargetLocation += FVector(0.f, 0.f, 1.f) * (Value * MoveSpeed * DeltaTime);
}

void FViewportNavigationController::AddYawInput(float Value)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
    {
        return;
    }

    Yaw += Value * RotationSpeed;
    Yaw = FRotator::NormalizeAxis(Yaw);

    if (bOrbiting)
    {
        UpdateOrbitCamera();
        return;
    }
    else
    {
        UpdateCameraRotation();
    }
}

void FViewportNavigationController::AddPitchInput(float Value)
{
    if (ViewportCamera == nullptr || FMath::IsNearlyZero(Value))
    {
        return;
    }

    Pitch += Value * RotationSpeed;
    Pitch = FMath::Clamp(Pitch, -89.f, 89.f); // Pitch는 -89도에서 89도로 제한

    if (bOrbiting)
    {
        UpdateOrbitCamera();
        return;
    }
    else
    {
        UpdateCameraRotation();
    }
}

void FViewportNavigationController::BeginOrbit()
{
    if (ViewportCamera == nullptr)
    {
        return;
    }

    OrbitPivot = ResolveOrbitPivot();

    const FVector CameraLocation = ViewportCamera->GetLocation();
    const FVector Offset = CameraLocation - OrbitPivot;

    OrbitRadius = Offset.Size();
    if (FMath::IsNearlyZero(OrbitRadius))
    {
        OrbitRadius = DefaultOrbitRadius;
    }

    //  Orbit - CameraLocation
    const FVector Forward = (-Offset).GetSafeNormal();
    Pitch = FMath::RadiansToDegrees(std::asin(FMath::Clamp(Forward.Z, -1.0f, 1.0f)));
    Yaw = FMath::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));

    bOrbiting = true;
}

FVector FViewportNavigationController::ResolveOrbitPivot() const
{
    if (ViewportCamera == nullptr)
    {
        return FVector::Zero();
    }
    
    if (SelectionController != nullptr)
    {
        const auto& SelectedActors = SelectionController->GetSelectedActors();
        if (!SelectedActors.empty())
        {
            if (AActor* PrimaryActor = SelectedActors.back())
            {
                return PrimaryActor->GetWorldMatrix().GetOrigin();
            }
        }
    }
    
    const FVector RayOrigin = ViewportCamera->GetLocation();
    const FVector RayDirection = ViewportCamera->GetForwardVector().GetSafeNormal();
    
    if (!FMath::IsNearlyZero(RayDirection.Z))
    {
        const float T = -RayOrigin.Z / RayDirection.Z;
        if (T > 0.0f)
        {
            return RayOrigin + RayDirection * T;
        }
    }
    
    return RayOrigin + RayDirection * DefaultOrbitRadius;
}

void FViewportNavigationController::UpdateOrbitCamera()
{
    if (ViewportCamera == nullptr)
    {
        return;
    }

    const float PitchRad = FMath::DegreesToRadians(Pitch);
    const float YawRad = FMath::DegreesToRadians(Yaw);

    FVector ToPivotDir{std::cos(PitchRad) * std::cos(YawRad), std::cos(PitchRad) * std::sin(YawRad),
                       std::sin(PitchRad)};

    ToPivotDir = ToPivotDir.GetSafeNormal();

    const FVector CameraLocation = OrbitPivot - ToPivotDir * OrbitRadius;

    ViewportCamera->SetLocation(CameraLocation);
    TargetLocation = CameraLocation;
    bHasTargetLocation = true;

    const FVector Forward = (OrbitPivot - CameraLocation).GetSafeNormal();

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

void FViewportNavigationController::EndOrbit() { bOrbiting = false; }

void FViewportNavigationController::UpdateCameraRotation()
{
    if (ViewportCamera == nullptr)
    {
        return;
    }

    const float PitchRad = FMath::DegreesToRadians(Pitch);
    const float YawRad = FMath::DegreesToRadians(Yaw);

    FVector Forward(std::cos(PitchRad) * std::cos(YawRad), std::cos(PitchRad) * std::sin(YawRad),
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

void FViewportNavigationController::EnsureTargetLocationInitialized()
{
    if (ViewportCamera == nullptr)
    {
        return;
    }

    if (!bHasTargetLocation)
    {
        TargetLocation = ViewportCamera->GetLocation();
        bHasTargetLocation = true;
    }
}