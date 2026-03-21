#include "Core/CoreMinimal.h"
#include "PrimitiveComponent.h"

Engine::Component::UPrimitiveComponent::UPrimitiveComponent() = default;

Engine::Component::UPrimitiveComponent ::~UPrimitiveComponent() = default;

Engine::Component::EPrimitiveType Engine::Component::UPrimitiveComponent::GetType()
{
	return {};
}

void Engine::Component::UPrimitiveComponent::SetType(EPrimitiveType NewType)
{
}

void Engine::Component::UPrimitiveComponent::Update(float DeltaTime)
{
	USceneComponent::Update(DeltaTime);
}

REGISTER_CLASS(Engine::Component, UPrimitiveComponent);
