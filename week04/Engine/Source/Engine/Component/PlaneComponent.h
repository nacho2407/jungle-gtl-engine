#pragma once

#include "PrimitiveComponent.h"

namespace Engine::Component
{
    class ENGINE_API UPlaneComp : public UPrimitiveComponent
    {
        DECLARE_RTTI(UPlaneComp, UPrimitiveComponent)
    public:
        UPlaneComp() = default;
        ~UPlaneComp() override = default;

        void Update(float DeltaTime) override;
    };
} // namespace Engine::Component
