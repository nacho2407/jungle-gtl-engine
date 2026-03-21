#include "TriangleComponent.h"

namespace Engine::Component
{
    EBasicMeshType UTriangleComponent::GetBasicMeshType() const { return EBasicMeshType::Triangle; }

    // REGISTER_CLASS(Engine::Component, UTriangleComponent);
} // namespace Engine::Component