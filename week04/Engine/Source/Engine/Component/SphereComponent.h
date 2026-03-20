#pragma once
#include "PrimitiveComponent.h"

namespace Engine::Component
{
    class USphereComp : public UPrimitiveComponent
    {
        DECLARE_RTTI(USphereComp, UPrimitiveComponent)
      public:
        USphereComp();
        virtual ~USphereComp() override;
        virtual void Update(float DeltaTime) override;
    };
} // namespace Engine::Component