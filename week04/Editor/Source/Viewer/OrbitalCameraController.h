#pragma once

#include "Core/CoreMinimal.h"
#include "Camera/ViewportCamera.h"

/**
 * Minimal orbital camera controller for the OBJ viewer.
 * Owns Yaw/Pitch/Radius/Pivot state and drives a FViewportCamera.
 */
class FOrbitalCameraController
{
public:
    void SetCamera(FViewportCamera* InCamera) { Camera = InCamera; }

    void SetRotationSpeed(float InSpeed) { RotationSpeed = FMath::Clamp(InSpeed, 0.01f, 10.0f); }
    void SetPanSpeed(float InSpeed)      { PanSpeed = FMath::Clamp(InSpeed, 0.001f, 10.0f); }
    void SetMinRadius(float InMin)       { MinOrbitRadius = std::max(InMin, 0.001f); }

    void  SetOrbitPivot(const FVector& InPivot)         { OrbitPivot = InPivot; }
    void  SetOrbitRadius(float InRadius)                { OrbitRadius = std::max(InRadius, MinOrbitRadius); }
    void  SetOrbitAngles(float InPitch, float InYaw)    { Pitch = InPitch; Yaw = InYaw; }
    float GetOrbitRadius() const                        { return OrbitRadius; }
    float GetOrbitPitch() const                         { return Pitch; }
    float GetOrbitYaw() const                           { return Yaw; }

    /** Accumulate yaw delta (pixels or normalized units) and reposition the camera. */
    void AddYawInput(float Value);

    /** Accumulate pitch delta and reposition the camera. */
    void AddPitchInput(float Value);

    /** Slide the orbit pivot along the camera's local right/up plane. */
    void AddPanInput(float DeltaX, float DeltaY);

    /** Move the camera closer/farther from the pivot. Positive Value = zoom in. */
    void Dolly(float Value);

    /** Smoothly interpolate camera rotation to the target angles over time. Alpha is [0, 1]. */
    void SlerpCamera(float TargetPitch, float TargetYaw, float Alpha);

    /** Recompute camera position and orientation from current orbit state. */
    void UpdateCamera();

private:
    FViewportCamera* Camera = nullptr;

    float   Yaw           = 0.f;   // degrees, horizontal orbit angle
    float   Pitch         = 0.f;   // degrees, vertical orbit angle, clamped [-89, 89]
    float   OrbitRadius   = 5.f;
    FVector OrbitPivot    = FVector::Zero();

    float Slerping = -1.f;

    float RotationSpeed  = 0.4f;
    float PanSpeed       = 0.1f;
    float MinOrbitRadius = 0.1f;
};
