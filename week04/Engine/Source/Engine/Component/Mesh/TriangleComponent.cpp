#include "TriangleComponent.h"

namespace Engine::Component
{
    Geometry::FAABB UTriangleComponent::GetLocalAABB() const
    {
        return Geometry::FAABB(FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f));
    }

    EBasicMeshType UTriangleComponent::GetBasicMeshType() const { return EBasicMeshType::Triangle; }

    REGISTER_CLASS(Engine::Component, UTriangleComponent)
} // namespace Engine::Component
