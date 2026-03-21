#pragma once
#include "PrimitiveComponent.h"

namespace Engine::Component
{
    class ENGINE_API USphereComp : public UPrimitiveComponent
    {
        DECLARE_RTTI(USphereComp, UPrimitiveComponent)
    public:
        USphereComp() = default;
        ~USphereComp() override = default;

        void Update(float DeltaTime) override;
    };
} // namespace Engine::Component