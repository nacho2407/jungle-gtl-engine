#include "Component/CameraComponent.h"
#include <algorithm>
#include <cmath>

DEFINE_CLASS(UCameraComponent, USceneComponent)
REGISTER_FACTORY(UCameraComponent)

FMatrix UCameraComponent::GetViewMatrix() const
{
	UpdateWorldMatrix();

	const FTransform WorldTransform = GetWorldTransform();
	const FTransform ViewSource(
		WorldTransform.GetRotation(),
		WorldTransform.GetTranslation(),
		FVector::OneVector
	);
	return ViewSource.ToInverseMatrixWithScale();
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
	float Cot = 1.0f / tanf(FOV * 0.5f);
	float Denom = FarZ - NearZ;

	if (!bIsOrthogonal)
	{
		return FMatrix(
			Cot / AspectRatio, 0, 0, 0,
			0, Cot, 0, 0,
			0, 0, FarZ / Denom, 1,
			0, 0, -(FarZ * NearZ) / Denom, 0
		);
	}
	else
	{
		float HalfW = OrthoWidth * 0.5f;
		float HalfH = HalfW / AspectRatio;
		return FMatrix(
			1.0f / HalfW, 0, 0, 0,
			0, 1.0f / HalfH, 0, 0,
			0, 0, 1.0f / Denom, 0,
			0, 0, -NearZ / Denom, 1
		);
	}
}

void UCameraComponent::LookAt(const FVector& Target)
{
	const FVector Position = GetWorldLocation();
	const FVector ToTarget = Target - Position;
	const FVector Forward = ToTarget.GetSafeNormal();

	if (Forward.IsNearlyZero())
	{
		return;
	}

	// X-forward, Y-right, Z-up
	// Unreal-like meaning:
	//   Pitch = rotation around Y
	//   Yaw   = rotation around Z
	const float YawDegrees = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
	const float FlatLength = std::sqrt(Forward.X * Forward.X + Forward.Y * Forward.Y);
	const float PitchDegrees = MathUtil::RadiansToDegrees(std::atan2(Forward.Z, FlatLength));

	SetViewRotationDegrees(PitchDegrees, YawDegrees);
}

void UCameraComponent::OnResize(int32 Width, int32 Height)
{
	AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
}

const FCameraState& UCameraComponent::GetCameraState() const
{
	CameraStateCache.FOV = FOV;
	CameraStateCache.AspectRatio = AspectRatio;
	CameraStateCache.NearZ = NearZ;
	CameraStateCache.FarZ = FarZ;
	CameraStateCache.OrthoWidth = OrthoWidth;
	CameraStateCache.bIsOrthogonal = bIsOrthogonal;
	return CameraStateCache;
}

const FCameraPostProcessSettings& UCameraComponent::GetPostProcessSettings() const
{
	PostProcessSettingsCache.bVignetteEnabled = bVignetteEnabled;
	PostProcessSettingsCache.VignetteIntensity = VignetteIntensity;
	PostProcessSettingsCache.VignetteRadius = VignetteRadius;
	PostProcessSettingsCache.VignetteSmoothness = VignetteSmoothness;
	PostProcessSettingsCache.VignetteColor = VignetteColor;
	return PostProcessSettingsCache;
}

void UCameraComponent::SetCameraState(const FCameraState& NewState)
{
	FOV = NewState.FOV;
	AspectRatio = NewState.AspectRatio;
	NearZ = NewState.NearZ;
	FarZ = NewState.FarZ;
	OrthoWidth = NewState.OrthoWidth;
	bIsOrthogonal = NewState.bIsOrthogonal;
}

void UCameraComponent::SetVignette(float Intensity, float Radius, float Smoothness, const FColor& Color)
{
	VignetteIntensity = MathUtil::Clamp(Intensity, 0.0f, 1.0f);
	VignetteRadius = MathUtil::Clamp(Radius, 0.0f, 2.0f);
	VignetteSmoothness = std::max(Smoothness, 0.001f);
	VignetteColor = Color;
	bVignetteEnabled = VignetteIntensity > 0.001f;
}

void UCameraComponent::ClearVignette()
{
	bVignetteEnabled = false;
	VignetteIntensity = 0.0f;
	VignetteRadius = 0.75f;
	VignetteSmoothness = 0.35f;
	VignetteColor = FColor::Black();
}

