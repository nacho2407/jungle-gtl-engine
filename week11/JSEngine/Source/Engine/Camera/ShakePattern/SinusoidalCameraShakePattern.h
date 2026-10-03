#pragma once
#include "Camera/CameraShakeBase.h"

#include "SinusoidalCameraShakePattern.generated.h"

UCLASS()
class USinusoidalCameraShakePattern : public UCameraShakePattern
{
    GENERATED_BODY_USinusoidalCameraShakePattern()
public:
    DECLARE_CLASS(USinusoidalCameraShakePattern, UCameraShakePattern)

        // Location parameters
    UPROPERTY(EditAnywhere)
    FVector LocationAmplitude{ 0, 0, 0 }; // units
    UPROPERTY(EditAnywhere)
    FVector LocationFrequency{ 0, 0, 0 }; // Hz per axis
    UPROPERTY(EditAnywhere)
    FVector LocationPhase{ 0, 0, 0 };     // radians per axis

    // Rotation parameters (degrees)
    UPROPERTY(EditAnywhere)
    FVector RotationAmplitudeDeg{ 0, 0, 0 }; // degrees
    UPROPERTY(EditAnywhere)
    FVector RotationFrequency{ 0, 0, 0 };    // Hz per axis
    UPROPERTY(EditAnywhere)
    FVector RotationPhase{ 0, 0, 0 };        // radians per axis

    // FOV parameters (degrees)
    UPROPERTY(EditAnywhere)
    float FOVAmplitude = 0.0f; // degrees
    UPROPERTY(EditAnywhere)
    float FOVFrequency = 0.0f; // Hz
    UPROPERTY(EditAnywhere)
    float FOVPhase = 0.0f;     // radians

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
    virtual void OnStartShakePattern(const FCameraShakeStartParams& Params) override; 
    virtual void OnUpdateShakePattern(
        const FCameraShakeUpdateParams& Params,
        FCameraShakeUpdateResult& OutResult) override;
};
