#pragma once

#include "Engine/Component/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class ENGINE_API UCubeComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(UCubeComponent, UPrimitiveComponent)
      public:
        UCubeComponent() = default;
        ~UCubeComponent() override = default;

        EBasicMeshType GetBasicMeshType() const override { return EBasicMeshType::Cube; }

      protected:
        Geometry::FAABB GetLocalAABB() const override
        {
            return Geometry::FAABB(FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f));
        }
    };
} // namespace Engine::Component