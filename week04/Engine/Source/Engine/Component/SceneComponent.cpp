#include "SceneComponent.h"

namespace Engine::Component
{
    void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
    {
        WorldTransform.SetLocation(NewLocation);
        OnTransformChanged();
    }

    bool USceneComponent::IsSelected() const { return false; }

    void USceneComponent::Update(float DeltaTime) {}

    FMatrix USceneComponent::GetRelativeMatrix() const
    {
        return WorldTransform.ToMatrixWithScale();
    }
} // namespace Engine::Component