#include "CubeComponent.h"

namespace Engine::Component
{
    EBasicMeshType UCubeComponent::GetBasicMeshType() const { return EBasicMeshType::Cube; }

    Geometry::FAABB UCubeComponent::GetLocalAABB() const
    {
        return Geometry::FAABB(FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f));
    }

    REGISTER_CLASS(Engine::Component, UCubeComponent);
} // namespace Engine::Component