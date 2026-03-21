#pragma once

#include "SceneComponent.h"

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
        UPrimitiveComponent();
        ~UPrimitiveComponent() override;

        EPrimitiveType GetType();
        void           SetType(EPrimitiveType NewType);
        void           Update(float DeltaTime) override;

    private:
        bool Pickable = true;
        EPrimitiveType Type;
    };
} // namespace Engine::Component
