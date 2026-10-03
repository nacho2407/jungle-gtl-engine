#pragma once

#include "Core/CoreMinimal.h"
#include "Engine/Geometry/Frustum.h"
#include "Engine/Geometry/Ray.h"

enum class EViewportProjectionType
{
    Perspective,
    Orthographic
};

class FViewportCamera
{
public:
    FViewportCamera() = default;
    ~FViewportCamera() = default;

    void SetLocation(const FVector& InLocation);
    void SetRotation(const FQuat& InRotation);
    void SetRotation(const FRotator& InRotation);

    const FVector& GetLocation() const { return Location; }
    const FQuat& GetRotation() const { return Rotation; }

    FVector GetForwardVector() const;
    FVector GetRightVector() const;
    FVector GetUpVector() const;
    FVector GetEffectiveForward() const;
    FVector GetEffectiveRight() const;
    FVector GetEffectiveUp() const;

    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    FMatrix GetViewProjectionMatrix() const;
    FRay DeprojectScreenToWorld(float ScreenX, float ScreenY, float ScreenWidth, float ScreenHeight) const;
    const FFrustum& GetFrustum() const;

    void SetProjectionType(EViewportProjectionType InType);
    EViewportProjectionType GetProjectionType() const { return ProjectionType; }
    bool IsOrthographic() const { return ProjectionType == EViewportProjectionType::Orthographic; }

    void SetFOV(float InFOV);
    void SetNearPlane(float InNear);
    void SetFarPlane(float InFar);
    void SetOrthoHeight(float InHeight);
    void SetLookAt(const FVector& Target);

    float GetFOV() const { return FOV; }
    float GetNearPlane() const { return NearPlane; }
    float GetFarPlane() const { return FarPlane; }
    float GetOrthoHeight() const { return OrthoHeight; }

    void OnResize(uint32 InWidth, uint32 InHeight);

    uint32 GetWidth() const { return Width; }
    uint32 GetHeight() const { return Height; }
    float GetAspectRatio() const { return AspectRatio; }

    void SetCustomLookDir(const FVector& InLookDir, const FVector& InViewUp);
    void ClearCustomLookDir();

    const FVector& GetViewUp() const { return ViewUp; }

private:
    void MarkViewDirty()
    {
        bIsViewDirty = true;
        bIsFrustumDirty = true;
    }

    void MarkProjectionDirty()
    {
        bIsProjectionDirty = true;
        bIsFrustumDirty = true;
    }

private:
    FVector Location = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;

    FVector ViewUp = FVector(0.f, 0.f, 1.f);
    bool bHasCustomLookDir = false;
    FVector CustomLookDir = FVector(1.f, 0.f, 0.f);

    EViewportProjectionType ProjectionType = EViewportProjectionType::Perspective;

    uint32 Width = 1920;
    uint32 Height = 1080;
    float AspectRatio = 16.0f / 9.0f;

    float FOV = 3.14159265358979f * 60.0f / 180.0f;
    float NearPlane = 0.1f;
    float FarPlane = 2000.0f;
    float OrthoHeight = 10.0f;

    mutable FMatrix CachedViewMatrix = FMatrix::Identity;
    mutable FMatrix CachedProjectionMatrix = FMatrix::Identity;
    mutable bool bIsViewDirty = true;
    mutable bool bIsProjectionDirty = true;

    mutable FFrustum CachedFrustum;
    mutable bool bIsFrustumDirty = true;
};

struct FCameraSnapshot
{
    FVector Location = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    float FOV = 3.14159265358979f * 90.0f / 180.0f;
    float NearPlane = 0.1f;
    float FarPlane = 2000.0f;
};
