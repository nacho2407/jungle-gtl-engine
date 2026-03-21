#pragma once

#include "Engine/Component/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class ENGINE_API UPlaneComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(UPlaneComponent, UPrimitiveComponent)
      public:
        UPlaneComponent() = default;
        ~UPlaneComponent() override = default;

        EBasicMeshType GetBasicMeshType() const override { return EBasicMeshType::Plane; }

      protected:
          Geometry::FAABB GetLocalAABB() const override;
    };
} // namespace Engine::Component