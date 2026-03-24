#pragma once

#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Color.h"
#include "SceneComponent.h"

enum class EBasicMeshType : uint8;

namespace Engine::Component
{
    class ENGINE_API UPrimitiveComponent : public Engine::Component::USceneComponent
    {
        DECLARE_RTTI(UPrimitiveComponent, USceneComponent)
      public:
        UPrimitiveComponent() = default;
        ~UPrimitiveComponent() override = default;

        virtual EBasicMeshType GetBasicMeshType() const = 0;

        const FColor& GetColor() const;
        void            SetColor(const FColor& NewColor);

        const Geometry::FAABB& GetWorldAABB() const;

        virtual bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const;

        void Update(float DeltaTime) override;
        void DescribeProperties(FComponentPropertyBuilder& Builder) override;

      protected:
        virtual Geometry::FAABB GetLocalAABB() const = 0;
        virtual void            UpdateBounds();
        void                    OnTransformChanged() override;

      protected:
        FColor          Color = FColor::White();
        Geometry::FAABB WorldAABB;
        bool            bBoundsDirty = true;
    };
} // namespace Engine::Component
