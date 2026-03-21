#include "Core/CoreMinimal.h"
#include "SceneComponent.h"

namespace Engine::Component
{
    FMatrix USceneComponent::GetRelativeMatrix() { return {}; }

    void USceneComponent::DrawProperties() {}

    bool USceneComponent::IsSelected() const { return false; }

    void USceneComponent::Update(float DeltaTime) {}

    REGISTER_CLASS(Engine::Component, USceneComponent);
} // namespace Engine::Component
