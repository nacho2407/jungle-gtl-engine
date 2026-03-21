#include "PlaneComponent.h"

namespace Engine::Component
{
    EBasicMeshType UPlaneComponent::GetBasicMeshType() const { return EBasicMeshType::Plane; }

    Geometry::FAABB UPlaneComponent::GetLocalAABB() const
    {
        return Geometry::FAABB(FVector(-1.0f, 0.0f, -1.0f), FVector(1.0f, 0.0f, 1.0f));
    }

    REGISTER_CLASS(Engine::Component, UPlaneComponent);
} // namespace Engine::Component