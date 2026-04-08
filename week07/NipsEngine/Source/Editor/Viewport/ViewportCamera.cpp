#include "Editor/Viewport/ViewportCamera.h"

void FViewportCamera::SetLocation(const FVector& InLocation)
{
    Location = InLocation;
    MarkViewDirty();
}

void FViewportCamera::SetRotation(const FQuat& InRotation)
{
    Rotation = InRotation;
    Rotation.Normalize();
    MarkViewDirty();
}

void FViewportCamera::SetRotation(const FRotator& InRotation)
{
    Rotation = InRotation.Quaternion();
    Rotation.Normalize();
    MarkViewDirty();
}

FVector FViewportCamera::GetForwardVector() const { return Rotation.GetForwardVector(); }

FVector FViewportCamera::GetRightVector() const { return Rotation.GetRightVector(); }

FVector FViewportCamera::GetUpVector() const { return Rotation.GetUpVector(); }

FVector FViewportCamera::GetEffectiveForward() const
{
    if (bHasCustomLookDir)
    {
        return CustomLookDir;
    }
    return GetForwardVector();
}

FVector FViewportCamera::GetEffectiveRight() const
{
    if (bHasCustomLookDir)
    {
        // ViewMatrix 와 동일한 기준: Right = Cross(ViewUp, Forward)
        return FVector::CrossProduct(ViewUp, CustomLookDir).GetSafeNormal();
    }
    return GetRightVector();
}

FVector FViewportCamera::GetEffectiveUp() const
{
    if (bHasCustomLookDir)
    {
        return ViewUp;
    }
    return GetUpVector();
}

FMatrix FViewportCamera::GetViewMatrix() const
{
    if (bIsViewDirty)
    {
        const FVector Forward = bHasCustomLookDir ? CustomLookDir : GetForwardVector().GetSafeNormal();
        const FVector ActualUp = bHasCustomLookDir ? ViewUp : GetUpVector().GetSafeNormal();
        CachedViewMatrix = FMatrix::MakeViewLookAtLH(Location, Location + Forward, ActualUp);
        bIsViewDirty = false;
    }

    return CachedViewMatrix;
}

void FViewportCamera::SetCustomLookDir(const FVector& InLookDir, const FVector& InViewUp)
{
    CustomLookDir = InLookDir.GetSafeNormal();
    ViewUp = InViewUp.GetSafeNormal();
    bHasCustomLookDir = true;
    MarkViewDirty();
}

void FViewportCamera::ClearCustomLookDir()
{
    bHasCustomLookDir = false;
    ViewUp = FVector(0.f, 0.f, 1.f);
    MarkViewDirty();
}

FMatrix FViewportCamera::GetProjectionMatrix() const
{
    if (bIsProjectionDirty)
    {
        switch (ProjectionType)
        {
        case EViewportProjectionType::Perspective:
        {
            CachedProjectionMatrix = FMatrix::MakePerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
            break;
        }
        case EViewportProjectionType::Orthographic:
        {
            CachedProjectionMatrix =
                FMatrix::MakeOrthographicLH(OrthoHeight * AspectRatio, OrthoHeight, NearPlane, FarPlane);
            break;
        }
        default:
            break;
        }

        bIsProjectionDirty = false;
    }

    return CachedProjectionMatrix;
}

FMatrix FViewportCamera::GetViewProjectionMatrix() const { return GetViewMatrix() * GetProjectionMatrix(); }

const FFrustum& FViewportCamera::GetFrustum() const
{
    if (bIsFrustumDirty)
    {
        CachedFrustum.UpdateFromCamera(GetViewMatrix(), GetProjectionMatrix());
        bIsFrustumDirty = false;
    }

    return CachedFrustum;
}

FRay FViewportCamera::DeprojectScreenToWorld(float ScreenX, float ScreenY, float ScreenWidth, float ScreenHeight) const
{
    if (ScreenWidth <= 0.0f || ScreenHeight <= 0.0f)
    {
        return FRay{GetLocation(), GetForwardVector().GetSafeNormal()};
    }

    const float NdcX = (2.0f * ScreenX) / ScreenWidth - 1.0f;
    const float NdcY = 1.0f - (2.0f * ScreenY) / ScreenHeight;

    // The viewport camera uses the same conventional D3D clip-space depth
    // mapping as the rest of the engine: near = 0, far = 1.
    // A previous reverse-Z experiment left these endpoints flipped, which
    // built rays from the far plane back toward the camera and caused picking
    // to prefer the farthest object along the view direction.
    const FVector NdcNear(NdcX, NdcY, 0.0f);
    const FVector NdcFar(NdcX, NdcY, 1.0f);

    const FMatrix InverseViewProjection = GetViewProjectionMatrix().GetInverse();
    const FVector WorldNear = InverseViewProjection.TransformPosition(NdcNear);
    const FVector WorldFar = InverseViewProjection.TransformPosition(NdcFar);

    FVector Origin = WorldNear;
    FVector Direction = (WorldFar - WorldNear).GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        Direction = GetForwardVector().GetSafeNormal();
    }

    FRay Ray = FRay(Origin, Direction);

    return Ray;
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

void FViewportCamera::SetNearPlane(float InNear)
{
    NearPlane = InNear;
    MarkProjectionDirty();
}

void FViewportCamera::SetFarPlane(float InFar)
{
    FarPlane = InFar;
    MarkProjectionDirty();
}

void FViewportCamera::SetOrthoHeight(float InHeight)
{
    OrthoHeight = InHeight;
    MarkProjectionDirty();
}

void FViewportCamera::SetLookAt(const FVector& Target)
{
    FVector Forward = (Target - Location).GetSafeNormal();
    if (Forward.IsNearlyZero())
        return;

    FVector UpRef = FVector::UpVector;
    if (std::abs(Forward.DotProduct(UpRef)) > 0.99f)
    {
        UpRef = FVector(1.0f, 0.0f, 0.0f);
    }

    FVector Right = FVector::CrossProduct(UpRef, Forward).GetSafeNormal();
    FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();

    FMatrix RotMat = FMatrix::Identity;
    RotMat.SetAxes(Forward, Right, Up);

    FQuat QuatRot(RotMat);
    QuatRot.Normalize();
    SetRotation(QuatRot);
}

void FViewportCamera::OnResize(uint32 InWidth, uint32 InHeight)
{
    Width = InWidth;
    Height = (InHeight == 0) ? 1u : InHeight;
    AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    MarkProjectionDirty();
}
