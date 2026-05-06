#pragma once
#include "Camera/CameraShakeBase.h"

class USinusoidalCameraShakePattern : public UCameraShakePattern
{
public:
    DECLARE_CLASS(USinusoidalCameraShakePattern, UCameraShakePattern)

        // Location parameters
    FVector LocationAmplitude{ 0, 0, 0 }; // units
    FVector LocationFrequency{ 0, 0, 0 }; // Hz per axis
    FVector LocationPhase{ 0, 0, 0 };     // radians per axis

    // Rotation parameters (degrees)
    FVector RotationAmplitudeDeg{ 0, 0, 0 }; // degrees
    FVector RotationFrequency{ 0, 0, 0 };    // Hz per axis
    FVector RotationPhase{ 0, 0, 0 };        // radians per axis

    // FOV parameters (degrees)
    float FOVAmplitude = 0.0f; // degrees
    float FOVFrequency = 0.0f; // Hz
    float FOVPhase = 0.0f;     // radians

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
    virtual void OnStartShakePattern(const FCameraShakeStartParams& Params) override; 
    virtual void OnUpdateShakePattern(
        const FCameraShakeUpdateParams& Params,
        FCameraShakeUpdateResult& OutResult) override;
};
