#include "SphereComponent.h"

namespace Engine::Component
{
    EBasicMeshType USphereComponent::GetBasicMeshType() const { return EBasicMeshType::Sphere; }

    Geometry::FAABB USphereComponent::GetLocalAABB() const
    {
        return Geometry::FAABB(FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f));
    }

    REGISTER_CLASS(Engine::Component, USphereComponent);
} // namespace Engine::Component