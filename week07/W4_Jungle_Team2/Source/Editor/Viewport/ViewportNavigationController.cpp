#include "Editor/Viewport/ViewportNavigationController.h"

#include <cmath>
#include <algorithm>
#include "Math/Utils.h"

void FViewportNavigationController::Tick(float DeltaTime)
{
	if (ViewportCamera == nullptr)
	{
		return;
	}

	if (bOrbiting)
	{
		return;
	}

	EnsureTargetLocationInitialized();

	const FVector CurrentLocation = ViewportCamera->GetLocation();
	const float LerpAlpha = MathUtil::Clamp(DeltaTime * LocationLerpSpeed, 0.0f, 1.0f);
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
		Pitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.0f, 1.0f)));
		Yaw = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
	}
}

void FViewportNavigationController::ModifyFOVorOrthoHeight(float Delta)
{
	if (ViewportCamera == nullptr || MathUtil::IsNearlyZero(Delta))
	{
		return;
	}

	if (ViewportCamera->GetProjectionType() == EViewportProjectionType::Perspective)
	{
		ModifyFOV(Delta);
	}
	else
	{
		ModifyOrthoHeight(Delta);
	}
}

void FViewportNavigationController::ModifyFOV(float DeltaFOV)
{
	if (ViewportCamera == nullptr || MathUtil::IsNearlyZero(DeltaFOV))
	{
		return;
	}

	constexpr float ZoomStepRad = 3.14159265358979f / 60.0f; // 3 deg
	const float Direction = (DeltaFOV > 0.f) ? 2.f : -2.f;

	float NewFOV = ViewportCamera->GetFOV() + Direction * ZoomStepRad;
	NewFOV = MathUtil::Clamp(NewFOV, MathUtil::DegreesToRadians(30.f), MathUtil::DegreesToRadians(120.f));
	ViewportCamera->SetFOV(NewFOV);
}

void FViewportNavigationController::ModifyOrthoHeight(float DeltaHeight)
{
	if (ViewportCamera == nullptr || MathUtil::IsNearlyZero(DeltaHeight))
	{
		return;
	}

	const float Direction = (DeltaHeight > 0.f) ? 1.f : -1.f;
	float NewHeight = ViewportCamera->GetOrthoHeight() + Direction * 5.f;
	NewHeight = MathUtil::Clamp(NewHeight, 1.0f, 1000.f);

	ViewportCamera->SetOrthoHeight(NewHeight);
}

void FViewportNavigationController::MoveForward(float Value, float DeltaTime)
{
	if (ViewportCamera == nullptr)
	{
		return;
	}

	if (bOrbiting)
	{
		return;
	}

	EnsureTargetLocationInitialized();
	TargetLocation += ViewportCamera->GetForwardVector() * (Value * MoveSpeed * DeltaTime);
}

void FViewportNavigationController::MoveRight(float Value, float DeltaTime)
{
	if (ViewportCamera == nullptr)
	{
		return;
	}

	if (bOrbiting)
	{
		return;
	}

	EnsureTargetLocationInitialized();
	TargetLocation += ViewportCamera->GetRightVector() * (Value * MoveSpeed * DeltaTime);
}

void FViewportNavigationController::MoveUp(float Value, float DeltaTime)
{
	if (ViewportCamera == nullptr || MathUtil::IsNearlyZero(Value))
	{
		return;
	}

	EnsureTargetLocationInitialized();
	TargetLocation += FVector(0.f, 0.f, 1.f) * (Value * MoveSpeed * DeltaTime);
}

void FViewportNavigationController::AddYawInput(float Value)
{
	if (ViewportCamera == nullptr || MathUtil::IsNearlyZero(Value))
	{
		return;
	}

	Yaw += Value * RotationSpeed;
	Yaw = FRotator::NormalizeAxis(Yaw);

	if (bOrbiting)
	{
		// UpdateOrbitCamera();
	}
	else
	{
		UpdateCameraRotation();
	}
}

void FViewportNavigationController::AddPitchInput(float Value)
{
	if (ViewportCamera == nullptr || MathUtil::IsNearlyZero(Value))
	{
		return;
	}

	Pitch += Value * RotationSpeed;
	Pitch = MathUtil::Clamp(Pitch, -89.f, 89.f);

	if (bOrbiting)
	{
		// UpdateOrbitCamera();
	}
	else
	{
		UpdateCameraRotation();
	}
}

