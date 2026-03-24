#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class ENGINE_API UCylinderComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(UCylinderComponent, UPrimitiveComponent)
      public:
        UCylinderComponent() = default;
        ~UCylinderComponent() override = default;

        EBasicMeshType GetBasicMeshType() const override { return EBasicMeshType::Cylinder; }

        bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;

      protected:
        Geometry::FAABB GetLocalAABB() const override;
    };
} // namespace Engine::Component
