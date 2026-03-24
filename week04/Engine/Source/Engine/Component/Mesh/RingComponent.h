#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class ENGINE_API URingComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(URingComponent, UPrimitiveComponent)
      public:
        URingComponent() = default;
        ~URingComponent() override = default;

        EBasicMeshType GetBasicMeshType() const override { return EBasicMeshType::Ring; }

        bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;

      protected:
        Geometry::FAABB GetLocalAABB() const override;
    };
} // namespace Engine::Component