// void FViewportNavigationController::BeginOrbit(const FVector& InPivot)
// {
// 	if (ViewportCamera == nullptr)
// 	{
// 		return;
// 	}
//
// 	if (ViewportCamera->GetProjectionType() != EViewportProjectionType::Perspective)
// 	{
// 		return;
// 	}
//
// 	OrbitPivot = InPivot;
//
// 	const FVector CameraLocation = ViewportCamera->GetLocation();
// 	const FVector Offset = CameraLocation - OrbitPivot;
//
// 	OrbitRadius = Offset.Size();
// 	if (MathUtil::IsNearlyZero(OrbitRadius))
// 	{
// 		OrbitRadius = DefaultOrbitRadius;
// 	}
//
// 	const FVector Forward = (-Offset).GetSafeNormal();
// 	Pitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.0f, 1.0f)));
// 	Yaw = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
//
// 	bOrbiting = true;
// }
//
// void FViewportNavigationController::UpdateOrbitCamera()
// {
// 	if (ViewportCamera == nullptr)
// 	{
// 		return;
// 	}
//
// 	if (ViewportCamera->GetProjectionType() != EViewportProjectionType::Perspective)
// 	{
// 		return;
// 	}
//
// 	const float PitchRad = MathUtil::DegreesToRadians(Pitch);
// 	const float YawRad = MathUtil::DegreesToRadians(Yaw);
//
// 	FVector ToPivotDir(
// 		std::cos(PitchRad) * std::cos(YawRad),
// 		std::cos(PitchRad) * std::sin(YawRad),
// 		std::sin(PitchRad));
//
// 	ToPivotDir = ToPivotDir.GetSafeNormal();
//
// 	const FVector CameraLocation = OrbitPivot - ToPivotDir * OrbitRadius;
// 	ViewportCamera->SetLocation(CameraLocation);
//
// 	TargetLocation = CameraLocation;
// 	bHasTargetLocation = true;
//
// 	const FVector Forward = (OrbitPivot - CameraLocation).GetSafeNormal();
//
// 	FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
// 	if (Right.IsNearlyZero())
// 	{
// 		return;
// 	}
//
// 	FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
//
// 	FMatrix RotationMatrix = FMatrix::Identity;
// 	RotationMatrix.SetAxes(Forward, Right, Up);
//
// 	FQuat NewRotation(RotationMatrix);
// 	NewRotation.Normalize();
// 	ViewportCamera->SetRotation(NewRotation);
// }
//
// void FViewportNavigationController::EndOrbit()
// {
// 	bOrbiting = false;
// }

void FViewportNavigationController::Dolly(float Value)
{
	if (ViewportCamera == nullptr || MathUtil::IsNearlyZero(Value))
	{
		return;
	}

	if (ViewportCamera->GetProjectionType() != EViewportProjectionType::Perspective)
	{
		return;
	}

	if (bOrbiting)
	{
		OrbitRadius -= Value * DollySpeed;
		OrbitRadius = std::max(OrbitRadius, MinOrbitRadius);
		// UpdateOrbitCamera();
		return;
	}

	EnsureTargetLocationInitialized();
	TargetLocation -= ViewportCamera->GetForwardVector().GetSafeNormal() * (Value * DollySpeed);
}

void FViewportNavigationController::BeginPanning()
{
	if (ViewportCamera == nullptr)
	{
		return;
	}

	EnsureTargetLocationInitialized();
	bPanning = true;
}

void FViewportNavigationController::EndPanning()
{
	bPanning = false;
}

void FViewportNavigationController::AddPanInput(float DeltaX, float DeltaY)
{
	if (ViewportCamera == nullptr)
	{
		return;
	}

	if (MathUtil::IsNearlyZero(DeltaX) && MathUtil::IsNearlyZero(DeltaY))
	{
		return;
	}

	EnsureTargetLocationInitialized();

	// 직교 뷰의 Custom LookDir 를 반영한 실제 화면 축 사용
	const FVector Right = ViewportCamera->GetEffectiveRight();
	const FVector Up    = ViewportCamera->GetEffectiveUp();

	const FVector PanDelta = (Right * DeltaX + Up * DeltaY) * PanSpeed;

	TargetLocation += PanDelta;

	if (bOrbiting)
	{
		OrbitPivot += PanDelta;
	}
}

void FViewportNavigationController::TranslateWithGizmoDelta(const FVector& Delta)
{
	if (ViewportCamera == nullptr)
	{
		return;
	}

	if (Delta.IsNearlyZero())
	{
		return;
	}

	EnsureTargetLocationInitialized();

	const FVector ScaledDelta = Delta * GizmoFollowSpeedScale;

	const FVector NewLocation = ViewportCamera->GetLocation() + ScaledDelta;
	ViewportCamera->SetLocation(NewLocation);
	TargetLocation = TargetLocation + ScaledDelta;
	bHasTargetLocation = true;

	if (bOrbiting)
	{
		OrbitPivot += ScaledDelta;
	}
}

void FViewportNavigationController::UpdateCameraRotation()
{
	if (ViewportCamera == nullptr)
	{
		return;
	}

	const float PitchRad = MathUtil::DegreesToRadians(Pitch);
	const float YawRad = MathUtil::DegreesToRadians(Yaw);

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