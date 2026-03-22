#include "ViewportNavigationController.h"

void FViewportNavigationController::Tick(float DeltaTime)
{
	//  Tick에서 카메라 이동 처리
}

void FViewportNavigationController::MoveForward(float Value, float DeltaTime)
{
	Position += GetForwardVector() * Value * MoveSpeed * DeltaTime;
}

void FViewportNavigationController::MoveRight(float Value, float DeltaTime)
{
	Position += GetRightVector() * Value * MoveSpeed * DeltaTime;
}

void FViewportNavigationController::MoveUp(float Value, float DeltaTime)
{
	Position += GetUpVector() * Value * MoveSpeed;
}

void FViewportNavigationController::AddYawInput(float Value)
{
	Yaw += Value * RotationSpeed;
}

void FViewportNavigationController::AddPitchInput(float Value)
{
	Pitch += Value * RotationSpeed; 
}

FVector FViewportNavigationController::GetForwardVector() const
{
	const float RadYaw = FMath::DegreesToRadians(Yaw);
	const float RadPitch = FMath::DegreesToRadians(Pitch);
	
	return FVector(
		std::cos(RadPitch) * std::cos(RadYaw),
		std::cos(RadPitch) * std::sin(RadYaw),
		std::sin(RadPitch)
	).GetSafeNormal();
}

FVector FViewportNavigationController::GetRightVector() const
{
    const FVector Forward = GetForwardVector();
    const FVector WorldUP = FVector::UpVector; // 월드 업 벡터
    return FVector::CrossProduct(WorldUP, Forward).GetSafeNormal();
}

FVector FViewportNavigationController::GetUpVector() const
{
	const FVector Forward = GetForwardVector();
	const FVector Right = GetRightVector();
	return FVector::CrossProduct(Forward, Right).GetSafeNormal();
}