#pragma once
#include "Camera/CameraShakeBase.h"

UCLASS()
class USinusoidalCameraShakePattern : public UCameraShakePattern
{
public:
	GENERATED_BODY(USinusoidalCameraShakePattern, UCameraShakePattern)

		// Location parameters
	UPROPERTY()
	FVector LocationAmplitude{ 0, 0, 0 }; // units

	UPROPERTY()
	FVector LocationFrequency{ 0, 0, 0 }; // Hz per axis

	UPROPERTY()
	FVector LocationPhase{ 0, 0, 0 };     // radians per axis

	// Rotation parameters (degrees)
	UPROPERTY()
	FVector RotationAmplitudeDeg{ 0, 0, 0 }; // degrees

	UPROPERTY()
	FVector RotationFrequency{ 0, 0, 0 };    // Hz per axis

	UPROPERTY()
	FVector RotationPhase{ 0, 0, 0 };        // radians per axis

	// FOV parameters (degrees)
	UPROPERTY()
	float FOVAmplitude = 0.0f; // degrees

	UPROPERTY()
	float FOVFrequency = 0.0f; // Hz

	UPROPERTY()
	float FOVPhase = 0.0f;     // radians

private:
	virtual void OnStartShakePattern(const FCameraShakeStartParams& Params) override; 
	virtual void OnUpdateShakePattern(
		const FCameraShakeUpdateParams& Params,
		FCameraShakeUpdateResult& OutResult) override;
};
