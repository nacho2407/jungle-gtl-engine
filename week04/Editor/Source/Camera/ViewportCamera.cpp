#include "ViewportCamera.h"

void FViewportCamera::SetLocation(const FVector& InLocation)
{
    Location = InLocation;
    MarkViewDirty();
}

void FViewportCamera::SetRotation(const FQuat& InRotation)
{
    Rotation = InRotation;
    MarkViewDirty();
}

void FViewportCamera::SetRotation(const FRotator& InRotation)
{
    Rotation = InRotation.Quaternion();
    MarkViewDirty();
}

FVector FViewportCamera::GetForwardVector() const { return Rotation.GetForwardVector(); }

FVector FViewportCamera::GetRightVector() const { return Rotation.GetRightVector(); }

FVector FViewportCamera::GetUpVector() const { return Rotation.GetUpVector(); }

FMatrix FViewportCamera::GetViewMatrix() const
{
    if (bIsViewDirty)
    {
        CachedViewMatrix = FMatrix::MakeViewLookAtLH(Location, GetForwardVector());
        bIsViewDirty = false;
    }

    return CachedViewMatrix;
}

FMatrix FViewportCamera::GetProjectionMatrix() const
{
    if (bIsProjectionDirty)
    {
        switch (ProjectionType)
        {
        case EViewportProjectionType::Perspective:
        {
            CachedProjectionMatrix =
                FMatrix::MakePerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
            break;
        }
        case EViewportProjectionType::Orthographic:
        {
            CachedProjectionMatrix = FMatrix::MakeOrthographicLH(OrthoHeight * AspectRatio,
                                                                 OrthoHeight, NearPlane, FarPlane);
            break;
        }
        default:
            break;
        }

        bIsProjectionDirty = false;
    }

    return CachedProjectionMatrix;
}

FMatrix FViewportCamera::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}

void FViewportCamera::SetProjectionType(EViewportProjectionType InType)
{
    ProjectionType = InType;
    MarkProjectionDirty();
}

void FViewportCamera::SetFOV(float InFOV)
{
    FOV = InFOV;
    MarkProjectionDirty();
}

void FViewportCamera::SetNearPlane(float InNearPlane)
{
    NearPlane = InNearPlane;
    MarkProjectionDirty();
}

void FViewportCamera::SetFarPlane(float InFarPlane)
{
    FarPlane = InFarPlane;
    MarkProjectionDirty();
}

void FViewportCamera::SetOrthoHeight(float InOrthoHeight)
{
    OrthoHeight = InOrthoHeight;
    MarkProjectionDirty();
}

void FViewportCamera::OnResize(uint32 InWidth, uint32 InHeight)
{
    Width = InWidth;
    Height = (InHeight == 0) ? 1 : InHeight;
    AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    MarkProjectionDirty();
}