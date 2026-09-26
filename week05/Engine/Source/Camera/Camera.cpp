#include "Camera.h"
#include <algorithm>
#include <cmath>
#include "Math/MathUtility.h"

void FCamera::SetPosition(const FVector& InPosition)
{
	Position = InPosition;
}

void FCamera::SetRotation(float InYaw, float InPitch)
{
	Yaw = InYaw;
	Pitch = InPitch;
}

FVector FCamera::GetForward() const
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

FVector FCamera::GetRight() const
{
	return FVector::CrossProduct(Up, GetForward()).GetSafeNormal();
}

void FCamera::MoveForward(float Delta)
{
	FVector Forward = GetForward();
	Position = Position + Forward * (Delta * Speed);
}

void FCamera::MoveRight(float Delta)
{
	FVector Right = GetRight();
	Position = Position + Right * (Delta * Speed);
}

void FCamera::MoveUp(float Delta)
{
	Position = Position + Up * (Delta * Speed);
}

void FCamera::Rotate(float DeltaYaw, float DeltaPitch)
{
	Yaw += DeltaYaw;
	Pitch += DeltaPitch;

	// Pitch 제한 (-89 ~ 89도)
	if (Pitch > 89.0f) Pitch = 89.0f;
	if (Pitch < -89.0f) Pitch = -89.0f;
}

FMatrix FCamera::GetViewMatrix() const
{
	FVector Target = Position + GetForward();
	return FMatrix::MakeViewLookAtLH(Position, Target, Up);
}

FMatrix FCamera::GetProjectionMatrix() const
{
	if (ProjectionMode == ECameraProjectionMode::Orthographic)
	{
		const float SafeViewWidth = (std::max)(OrthoWidth, 0.01f);
		const float SafeAspectRatio = (std::max)(AspectRatio, 0.01f);
		return FMatrix::MakeOrthographicLH(SafeViewWidth, SafeViewWidth / SafeAspectRatio, NearPlane, FarPlane);
	}

	return FMatrix::MakePerspectiveFovLH(FMath::DegreesToRadians(FOV), AspectRatio, NearPlane, FarPlane);
}

void FCamera::SetAspectRatio(float InAspectRatio)
{
	AspectRatio = (std::max)(InAspectRatio, 0.01f);
}

FVector FCamera::GetPosition() const
{
	return Position;
}

float FCamera::GetYaw() const
{
	return Yaw;
}

float FCamera::GetPitch() const
{
	return Pitch;
}

float FCamera::GetFOV() const
{
	return FOV;
}

void FCamera::SetFOV(float InFOV)
{
	FOV = std::clamp(InFOV, 1.0f, 179.0f);
}

ECameraProjectionMode FCamera::GetProjectionMode() const
{
	return ProjectionMode;
}

bool FCamera::IsOrthographic() const
{
	return ProjectionMode == ECameraProjectionMode::Orthographic;
}

void FCamera::SetProjectionMode(ECameraProjectionMode InProjectionMode)
{
	ProjectionMode = InProjectionMode;
}

float FCamera::GetOrthoWidth() const
{
	return OrthoWidth;
}

float FCamera::GetOrthoHeight() const
{
	return OrthoWidth / (std::max)(AspectRatio, 0.01f);
}

void FCamera::SetOrthoWidth(float InOrthoWidth)
{
	OrthoWidth = (std::max)(InOrthoWidth, 0.01f);
}
