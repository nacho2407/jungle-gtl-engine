#pragma once

#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Geometry/Primitives/AABBUtility.h"
#include "Core/Math/Vector4.h"
#include "SceneComponent.h"

enum class EBasicMeshType;

namespace Engine::Component
{
    class ENGINE_API UPrimitiveComponent : public Engine::Component::USceneComponent
    {
        DECLARE_RTTI(UPrimitiveComponent, USceneComponent)
      public:
        UPrimitiveComponent() = default;
        ~UPrimitiveComponent() override = default;

        virtual EBasicMeshType GetBasicMeshType() const = 0;

        const FVector4& GetColor() const;
        void            SetColor(const FVector4& NewColor);

        const Geometry::FAABB& GetAABB() const;

        void Update(float DeltaTime) override;

      protected:
        virtual Geometry::FAABB GetLocalAABB() const = 0;
        virtual void            UpdateBounds();
        void                    OnTransformChanged() override;

      protected:
        FVector4        Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        Geometry::FAABB WorldAABB;
        bool            bBoundsDirty = true;
    };
} // namespace Engine::Component