#include "Component/CameraComponent.h"
#include <cmath>

DEFINE_CLASS(UCameraComponent, USceneComponent)
REGISTER_FACTORY(UCameraComponent)

UCameraComponent* UCameraComponent::Duplicate()
{
	UCameraComponent* NewComp = UObjectManager::Get().CreateObject<UCameraComponent>();
    
    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr); 

    NewComp->SetRelativeLocation(this->GetRelativeLocation());
    NewComp->SetRelativeRotation(this->GetRelativeRotation());
    NewComp->SetRelativeScale(this->GetRelativeScale());

    NewComp->CameraState = this->CameraState;

    return NewComp;
}

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
	float Cot = 1.0f / tanf(CameraState.FOV * 0.5f);
	float Denom = CameraState.FarZ - CameraState.NearZ;

	if (!CameraState.bIsOrthogonal)
	{
		return FMatrix(
			Cot / CameraState.AspectRatio, 0, 0, 0,
			0, Cot, 0, 0,
			0, 0, CameraState.FarZ / Denom, 1,
			0, 0, -(CameraState.FarZ * CameraState.NearZ) / Denom, 0
		);
	}
	else
	{
		float HalfW = CameraState.OrthoWidth * 0.5f;
		float HalfH = HalfW / CameraState.AspectRatio;
		return FMatrix(
			1.0f / HalfW, 0, 0, 0,
			0, 1.0f / HalfH, 0, 0,
			0, 0, 1.0f / Denom, 0,
			0, 0, -CameraState.NearZ / Denom, 1
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
	CameraState.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
}

void UCameraComponent::SetCameraState(const FCameraState& NewState)
{
	CameraState = NewState;
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

void UCameraComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "FOV", EPropertyType::Float, &CameraState.FOV, 0.1f, 3.14f, 0.01f });
	OutProps.push_back({ "Near Z", EPropertyType::Float, &CameraState.NearZ, 0.01f, 100.0f, 0.01f });
	OutProps.push_back({ "Far Z", EPropertyType::Float, &CameraState.FarZ, 1.0f, 100000.0f, 10.0f });
	OutProps.push_back({ "Orthographic", EPropertyType::Bool, &CameraState.bIsOrthogonal });
	OutProps.push_back({ "Ortho Width", EPropertyType::Float, &CameraState.OrthoWidth, 0.1f, 1000.0f, 0.5f });
}

void UCameraComponent::SetViewRotationDegrees(float PitchDegrees, float YawDegrees)
{
	PitchDegrees = MathUtil::Clamp(PitchDegrees, -89.0f, 89.0f);

	// Current engine convention inferred from existing code:
	// RelativeRotation.Y = Pitch
	// RelativeRotation.Z = Yaw
	// RelativeRotation.X = Roll
	FVector RotationEuler = GetRelativeRotation();
	RotationEuler.X = 0.0f;          // Roll = 0
	RotationEuler.Y = PitchDegrees;  // Pitch
	RotationEuler.Z = YawDegrees;    // Yaw

	SetRelativeRotation(RotationEuler);
}

float UCameraComponent::GetPitchDegrees() const
{
	return GetRelativeRotation().Y;
}

float UCameraComponent::GetYawDegrees() const
{
	return GetRelativeRotation().Z;
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
	const float PitchRad = MathUtil::RadiansToDegrees(GetPitchDegrees());
	const float YawRad = MathUtil::RadiansToDegrees(GetYawDegrees());

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
	const float YawRad = MathUtil::RadiansToDegrees(GetYawDegrees());

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