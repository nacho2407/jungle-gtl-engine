#include "Core/CoreMinimal.h"
#include "SceneComponent.h"

Engine::Component::USceneComponent::USceneComponent() = default;

Engine::Component::USceneComponent ::~USceneComponent() = default;

FMatrix Engine::Component::USceneComponent::GetRelativeMatrix()
{
	return {};
}

void Engine::Component::USceneComponent::DrawProperties()
{
}

bool Engine::Component::USceneComponent::IsSelected() const
{
	return {};
}

REGISTER_CLASS(Engine::Component, USceneComponent);
