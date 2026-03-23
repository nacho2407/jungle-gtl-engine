#pragma once

#include "Engine/Component/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class ENGINE_API UQuadComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(UQuadComponent, UPrimitiveComponent)
      public:
        UQuadComponent() = default;
        ~UQuadComponent() override = default;

        EBasicMeshType GetBasicMeshType() const override { return EBasicMeshType::Quad; }

      protected:
        Geometry::FAABB GetLocalAABB() const override;
    };
} // namespace Engine::Component