#include "Component/UCameraComponent.h"

UCameraComponent::UCameraComponent()
    : FieldOfView(90.0f)
    , AspectRatio(16.0f / 9.0f)
    , NearClip(0.1f)
    , FarClip(1000.0f)
{
}

UCameraComponent::~UCameraComponent()
{
}

void UCameraComponent::BeginPlay()
{
    USceneComponent::BeginPlay();
}

void UCameraComponent::TickComponent(float DeltaTime)
{
    USceneComponent::TickComponent(DeltaTime);
}

const char* UCameraComponent::GetObjClassName() const
{
    return "UCameraComponent";
}

void UCameraComponent::SetFieldOfView(float InFOV)
{
    FieldOfView = InFOV;
}

float UCameraComponent::GetFieldOfView() const
{
    return FieldOfView;
}

void UCameraComponent::SetAspectRatio(float InAspectRatio)
{
    AspectRatio = InAspectRatio;
}

float UCameraComponent::GetAspectRatio() const
{
    return AspectRatio;
}

void UCameraComponent::SetNearClip(float InNearClip)
{
    NearClip = InNearClip;
}

float UCameraComponent::GetNearClip() const
{
    return NearClip;
}

void UCameraComponent::SetFarClip(float InFarClip)
{
    FarClip = InFarClip;
}

float UCameraComponent::GetFarClip() const
{
    return FarClip;
}

void UCameraComponent::SetProjectionMode(EProjectionMode InMode)
{
    ProjectionMode = InMode;
}

EProjectionMode UCameraComponent::GetProjectionMode() const
{
    return ProjectionMode;
}

void UCameraComponent::SetOrthoWidth(float InOrthoWidth)
{
    if (InOrthoWidth > 0.001f)
    {
        OrthoWidth = InOrthoWidth;
    }
}

float UCameraComponent::GetOrthoWidth() const
{
    return OrthoWidth;
}

bool UCameraComponent::IsOrthogonal() const
{
    return ProjectionMode == EProjectionMode::Orthogonal;
}


FMatrix UCameraComponent::GetProjectionMatrix() const
{
    if (ProjectionMode == EProjectionMode::Orthogonal)
    {
        return FMatrix::MakeOrthogonalMatrix(
            OrthoWidth,
            AspectRatio,
            NearClip,
            FarClip);
    }

    return FMatrix::MakePerspectiveMatrix(
        FieldOfView,
        AspectRatio,
        NearClip,
        FarClip);
}

FMatrix UCameraComponent::GetViewMatrix() const
{
    return FMatrix::MakeViewMatrix(
        GetRelativeLocation(),
        GetRelativeRotation());
}

void UCameraComponent::UpdateAspectRatio(uint32 Width, uint32 Height)
{
    if (Height == 0)
    {
        return;
    }

    AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
}