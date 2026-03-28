#include "Camera.h"
#include <algorithm>
#include <cmath>
#include "Math/MathUtility.h"

void CCamera::SetPosition(const FVector& InPosition)
{
	Position = InPosition;
}

void CCamera::SetRotation(float InYaw, float InPitch)
{
	Yaw = InYaw;
	Pitch = InPitch;
}

FVector CCamera::GetForward() const
{
	float RadYaw = FMath::DegreesToRadians(Yaw);
	float RadPitch = FMath::DegreesToRadians(Pitch);

	// 변경 (Z-up, 언리얼 방식)
	FVector Forward;
	Forward.X = cosf(RadPitch) * cosf(RadYaw);   // X가 Forward
	Forward.Y = cosf(RadPitch) * sinf(RadYaw);   // Y가 Right
	Forward.Z = sinf(RadPitch);                   // Z가 상하
	return Forward.GetSafeNormal();
}

FVector CCamera::GetRight() const
{
	return FVector::CrossProduct(Up, GetForward()).GetSafeNormal();
}

void CCamera::MoveForward(float Delta)
{
	FVector Forward = GetForward();
	Position = Position + Forward * (Delta * Speed);
}

void CCamera::MoveRight(float Delta)
{
	FVector Right = GetRight();
	Position = Position + Right * (Delta * Speed);
}

void CCamera::MoveUp(float Delta)
{
	Position = Position + Up * (Delta * Speed);
}

void CCamera::Rotate(float DeltaYaw, float DeltaPitch)
{
	Yaw += DeltaYaw;
	Pitch += DeltaPitch;

	// Pitch 제한 (-89 ~ 89도)
	if (Pitch > 89.0f) Pitch = 89.0f;
	if (Pitch < -89.0f) Pitch = -89.0f;
}

FMatrix CCamera::GetViewMatrix() const
{
	FVector Target = Position + GetForward();
	return FMatrix::MakeViewLookAtLH(Position, Target, Up);
}

FMatrix CCamera::GetProjectionMatrix() const
{
	if (ProjectionMode == ECameraProjectionMode::Orthographic)
	{
		const float SafeViewWidth = (std::max)(OrthoWidth, 0.01f);
		const float SafeAspectRatio = (std::max)(AspectRatio, 0.01f);
		return FMatrix::MakeOrthographicLH(SafeViewWidth, SafeViewWidth / SafeAspectRatio, NearPlane, FarPlane);
	}

	return FMatrix::MakePerspectiveFovLH(FMath::DegreesToRadians(FOV), AspectRatio, NearPlane, FarPlane);
}

void CCamera::SetAspectRatio(float InAspectRatio)
{
	AspectRatio = (std::max)(InAspectRatio, 0.01f);
}

FVector CCamera::GetPosition() const
{
	return Position;
}

float CCamera::GetYaw() const
{
	return Yaw;
}

float CCamera::GetPitch() const
{
	return Pitch;
}

float CCamera::GetFOV() const
{
	return FOV;
}

void CCamera::SetFOV(float InFOV)
{
	FOV = std::clamp(InFOV, 1.0f, 179.0f);
}

ECameraProjectionMode CCamera::GetProjectionMode() const
{
	return ProjectionMode;
}

bool CCamera::IsOrthographic() const
{
	return ProjectionMode == ECameraProjectionMode::Orthographic;
}

void CCamera::SetProjectionMode(ECameraProjectionMode InProjectionMode)
{
	ProjectionMode = InProjectionMode;
}

float CCamera::GetOrthoWidth() const
{
	return OrthoWidth;
}

float CCamera::GetOrthoHeight() const
{
	return OrthoWidth / (std::max)(AspectRatio, 0.01f);
}

void CCamera::SetOrthoWidth(float InOrthoWidth)
{
	OrthoWidth = (std::max)(InOrthoWidth, 0.01f);
}
