#include "Core/CoreMinimal.h"
#include "CameraComponent.h"

namespace Engine::Component
{
    void UCameraComponent::SetRelativeLocation(const FVector& NewLocation)
    {
        WorldTransform.SetLocation(NewLocation);
        bIsViewDirty = true;
    }

    void UCameraComponent::SetRelativeRotation(const FQuat& NewRotation)
    {
        WorldTransform.SetRotation(NewRotation);
        bIsViewDirty = true;
    }

    void UCameraComponent::SetRelativeRotation(const FRotator& NewRotation)
    {
        WorldTransform.SetRotation(NewRotation);
        bIsViewDirty = true;
    }

    void UCameraComponent::Update(float InDeltaTime) {}

    FMatrix UCameraComponent::GetViewMatrix() const
    {
        if (bIsViewDirty)
        {
            CachedViewMatrix = FMatrix::MakeViewLookAtLH(
                WorldTransform.GetLocation(), WorldTransform.GetRotation().GetForwardVector());
            bIsViewDirty = false;
        }
        return CachedViewMatrix;
    }
    FMatrix UCameraComponent::GetProjectionMatrix() const
    {
        if (bIsProjectionDirty)
        {
            switch (ProjectionType)
            {
            case EProjectionType::Perspective:
            {
                CachedProjectionMatrix =
                    FMatrix::MakePerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
            }
            case EProjectionType::Orthographic:
            {
                CachedProjectionMatrix = FMatrix::MakeOrthographicLH(
                    OrthoHeight * AspectRatio, OrthoHeight, NearPlane, FarPlane);
            }
            }
            bIsProjectionDirty = false;
        }
        return CachedProjectionMatrix;
    }

    FMatrix UCameraComponent::GetViewProjectionMatrix() const
    {
        return GetViewMatrix() * GetProjectionMatrix();
    }

    void UCameraComponent::SetProjectionType(EProjectionType InType)
    {
        ProjectionType = InType;
        bIsProjectionDirty = true;
    }

    void UCameraComponent::SetFOV(float InFOV)
    {
        FOV = InFOV;
        bIsProjectionDirty = true;
    }

    void UCameraComponent::SetNearPlane(float InNear)
    {
        NearPlane = InNear;
        bIsProjectionDirty = true;
    }

    void UCameraComponent::SetFarPlane(float InFar)
    {
        FarPlane = InFar;
        bIsProjectionDirty = true;
    }

    void UCameraComponent::SetOrthoHeight(float InHeight)
    {
        OrthoHeight = InHeight;
        bIsProjectionDirty = true;
    }

    void UCameraComponent::OnResize(uint32 InWidth, uint32 InHeight)
    {
        Height = InHeight;
        Width = InWidth;
        AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
        bIsProjectionDirty = true;
    }

    FVector UCameraComponent::WorldToScreen(const FVector& InWorldPos) const { return FVector{}; }

    REGISTER_CLASS(Engine::Component, UCameraComponent)
} // namespace Engine::Component
