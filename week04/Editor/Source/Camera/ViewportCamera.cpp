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
        CachedViewMatrix = FMatrix::MakeViewLookAtLH(Location, Location + Forward);
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

Geometry::FRay FViewportCamera::ScreenPointToRay(const FVector2& MousePos)
{
    float    ndcX = (2.0f * (static_cast<float>(MousePos.X) + 0.5f)) / Width - 1.0f;
    float    ndcY = 1.0f - (2.0f * (static_cast<float>(MousePos.Y) + 0.5f)) / Height;
    FVector4 Near = FVector4(ndcX, ndcY, 0.f, 1.f);
    FVector4 Far = FVector4(ndcX, ndcY, 1.f, 1.f);

    FMatrix InvViewProjMat{GetViewProjectionMatrix()};
    (void)InvViewProjMat.Inverse();
    FVector4 NearWorld4 = Near * InvViewProjMat;
    FVector4 FarWorld4 = Far * InvViewProjMat;
    if (NearWorld4.W != 0.0f)
        NearWorld4 = NearWorld4 * (1.0f / NearWorld4.W);
    if (FarWorld4.W != 0.0f)
        FarWorld4 = FarWorld4 * (1.0f / FarWorld4.W);

    FVector  NearWorld = FVector(NearWorld4.X, NearWorld4.Y, NearWorld4.Z);
    FVector  FarWorld = FVector(FarWorld4.X, FarWorld4.Y, FarWorld4.Z);
    FVector  Direction = FarWorld - NearWorld;
    Direction.Normalize();

    return Geometry::FRay(NearWorld, Direction);
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