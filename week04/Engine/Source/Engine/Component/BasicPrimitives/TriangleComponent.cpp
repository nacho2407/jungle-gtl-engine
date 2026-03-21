#include "TriangleComponent.h"

namespace Engine::Component
{
	void UTriangleComponent::Update(float DeltaTime)
	{
		UPrimitiveComponent::Update(DeltaTime);
	}

    EBasicMeshType UTriangleComponent::GetBasicMeshType() const { return EBasicMeshType::Triangle; }

    // REGISTER_CLASS(Engine::Component, UTriangleComponent);
} // namespace Engine::Component