FRay UCameraComponent::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight)
{
	float NdcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
	float NdcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

	FVector NdcNear(NdcX, NdcY, 0.0f);
	FVector NdcFar(NdcX, NdcY, 1.0f);

	FMatrix ViewProj = GetViewMatrix() * GetProjectionMatrix();
	FMatrix InverseViewProjection = ViewProj.GetInverse();

	FVector WorldNear = InverseViewProjection.TransformPosition(NdcNear);
	FVector WorldFar = InverseViewProjection.TransformPosition(NdcFar);

	FRay Ray;
	Ray.Origin = WorldNear;

	FVector Dir = WorldFar - WorldNear;
	float Length = std::sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y + Dir.Z * Dir.Z);
	Ray.Direction = (Length > 1e-4f) ? Dir / Length : FVector(1, 0, 0);

	return Ray;
}

void UCameraComponent::SetViewRotationDegrees(float PitchDegrees, float YawDegrees)
{
	PitchDegrees = MathUtil::Clamp(PitchDegrees, -89.0f, 89.0f);

	const float PitchRad = MathUtil::DegreesToRadians(PitchDegrees);
	const float YawRad = MathUtil::DegreesToRadians(YawDegrees);

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
	SetRelativeRotationQuat(NewRotation);
}

float UCameraComponent::GetPitchDegrees() const
{
	const FVector Forward = GetRelativeQuat().GetForwardVector().GetSafeNormal();
	return MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.0f, 1.0f)));
}

float UCameraComponent::GetYawDegrees() const
{
	const FVector Forward = GetRelativeQuat().GetForwardVector().GetSafeNormal();
	return MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
}

void UCameraComponent::AddYawInput(float DeltaYawDegrees)
{
	if (std::abs(DeltaYawDegrees) < 1e-6f)
	{
		return;
	}

	SetViewRotationDegrees(
		GetPitchDegrees(),
		GetYawDegrees() + DeltaYawDegrees
	);
}

void UCameraComponent::AddPitchInput(float DeltaPitchDegrees)
{
	if (std::abs(DeltaPitchDegrees) < 1e-6f)
	{
		return;
	}

	SetViewRotationDegrees(
		GetPitchDegrees() + DeltaPitchDegrees,
		GetYawDegrees()
	);
}

FVector UCameraComponent::GetForwardVector() const
{
	const float PitchRad = MathUtil::DegreesToRadians(GetPitchDegrees());
	const float YawRad = MathUtil::DegreesToRadians(GetYawDegrees());

	const float CosPitch = std::cos(PitchRad);
	const float SinPitch = std::sin(PitchRad);
	const float CosYaw = std::cos(YawRad);
	const float SinYaw = std::sin(YawRad);

	// X-forward, Y-right, Z-up
	FVector Forward(
		CosPitch * CosYaw,
		CosPitch * SinYaw,
		SinPitch
	);

	return Forward.GetSafeNormal();
}

FVector UCameraComponent::GetRightVector() const
{
	// Keep right vector level with world-up for editor-like behavior.
	// This prevents strange strafe tilt when looking up/down.
	const float YawRad = MathUtil::DegreesToRadians(GetYawDegrees());

	FVector Right(
		-std::sin(YawRad),
		std::cos(YawRad),
		0.0f
	);

	return Right.GetSafeNormal();
}

FVector UCameraComponent::GetUpVector() const
{
	return FVector(0.0f, 0.0f, 1.0f);
}

void UCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& OutView) const
{
	const FTransform& T = GetWorldTransform();

	OutView.Location = T.GetLocation();
	OutView.Rotation = T.GetRotation().GetNormalized();
	OutView.FOV = FOV;
	OutView.bIsOrthogonal = bIsOrthogonal;
	OutView.AspectRatio = AspectRatio;
	OutView.FarZ = FarZ;
	OutView.NearZ = NearZ;
	OutView.OrthoWidth = OrthoWidth;
	OutView.PostProcessSettings = GetPostProcessSettings();
}

void UCameraComponent::MoveForward(float Distance)
{
	if (std::abs(Distance) < 1e-6f)
	{
		return;
	}

	const FVector NewLocation = GetWorldLocation() + GetForwardVector() * Distance;
	SetWorldLocation(NewLocation);
}

void UCameraComponent::MoveRight(float Distance)
{
	if (std::abs(Distance) < 1e-6f)
	{
		return;
	}

	const FVector NewLocation = GetWorldLocation() + GetRightVector() * Distance;
	SetWorldLocation(NewLocation);
}

void UCameraComponent::MoveUp(float Distance)
{
	if (std::abs(Distance) < 1e-6f)
	{
		return;
	}

	const FVector NewLocation = GetWorldLocation() + GetUpVector() * Distance;
	SetWorldLocation(NewLocation);
}
