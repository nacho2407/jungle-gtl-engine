#pragma once
#include "PrimitiveComponent.h"

namespace Engine::Component
{
    class ENGINE_API USphereComp : public UPrimitiveComponent
    {
        DECLARE_RTTI(USphereComp, UPrimitiveComponent)
    public:
        USphereComp();
        ~USphereComp() override;
        void Update(float DeltaTime) override;
    };
} // namespace Engine::Component