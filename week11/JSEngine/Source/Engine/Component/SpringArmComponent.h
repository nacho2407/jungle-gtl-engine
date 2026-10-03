#pragma once

#include "SpringArmComponent.generated.h"

#include "Component/SceneComponent.h"

UCLASS()
class USpringArmComponent : public USceneComponent
{
    GENERATED_BODY_USpringArmComponent()
public:
	DECLARE_CLASS(USpringArmComponent, USceneComponent)

	USpringArmComponent();

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
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
	UPROPERTY(EditAnywhere)
	float TargetArmLength = 5.0f;
	UPROPERTY(EditAnywhere)
	FVector SocketOffset = FVector(0.0f, 0.0f, 0.25f);
	UPROPERTY(EditAnywhere)
	bool bEnableCameraLag = false;
	UPROPERTY(EditAnywhere)
	float CameraLagSpeed = 10.0f;

	mutable FVector LagLocation = FVector::ZeroVector;
	mutable bool bLagLocationInitialized = false;
};
