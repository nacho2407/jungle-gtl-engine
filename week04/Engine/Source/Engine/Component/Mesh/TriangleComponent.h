#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

enum class EBasicMeshType : uint8;

namespace Engine::Component
{
    class ENGINE_API UTriangleComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(UTriangleComponent, UPrimitiveComponent)
      public:
        UTriangleComponent() = default;
        virtual ~UTriangleComponent() override = default;

        EBasicMeshType GetBasicMeshType() const override;

        bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;

      protected:
        Geometry::FAABB GetLocalAABB() const override;
    };
} // namespace Engine::Component
