#include "SceneComponent.h"

namespace Engine::Component
{
    void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
    {
        WorldTransform.SetLocation(NewLocation);
        OnTransformChanged();
    }

    void USceneComponent::SetRelativeRotation(const FQuat& NewRotation)
    {
        WorldTransform.SetRotation(NewRotation);
        OnTransformChanged();
    }

    void USceneComponent::SetRelativeRotation(const FRotator& NewRotation)
    {
        WorldTransform.SetRotation(NewRotation);
        OnTransformChanged();
    }

    void USceneComponent::SetRelativeScale3D(const FVector& NewScale)
    {
        WorldTransform.SetScale3D(NewScale);
        OnTransformChanged();
    }

    void USceneComponent::Update(float DeltaTime) {}

    FMatrix USceneComponent::GetRelativeMatrix() const
    {
        return WorldTransform.ToMatrixWithScale();
    }

    void USceneComponent::DrawProperties() {}
} // namespace Engine::Component