#pragma once

#include "Component/SceneComponent.h"

UCLASS()
class USpringArmComponent : public USceneComponent
{
public:
	GENERATED_BODY(USpringArmComponent, USceneComponent)

	USpringArmComponent();

	void PostEditProperty(const char* PropertyName) override;

	void UpdateWorldMatrix() const override;
	FVector GetSocketLocalLocation() const;
	void UpdateSocketChildren();

	void AddYawInput(float DeltaYawDegrees);
	void AddPitchInput(float DeltaPitchDegrees);

	float GetTargetArmLength() const { return TargetArmLength; }
	void SetTargetArmLength(float InTargetArmLength);

	const FVector& GetSocketOffset() const { return SocketOffset; }
	void SetSocketOffset(const FVector& InSocketOffset);

	bool IsCameraLagEnabled() const { return bEnableCameraLag; }
	void SetCameraLagEnabled(bool bEnabled);

	float GetCameraLagSpeed() const { return CameraLagSpeed; }
	void SetCameraLagSpeed(float InCameraLagSpeed);

protected:
	void TickComponent(float DeltaTime) override;

private:
	void SetViewRotationDegrees(float PitchDegrees, float YawDegrees);
	float GetPitchDegrees() const;
	float GetYawDegrees() const;
	FTransform CalculateDesiredSocketTransform() const;
	void ResetCameraLag();

private:
	UPROPERTY(DisplayName = "Target Arm Length", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float TargetArmLength = 5.0f;

	UPROPERTY(DisplayName = "Socket Offset", Speed = 0.1f)
	FVector SocketOffset = FVector(0.0f, 0.0f, 0.25f);

	UPROPERTY(DisplayName = "Enable Camera Lag")
	bool bEnableCameraLag = false;

	UPROPERTY(DisplayName = "Camera Lag Speed", Min = 0.01f, Max = 100.0f, Speed = 0.1f)
	float CameraLagSpeed = 10.0f;

	mutable FVector LagLocation = FVector::ZeroVector;
	mutable bool bLagLocationInitialized = false;
};
