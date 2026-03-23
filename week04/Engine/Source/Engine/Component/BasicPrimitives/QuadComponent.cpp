#include "QuadComponent.h"

namespace Engine::Component
{
    Geometry::FAABB UQuadComponent::GetLocalAABB() const
    {
        return Geometry::FAABB(FVector(-1.0f, 0.0f, -1.0f), FVector(1.0f, 0.0f, 1.0f));
    }

    REGISTER_CLASS(Engine::Component, UQuadComponent)
} // namespace Engine::Component