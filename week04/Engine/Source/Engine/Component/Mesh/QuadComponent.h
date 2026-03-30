#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Renderer/Types/BasicMeshType.h"

namespace Engine::Component
{
    /**
     * @brief 2D 평면(Quad)을 렌더링하기 위한 기본 컴포넌트입니다.
     * 스프라이트 및 텍스트 컴포넌트의 기반이 됩니다.
     */
    class ENGINE_API UQuadComponent : public UPrimitiveComponent
    {
        DECLARE_RTTI(UQuadComponent, UPrimitiveComponent)
      public:
        UQuadComponent() = default;
        virtual ~UQuadComponent() override = default;

        virtual EBasicMeshType GetBasicMeshType() const override { return EBasicMeshType::Quad; }

        virtual bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;

      protected:
        virtual Geometry::FAABB GetLocalAABB() const override;
    };
} // namespace Engine::Component
