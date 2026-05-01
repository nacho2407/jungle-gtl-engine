#include "Component/SpringArmComponent.h"

#include "Object/ObjectFactory.h"

#include <cmath>
#include <cstring>

DEFINE_CLASS(USpringArmComponent, USceneComponent)
REGISTER_FACTORY(USpringArmComponent)

USpringArmComponent::USpringArmComponent()
{
	SetComponentTickEnabled(true);
}

void USpringArmComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << "TargetArmLength" << TargetArmLength;
	Ar << "SocketOffset" << SocketOffset;
	Ar << "EnableCameraLag" << bEnableCameraLag;
	Ar << "CameraLagSpeed" << CameraLagSpeed;
}

void USpringArmComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Target Arm Length", EPropertyType::Float, &TargetArmLength, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "Socket Offset", EPropertyType::Vec3, &SocketOffset, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Enable Camera Lag", EPropertyType::Bool, &bEnableCameraLag });
	OutProps.push_back({ "Camera Lag Speed", EPropertyType::Float, &CameraLagSpeed, 0.01f, 100.0f, 0.1f });
}

void USpringArmComponent::PostEditProperty(const char* PropertyName)
{
	if (TargetArmLength < 0.0f)
	{
		TargetArmLength = 0.0f;
	}

	if (CameraLagSpeed < 0.01f)
	{
		CameraLagSpeed = 0.01f;
	}

	if (PropertyName == nullptr
		|| std::strcmp(PropertyName, "Target Arm Length") == 0
		|| std::strcmp(PropertyName, "Socket Offset") == 0
		|| std::strcmp(PropertyName, "Enable Camera Lag") == 0)
	{
		ResetCameraLag();
	}

	USceneComponent::PostEditProperty(PropertyName);
	UpdateSocketChildren();
}

void USpringArmComponent::UpdateWorldMatrix() const
{
	USceneComponent::UpdateWorldMatrix();
}

FVector USpringArmComponent::GetSocketLocalLocation() const
{
	return SocketOffset - FVector(TargetArmLength, 0.0f, 0.0f);
}

void USpringArmComponent::UpdateSocketChildren()
{
	FVector SocketLocalLocation = GetSocketLocalLocation();
	if (bEnableCameraLag && bLagLocationInitialized)
	{
		const FTransform ArmOriginTransform = GetWorldTransform();
		SocketLocalLocation = ArmOriginTransform.InverseTransformPosition(LagLocation);
	}

	for (USceneComponent* Child : ChildComponents)
	{
		if (Child)
		{
			Child->SetRelativeLocation(SocketLocalLocation);
		}
	}
}

void USpringArmComponent::AddYawInput(float DeltaYawDegrees)
{
	if (std::abs(DeltaYawDegrees) < 1e-6f)
	{
		return;
	}

	SetViewRotationDegrees(GetPitchDegrees(), GetYawDegrees() + DeltaYawDegrees);
}

void USpringArmComponent::AddPitchInput(float DeltaPitchDegrees)
{
	if (std::abs(DeltaPitchDegrees) < 1e-6f)
	{
		return;
	}

	SetViewRotationDegrees(GetPitchDegrees() + DeltaPitchDegrees, GetYawDegrees());
}

void USpringArmComponent::SetTargetArmLength(float InTargetArmLength)
{
	TargetArmLength = InTargetArmLength < 0.0f ? 0.0f : InTargetArmLength;
	ResetCameraLag();
	MarkTransformDirty();
	UpdateSocketChildren();
}

void USpringArmComponent::SetSocketOffset(const FVector& InSocketOffset)
{
	SocketOffset = InSocketOffset;
	ResetCameraLag();
	MarkTransformDirty();
	UpdateSocketChildren();
}

void USpringArmComponent::SetCameraLagEnabled(bool bEnabled)
{
	if (bEnableCameraLag != bEnabled)
	{
		bEnableCameraLag = bEnabled;
		ResetCameraLag();
		MarkTransformDirty();
		UpdateSocketChildren();
	}
}

void USpringArmComponent::SetCameraLagSpeed(float InCameraLagSpeed)
{
	CameraLagSpeed = InCameraLagSpeed < 0.01f ? 0.01f : InCameraLagSpeed;
}

void USpringArmComponent::TickComponent(float DeltaTime)
{
	if (!bEnableCameraLag)
	{
		bLagLocationInitialized = false;
		UpdateSocketChildren();
		return;
	}

	const FTransform DesiredTransform = CalculateDesiredSocketTransform();
	const FVector DesiredLocation = DesiredTransform.GetTranslation();

	if (!bLagLocationInitialized)
	{
		LagLocation = DesiredLocation;
		bLagLocationInitialized = true;
	}
	else
	{
		const float Alpha = MathUtil::Clamp(DeltaTime * CameraLagSpeed, 0.0f, 1.0f);
		LagLocation = FVector::Lerp(LagLocation, DesiredLocation, Alpha);
	}

	MarkTransformDirty();
	UpdateSocketChildren();
}

FTransform USpringArmComponent::CalculateDesiredSocketTransform() const
{
	const FTransform RelativeTransform = GetRelativeTransform();
	const FTransform ParentTransform = ParentComponent ? ParentComponent->GetWorldTransform() : FTransform::Identity;
	const FTransform ArmOriginTransform = RelativeTransform * ParentTransform;

	const FQuat ArmRotation = ArmOriginTransform.GetRotation();
	const FVector DesiredLocation = ArmOriginTransform.TransformPosition(GetSocketLocalLocation());

	return FTransform(ArmRotation, DesiredLocation, ArmOriginTransform.GetScale3D());
}

void USpringArmComponent::SetViewRotationDegrees(float PitchDegrees, float YawDegrees)
{
	PitchDegrees = MathUtil::Clamp(PitchDegrees, -89.0f, 89.0f);

	FVector RotationEuler = GetRelativeRotation();
	RotationEuler.X = 0.0f;
	RotationEuler.Y = PitchDegrees;
	RotationEuler.Z = YawDegrees;
	SetRelativeRotation(RotationEuler);
}

float USpringArmComponent::GetPitchDegrees() const
{
	return GetRelativeRotation().Y;
}

float USpringArmComponent::GetYawDegrees() const
{
	return GetRelativeRotation().Z;
}

void USpringArmComponent::ResetCameraLag()
{
	bLagLocationInitialized = false;
	LagLocation = FVector::ZeroVector;
}
