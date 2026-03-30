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
        const FVector Forward = GetForwardVector();
        CachedViewMatrix = FMatrix::MakeViewLookAtLH(Location, Location + Forward, GetUpVector());
        bIsViewDirty = false;
    }

    return CachedViewMatrix;
}

FMatrix FViewportCamera::GetProjectionMatrix() const
{
    if (bIsProjectionDirty)
    {
        // 투영행렬은 파라미터가 바뀔 때만 다시 계산하고, 그 외에는 캐시를 재사용합니다.
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
    // 창 크기가 바뀌면 aspect ratio가 바뀌므로 다음 프레임에 projection cache를 다시 만듭니다.
    MarkProjectionDirty();
}
