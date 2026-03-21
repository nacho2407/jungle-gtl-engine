#pragma once

#include "SceneComponent.h"
#include "Core/Math/Vector4.h"

namespace Engine::Component
{
    enum EPrimitiveType
    {
        Sphere,
        Cube,
        Triangle,
        Plane,
        Axis
    };

    class ENGINE_API UPrimitiveComponent : public Engine::Component::USceneComponent
    {
        DECLARE_RTTI(UPrimitiveComponent, USceneComponent)
      public:
        UPrimitiveComponent() = default;
        ~UPrimitiveComponent() override = default;

        EPrimitiveType  GetType();
        void            SetType(EPrimitiveType NewType);
        const FVector4& GetColor() const;
        void            SetColor(const FVector4& NewColor);
        void            Update(float DeltaTime) override;

      private:
        EPrimitiveType Type = Sphere;
        FVector4       Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    };
} // namespace Engine::Component
