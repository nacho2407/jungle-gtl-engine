#pragma once

#include "Engine/Component/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class ENGINE_API USphereComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(USphereComponent, UPrimitiveComponent)
      public:
        USphereComponent() = default;
        ~USphereComponent() override = default;

        EBasicMeshType GetBasicMeshType() const override { return EBasicMeshType::Sphere; }

      protected:
        Geometry::FAABB GetLocalAABB() const override;
    };
} // namespace Engine::Component