#pragma once

#include "Engine/Component/Core/PrimitiveComponent.h"
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
        
        bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;
        
    protected:
        Geometry::FAABB GetLocalAABB() const override;
    };
} // namespace Engine::Component