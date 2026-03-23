#include "SceneComponent.h"

#include "Core/Geometry/Primitives/AABBUtility.h"

namespace Engine::Component
{
    void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
    {
        if (WorldTransform.GetLocation() == NewLocation)
        {
            return;
        }

        WorldTransform.SetLocation(NewLocation);
        OnTransformChanged();
    }

    void USceneComponent::SetRelativeRotation(const FQuat& NewRotation)
    {
        if (WorldTransform.GetRotation() == NewRotation)
        {
            return;
        }

        WorldTransform.SetRotation(NewRotation);
        OnTransformChanged();
    }

    void USceneComponent::SetRelativeRotation(const FRotator& NewRotation)
    {
        SetRelativeRotation(NewRotation.Quaternion());
    }

    void USceneComponent::SetRelativeScale3D(const FVector& NewScale)
    {
        if (WorldTransform.GetScale3D() == NewScale)
        {
            return;
        }

        WorldTransform.SetScale3D(NewScale);
        OnTransformChanged();
    }

    void USceneComponent::SetRelativeTransform(const FVector& NewTransform)
    {
        WorldTransform.SetScale3D(NewTransform);
        OnTransformChanged();
    }

    void USceneComponent::Update(float DeltaTime) {}

    FMatrix USceneComponent::GetRelativeMatrix() const
    {
        return WorldTransform.ToMatrixWithScale();
    }

    bool USceneComponent::IsSelected() const { return bIsSelected; }

} // namespace Engine::Component