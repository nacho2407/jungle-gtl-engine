#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    class ENGINE_API UConeComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(UConeComponent, UPrimitiveComponent)
      public:
        UConeComponent() = default;
        ~UConeComponent() override = default;

        EBasicMeshType GetBasicMeshType() const override { return EBasicMeshType::Cone; }

        bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;

      protected:
        Geometry::FAABB GetLocalAABB() const override;
    };
} // namespace Engine::Component
